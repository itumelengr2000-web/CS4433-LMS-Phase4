#include "lms/moderation_service.hpp"

#include <sqlite3.h>

#include <chrono>
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

void bind_int(sqlite3* db, sqlite3_stmt* statement, int index, int value) {
    if (sqlite3_bind_int(statement, index, value) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

void bind_text(sqlite3* db, sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

int scalar(sqlite3* db, const char* sql, int first, int second = 0, bool two = false) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, first);
    if (two) bind_int(db, statement, 2, second);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return 0;
    }
    const int value = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

int assignment_for_batch(sqlite3* db, int hod_user_id, int batch_id) {
    const char* sql =
        "SELECT a.assignment_id FROM assessment_batch b JOIN assignment a ON a.assignment_id = b.assignment_id "
        "JOIN course c ON c.course_id = a.course_id JOIN hod_department hd ON hd.department_id = c.department_id "
        "WHERE b.batch_id = ?1 AND hd.hod_user_id = ?2";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, batch_id);
    bind_int(db, statement, 2, hod_user_id);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        throw std::runtime_error("HoD cannot access this assessment batch");
    }
    const int assignment_id = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return assignment_id;
}

} // namespace

int ModerationService::open_batch(int hod_user_id, int assignment_id) {
    if (hod_user_id <= 0 || assignment_id <= 0) throw std::invalid_argument("invalid batch request");
    sqlite3* db = database_.handle();
    const char* scope_sql =
        "SELECT 1 FROM assignment a JOIN course c ON c.course_id = a.course_id "
        "JOIN hod_department hd ON hd.department_id = c.department_id "
        "WHERE a.assignment_id = ?1 AND hd.hod_user_id = ?2";
    sqlite3_stmt* scope = nullptr;
    if (sqlite3_prepare_v2(db, scope_sql, -1, &scope, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, scope, 1, assignment_id);
    bind_int(db, scope, 2, hod_user_id);
    if (sqlite3_step(scope) != SQLITE_ROW) {
        sqlite3_finalize(scope);
        throw std::runtime_error("HoD cannot access this assignment");
    }
    sqlite3_finalize(scope);

    const char* existing_sql = "SELECT batch_id FROM assessment_batch WHERE assignment_id = ?1";
    sqlite3_stmt* existing = nullptr;
    if (sqlite3_prepare_v2(db, existing_sql, -1, &existing, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, existing, 1, assignment_id);
    if (sqlite3_step(existing) == SQLITE_ROW) {
        const int id = sqlite3_column_int(existing, 0);
        sqlite3_finalize(existing);
        return id;
    }
    sqlite3_finalize(existing);

    const char* insert_sql = "INSERT INTO assessment_batch (assignment_id, status) VALUES (?1, 'OPEN')";
    sqlite3_stmt* insert = nullptr;
    if (sqlite3_prepare_v2(db, insert_sql, -1, &insert, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, insert, 1, assignment_id);
    if (sqlite3_step(insert) != SQLITE_DONE) {
        const std::string error = sqlite3_errmsg(db);
        sqlite3_finalize(insert);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(insert);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

std::vector<BatchSummary> ModerationService::batches_for_hod(int hod_user_id) const {
    if (hod_user_id <= 0) throw std::invalid_argument("HoD id must be positive");
    const char* sql =
        "SELECT b.batch_id, b.assignment_id, b.status FROM assessment_batch b "
        "JOIN assignment a ON a.assignment_id=b.assignment_id JOIN course c ON c.course_id=a.course_id "
        "JOIN hod_department hd ON hd.department_id=c.department_id WHERE hd.hod_user_id=?1 ORDER BY b.batch_id";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, hod_user_id);
    std::vector<BatchSummary> result;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        result.push_back({sqlite3_column_int(statement, 0), sqlite3_column_int(statement, 1), reinterpret_cast<const char*>(sqlite3_column_text(statement, 2))});
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<ModerationMarkSummary> ModerationService::marks_for_batch(int hod_user_id, int batch_id) const {
    sqlite3* db = database_.handle();
    const int assignment_id = assignment_for_batch(db, hod_user_id, batch_id);
    const char* sql =
        "SELECT m.mark_id, s.student_id, m.raw_mark, m.final_mark, COALESCE(md.decision, 'PENDING') "
        "FROM mark m JOIN submission s ON s.submission_id=m.submission_id "
        "LEFT JOIN moderation_decision md ON md.mark_id=m.mark_id AND md.batch_id=?1 "
        "WHERE s.assignment_id=?2 ORDER BY m.mark_id";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, batch_id); bind_int(db, statement, 2, assignment_id);
    std::vector<ModerationMarkSummary> result;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        result.push_back({sqlite3_column_int(statement, 0), sqlite3_column_int(statement, 1), sqlite3_column_double(statement, 2), sqlite3_column_double(statement, 3), reinterpret_cast<const char*>(sqlite3_column_text(statement, 4))});
    }
    sqlite3_finalize(statement);
    return result;
}

void ModerationService::approve_mark(int hod_user_id, int batch_id, int mark_id, const std::string& reason) {
    if (reason.empty()) throw std::invalid_argument("moderation reason cannot be empty");
    sqlite3* db = database_.handle();
    const int assignment_id = assignment_for_batch(db, hod_user_id, batch_id);
    const char* mark_sql = "SELECT 1 FROM mark m JOIN submission s ON s.submission_id = m.submission_id WHERE m.mark_id = ?1 AND s.assignment_id = ?2";
    if (scalar(db, mark_sql, mark_id, assignment_id, true) == 0) throw std::runtime_error("mark is not in this batch");
    database_.execute_script("BEGIN TRANSACTION;");
    try {
        const char* sql = "INSERT INTO moderation_decision (batch_id, mark_id, hod_user_id, decision, reason, decided_at) VALUES (?1, ?2, ?3, 'APPROVED', ?4, ?5) ON CONFLICT(batch_id, mark_id) DO UPDATE SET decision='APPROVED', reason=excluded.reason, decided_at=excluded.decided_at";
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        bind_int(db, statement, 1, batch_id); bind_int(db, statement, 2, mark_id); bind_int(db, statement, 3, hod_user_id); bind_text(db, statement, 4, reason); bind_text(db, statement, 5, utc_timestamp());
        if (sqlite3_step(statement) != SQLITE_DONE) { const std::string error = sqlite3_errmsg(db); sqlite3_finalize(statement); throw std::runtime_error(error); }
        sqlite3_finalize(statement);
        database_.execute_script("COMMIT;");
    } catch (...) { database_.execute_script("ROLLBACK;"); throw; }
}

void ModerationService::reject_mark(int hod_user_id, int batch_id, int mark_id, const std::string& reason) {
    if (reason.empty()) throw std::invalid_argument("moderation reason cannot be empty");
    sqlite3* db = database_.handle();
    const int assignment_id = assignment_for_batch(db, hod_user_id, batch_id);
    const char* mark_sql = "SELECT 1 FROM mark m JOIN submission s ON s.submission_id = m.submission_id WHERE m.mark_id = ?1 AND s.assignment_id = ?2";
    if (scalar(db, mark_sql, mark_id, assignment_id, true) == 0) throw std::runtime_error("mark is not in this batch");
    const char* sql = "INSERT INTO moderation_decision (batch_id, mark_id, hod_user_id, decision, reason, decided_at) VALUES (?1, ?2, ?3, 'REJECTED', ?4, ?5) ON CONFLICT(batch_id, mark_id) DO UPDATE SET decision='REJECTED', reason=excluded.reason, decided_at=excluded.decided_at";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, batch_id); bind_int(db, statement, 2, mark_id); bind_int(db, statement, 3, hod_user_id); bind_text(db, statement, 4, reason); bind_text(db, statement, 5, utc_timestamp());
    if (sqlite3_step(statement) != SQLITE_DONE) { const std::string error = sqlite3_errmsg(db); sqlite3_finalize(statement); throw std::runtime_error(error); }
    sqlite3_finalize(statement);
}

void ModerationService::release_batch(int hod_user_id, int batch_id) {
    sqlite3* db = database_.handle();
    const int assignment_id = assignment_for_batch(db, hod_user_id, batch_id);
    const char* count_sql = "SELECT (SELECT COUNT(*) FROM mark m JOIN submission s ON s.submission_id=m.submission_id WHERE s.assignment_id=?1), (SELECT COUNT(*) FROM enrolment e JOIN assignment a ON a.course_id=e.course_id WHERE a.assignment_id=?1 AND e.withdrawn_at IS NULL), (SELECT COUNT(*) FROM moderation_decision md JOIN mark m ON m.mark_id=md.mark_id JOIN submission s ON s.submission_id=m.submission_id WHERE md.batch_id=?2 AND s.assignment_id=?1 AND md.decision='APPROVED')";
    sqlite3_stmt* counts = nullptr;
    if (sqlite3_prepare_v2(db, count_sql, -1, &counts, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, counts, 1, assignment_id); bind_int(db, counts, 2, batch_id);
    if (sqlite3_step(counts) != SQLITE_ROW) { sqlite3_finalize(counts); throw std::runtime_error("cannot inspect batch"); }
    const int marks = sqlite3_column_int(counts, 0); const int students = sqlite3_column_int(counts, 1); const int approved = sqlite3_column_int(counts, 2);
    sqlite3_finalize(counts);
    if (marks == 0 || marks != students || approved != marks) throw std::runtime_error("complete approved batch is required before release");
    const char* sql = "UPDATE assessment_batch SET status='RELEASED', released_at=?1 WHERE batch_id=?2 AND status='OPEN'";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_text(db, statement, 1, utc_timestamp()); bind_int(db, statement, 2, batch_id);
    if (sqlite3_step(statement) != SQLITE_DONE) { const std::string error = sqlite3_errmsg(db); sqlite3_finalize(statement); throw std::runtime_error(error); }
    const int changed = sqlite3_changes(db); sqlite3_finalize(statement);
    if (changed == 0) throw std::runtime_error("batch is already released or unavailable");
}

std::optional<ReleasedMark> ModerationService::released_mark(int student_id, int assignment_id) const {
    const char* sql = "SELECT m.raw_mark, m.final_mark, m.feedback FROM mark m JOIN submission s ON s.submission_id=m.submission_id JOIN assessment_batch b ON b.assignment_id=s.assignment_id AND b.status='RELEASED' WHERE s.student_id=?1 AND s.assignment_id=?2";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, student_id); bind_int(db, statement, 2, assignment_id);
    if (sqlite3_step(statement) != SQLITE_ROW) { sqlite3_finalize(statement); return std::nullopt; }
    ReleasedMark result{sqlite3_column_double(statement, 0), sqlite3_column_double(statement, 1), reinterpret_cast<const char*>(sqlite3_column_text(statement, 2))};
    sqlite3_finalize(statement);
    return result;
}

} // namespace lms
