#include "lms/student_service.hpp"

#include <openssl/evp.h>
#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace lms {
namespace fs = std::filesystem;
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

std::string sha256_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open submission file");
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context || EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
        if (context) EVP_MD_CTX_free(context);
        throw std::runtime_error("cannot initialise SHA-256");
    }
    std::vector<char> buffer(8192);
    while (input.good()) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && EVP_DigestUpdate(context, buffer.data(), static_cast<std::size_t>(count)) != 1) {
            EVP_MD_CTX_free(context);
            throw std::runtime_error("cannot calculate checksum");
        }
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(context, digest, &digest_size) != 1) {
        EVP_MD_CTX_free(context);
        throw std::runtime_error("cannot finalise checksum");
    }
    EVP_MD_CTX_free(context);
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_size; ++i) result << std::setw(2) << static_cast<int>(digest[i]);
    return result.str();
}

} // namespace

std::vector<AssignmentSummary> StudentService::active_assignments(int student_id) const {
    if (student_id <= 0) throw std::invalid_argument("student id must be positive");
    const char* sql =
        "SELECT a.assignment_id, c.course_code, a.title, a.maximum_mark, "
        "COALESCE(e2.new_due_at, a.effective_due_at) "
        "FROM enrolment e "
        "JOIN course c ON c.course_id = e.course_id "
        "JOIN assignment a ON a.course_id = c.course_id AND a.active = 1 "
        "LEFT JOIN extension e2 ON e2.assignment_id = a.assignment_id AND e2.student_id = e.student_id "
        "WHERE e.student_id = ?1 AND e.withdrawn_at IS NULL ORDER BY a.assignment_id";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, student_id);
    std::vector<AssignmentSummary> result;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int(statement, 0),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)),
            sqlite3_column_double(statement, 3),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 4))
        });
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<StudentSubmissionSummary> StudentService::submissions_for_student(int student_id) const {
    if (student_id <= 0) throw std::invalid_argument("student id must be positive");
    const char* sql =
        "SELECT s.submission_id, s.assignment_id, a.title, v.version_no, v.received_at, v.is_late, s.status "
        "FROM submission s JOIN assignment a ON a.assignment_id = s.assignment_id "
        "JOIN submission_version v ON v.submission_id = s.submission_id "
        "WHERE s.student_id = ?1 ORDER BY s.assignment_id, v.version_no";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, statement, 1, student_id);
    std::vector<StudentSubmissionSummary> result;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int(statement, 0),
            sqlite3_column_int(statement, 1),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)),
            sqlite3_column_int(statement, 3),
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 4)),
            sqlite3_column_int(statement, 5) != 0,
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 6))
        });
    }
    sqlite3_finalize(statement);
    return result;
}

SubmissionReceipt StudentService::submit_assignment(int student_id, int assignment_id,
                                                     const std::string& source_file,
                                                     const std::string& received_at) {
    if (student_id <= 0 || assignment_id <= 0 || source_file.empty()) throw std::invalid_argument("invalid submission");
    const fs::path source(source_file);
    if (!fs::is_regular_file(source)) throw std::invalid_argument("submission file does not exist");
    sqlite3* db = database_.handle();
    const std::string receipt_time = received_at.empty() ? utc_timestamp() : received_at;

    const char* lookup_sql =
        "SELECT a.course_id, COALESCE(e2.new_due_at, a.effective_due_at) "
        "FROM assignment a JOIN enrolment e ON e.course_id = a.course_id AND e.student_id = ?1 AND e.withdrawn_at IS NULL "
        "LEFT JOIN extension e2 ON e2.assignment_id = a.assignment_id AND e2.student_id = ?1 "
        "WHERE a.assignment_id = ?2 AND a.active = 1";
    sqlite3_stmt* lookup = nullptr;
    if (sqlite3_prepare_v2(db, lookup_sql, -1, &lookup, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, lookup, 1, student_id);
    bind_int(db, lookup, 2, assignment_id);
    if (sqlite3_step(lookup) != SQLITE_ROW) {
        sqlite3_finalize(lookup);
        throw std::runtime_error("student cannot submit this assignment");
    }
    const auto* due_value = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 1));
    const std::string due_at = due_value ? due_value : "";
    sqlite3_finalize(lookup);
    const bool late = receipt_time > due_at;

    int submission_id = 0;
    int next_version = 1;
    const char* existing_sql = "SELECT submission_id, COALESCE((SELECT MAX(version_no) FROM submission_version WHERE submission_id = s.submission_id), 0) FROM submission s WHERE assignment_id = ?1 AND student_id = ?2";
    sqlite3_stmt* existing = nullptr;
    if (sqlite3_prepare_v2(db, existing_sql, -1, &existing, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, existing, 1, assignment_id);
    bind_int(db, existing, 2, student_id);
    if (sqlite3_step(existing) == SQLITE_ROW) {
        submission_id = sqlite3_column_int(existing, 0);
        next_version = sqlite3_column_int(existing, 1) + 1;
    }
    sqlite3_finalize(existing);

    if (submission_id == 0) {
        const char* insert_submission = "INSERT INTO submission (assignment_id, student_id, status) VALUES (?1, ?2, ?3)";
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, insert_submission, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        bind_int(db, statement, 1, assignment_id);
        bind_int(db, statement, 2, student_id);
        bind_text(db, statement, 3, late ? "LATE" : "SUBMITTED");
        if (sqlite3_step(statement) != SQLITE_DONE) {
            const std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(statement);
            throw std::runtime_error(error);
        }
        sqlite3_finalize(statement);
        submission_id = static_cast<int>(sqlite3_last_insert_rowid(db));
    } else {
        const char* update_submission = "UPDATE submission SET status = ?1 WHERE submission_id = ?2";
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, update_submission, -1, &statement, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        bind_text(db, statement, 1, late ? "LATE" : "SUBMITTED");
        bind_int(db, statement, 2, submission_id);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            const std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(statement);
            throw std::runtime_error(error);
        }
        sqlite3_finalize(statement);
    }

    const fs::path directory = fs::path("submission_files");
    fs::create_directories(directory);
    const fs::path destination = directory / ("student_" + std::to_string(student_id) + "_assignment_" + std::to_string(assignment_id) + "_v" + std::to_string(next_version) + source.extension().string());
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    const std::string checksum = sha256_file(destination);

    const char* insert_version = "INSERT INTO submission_version (submission_id, version_no, received_at, storage_path, checksum, is_late) VALUES (?1, ?2, ?3, ?4, ?5, ?6)";
    sqlite3_stmt* version = nullptr;
    if (sqlite3_prepare_v2(db, insert_version, -1, &version, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    bind_int(db, version, 1, submission_id);
    bind_int(db, version, 2, next_version);
    bind_text(db, version, 3, receipt_time);
    bind_text(db, version, 4, destination.string());
    bind_text(db, version, 5, checksum);
    bind_int(db, version, 6, late ? 1 : 0);
    if (sqlite3_step(version) != SQLITE_DONE) {
        const std::string error = sqlite3_errmsg(db);
        sqlite3_finalize(version);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(version);
    return {submission_id, next_version, receipt_time, destination.string(), checksum, late};
}

} // namespace lms
