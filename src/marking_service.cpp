#include "lms/marking_service.hpp"

#include <sqlite3.h>

#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace lms {
namespace {

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream result;
    result << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}

std::time_t parse_utc(const std::string& value) {
    std::tm parsed{};
    std::istringstream input(value);
    input >> std::get_time(&parsed, "%Y-%m-%dT%H:%M:%SZ");
    if (input.fail()) throw std::invalid_argument("timestamp must use UTC ISO-8601 format");
#if defined(_WIN32)
    return _mkgmtime(&parsed);
#else
    return timegm(&parsed);
#endif
}

int late_days(const std::string& received_at, const std::string& due_at) {
    const std::time_t received = parse_utc(received_at);
    const std::time_t due = parse_utc(due_at);
    const double seconds = std::difftime(received, due);
    if (seconds <= 0.0) return 0;
    return static_cast<int>(std::ceil(seconds / 86400.0));
}

void bind_text(sqlite3* db, sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

void bind_int(sqlite3* db, sqlite3_stmt* statement, int index, int value) {
    if (sqlite3_bind_int(statement, index, value) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

void bind_double(sqlite3* db, sqlite3_stmt* statement, int index, double value) {
    if (sqlite3_bind_double(statement, index, value) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

std::string mark_text(double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << value;
    return output.str();
}

} // namespace

MarkResult MarkingService::mark_submission(int lecturer_id, int submission_id, int version_id,
                                           double raw_mark, const std::string& feedback) {
    if (lecturer_id <= 0 || submission_id <= 0 || version_id <= 0 || feedback.empty()) {
        throw std::invalid_argument("invalid marking data");
    }
    sqlite3* db = database_.handle();
    const char* lookup_sql =
        "SELECT a.maximum_mark, a.lecturer_id, v.received_at, "
        "COALESCE(e.new_due_at, a.effective_due_at) "
        "FROM submission s JOIN assignment a ON a.assignment_id = s.assignment_id "
        "JOIN submission_version v ON v.submission_id = s.submission_id AND v.version_id = ?1 "
        "LEFT JOIN extension e ON e.assignment_id = a.assignment_id AND e.student_id = s.student_id "
        "WHERE s.submission_id = ?2";
    sqlite3_stmt* lookup = nullptr;
    if (sqlite3_prepare_v2(db, lookup_sql, -1, &lookup, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, lookup, 1, version_id);
    bind_int(db, lookup, 2, submission_id);
    if (sqlite3_step(lookup) != SQLITE_ROW) {
        sqlite3_finalize(lookup);
        throw std::runtime_error("submission version not found");
    }
    const double maximum_mark = sqlite3_column_double(lookup, 0);
    const int owner_lecturer = sqlite3_column_int(lookup, 1);
    const auto* received_value = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 2));
    const auto* due_value = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 3));
    const std::string received = received_value ? received_value : "";
    const std::string due = due_value ? due_value : "";
    sqlite3_finalize(lookup);
    if (owner_lecturer != lecturer_id) throw std::runtime_error("lecturer cannot mark this submission");
    if (raw_mark < 0.0 || raw_mark > maximum_mark) throw std::invalid_argument("mark is outside the allowed range");

    const int days = late_days(received, due);
    const double final_mark = penalty_.final_mark(raw_mark, maximum_mark, days);
    database_.execute_script("BEGIN TRANSACTION;");
    try {
        int mark_id = 0;
        double old_final = 0.0;
        bool existing = false;
        const char* existing_sql = "SELECT mark_id, final_mark FROM mark WHERE submission_id = ?1";
        sqlite3_stmt* existing_statement = nullptr;
        if (sqlite3_prepare_v2(db, existing_sql, -1, &existing_statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        bind_int(db, existing_statement, 1, submission_id);
        if (sqlite3_step(existing_statement) == SQLITE_ROW) {
            existing = true;
            mark_id = sqlite3_column_int(existing_statement, 0);
            old_final = sqlite3_column_double(existing_statement, 1);
        }
        sqlite3_finalize(existing_statement);

        sqlite3_stmt* statement = nullptr;
        if (existing) {
            const char* sql = "UPDATE mark SET version_id = ?1, raw_mark = ?2, final_mark = ?3, feedback = ?4, marked_by = ?5, marked_at = ?6 WHERE mark_id = ?7";
            if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
            bind_int(db, statement, 1, version_id);
            bind_double(db, statement, 2, raw_mark);
            bind_double(db, statement, 3, final_mark);
            bind_text(db, statement, 4, feedback);
            bind_int(db, statement, 5, lecturer_id);
            bind_text(db, statement, 6, utc_timestamp());
            bind_int(db, statement, 7, mark_id);
        } else {
            const char* sql = "INSERT INTO mark (submission_id, version_id, raw_mark, final_mark, feedback, marked_by, marked_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)";
            if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
            bind_int(db, statement, 1, submission_id);
            bind_int(db, statement, 2, version_id);
            bind_double(db, statement, 3, raw_mark);
            bind_double(db, statement, 4, final_mark);
            bind_text(db, statement, 5, feedback);
            bind_int(db, statement, 6, lecturer_id);
            bind_text(db, statement, 7, utc_timestamp());
        }
        if (sqlite3_step(statement) != SQLITE_DONE) {
            const std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(statement);
            throw std::runtime_error(error);
        }
        if (!existing) mark_id = static_cast<int>(sqlite3_last_insert_rowid(db));
        sqlite3_finalize(statement);

        const char* status_sql = "UPDATE submission SET status = 'MARKED' WHERE submission_id = ?1";
        if (sqlite3_prepare_v2(db, status_sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        bind_int(db, statement, 1, submission_id);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            const std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(statement);
            throw std::runtime_error(error);
        }
        sqlite3_finalize(statement);

        const char* audit_sql = "INSERT INTO audit_log (changed_by, entity_type, entity_id, action, old_value, new_value, reason, changed_at) VALUES (?1, 'MARK', ?2, ?3, ?4, ?5, ?6, ?7)";
        if (sqlite3_prepare_v2(db, audit_sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        bind_int(db, statement, 1, lecturer_id);
        bind_int(db, statement, 2, mark_id);
        bind_text(db, statement, 3, existing ? "MARK_UPDATED" : "MARK_CREATED");
        bind_text(db, statement, 4, existing ? mark_text(old_final) : "");
        bind_text(db, statement, 5, mark_text(final_mark));
        bind_text(db, statement, 6, "Lecturer marking");
        bind_text(db, statement, 7, utc_timestamp());
        if (sqlite3_step(statement) != SQLITE_DONE) {
            const std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(statement);
            throw std::runtime_error(error);
        }
        sqlite3_finalize(statement);
        database_.execute_script("COMMIT;");
        return {mark_id, raw_mark, final_mark, days, feedback};
    } catch (...) {
        database_.execute_script("ROLLBACK;");
        throw;
    }
}

std::vector<SubmissionSummary> MarkingService::submissions_for_lecturer(int lecturer_id) const {
    if (lecturer_id <= 0) throw std::invalid_argument("lecturer id must be positive");
    const char* sql =
        "SELECT s.submission_id, v.version_id, s.student_id, a.assignment_id, a.title, "
        "v.version_no, v.received_at, v.storage_path, v.is_late "
        "FROM submission s JOIN assignment a ON a.assignment_id = s.assignment_id "
        "JOIN submission_version v ON v.submission_id = s.submission_id "
        "WHERE a.lecturer_id = ?1 ORDER BY a.assignment_id, s.student_id, v.version_no";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, lecturer_id);
    std::vector<SubmissionSummary> result;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int(statement, 0),
            sqlite3_column_int(statement, 1),
            sqlite3_column_int(statement, 2),
            sqlite3_column_int(statement, 3),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 4)),
            sqlite3_column_int(statement, 5),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 6)),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 7)),
            sqlite3_column_int(statement, 8) != 0
        });
    }
    sqlite3_finalize(statement);
    return result;
}

} // namespace lms
