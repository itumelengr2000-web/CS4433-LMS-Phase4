#include "lms/assignment_service.hpp"

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

void require_lecturer_course(sqlite3* db, int lecturer_id, int course_id) {
    const char* sql = "SELECT 1 FROM course_lecturer WHERE course_id = ?1 AND lecturer_id = ?2";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_int(db, statement, 1, course_id);
    bind_int(db, statement, 2, lecturer_id);
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_ROW) throw std::runtime_error("lecturer is not assigned to this course");
}

void require_step(sqlite3* db, sqlite3_stmt* statement, const char* operation) {
    if (sqlite3_step(statement) != SQLITE_DONE) {
        const std::string error = std::string(operation) + ": " + sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        throw std::runtime_error(error);
    }
}

} // namespace

int AssignmentService::create_assignment(int lecturer_id, int course_id,
                                         const std::string& title,
                                         const std::string& instructions,
                                         const std::string& due_at,
                                         double maximum_mark, bool active) {
    if (lecturer_id <= 0 || course_id <= 0 || title.empty() || instructions.empty() ||
        due_at.empty() || maximum_mark <= 0.0) {
        throw std::invalid_argument("invalid assignment data");
    }
    sqlite3* db = database_.handle();
    require_lecturer_course(db, lecturer_id, course_id);
    const char* sql = "INSERT INTO assignment "
                      "(course_id, lecturer_id, title, instructions, effective_due_at, maximum_mark, active) "
                      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_int(db, statement, 1, course_id);
    bind_int(db, statement, 2, lecturer_id);
    bind_text(db, statement, 3, title);
    bind_text(db, statement, 4, instructions);
    bind_text(db, statement, 5, due_at);
    bind_double(db, statement, 6, maximum_mark);
    bind_int(db, statement, 7, active ? 1 : 0);
    require_step(db, statement, "assignment creation");
    sqlite3_finalize(statement);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

void AssignmentService::update_assignment(int lecturer_id, int assignment_id,
                                           const std::string& title,
                                           const std::string& instructions,
                                           const std::string& due_at,
                                           double maximum_mark, bool active) {
    if (assignment_id <= 0 || title.empty() || instructions.empty() || due_at.empty() ||
        maximum_mark <= 0.0) {
        throw std::invalid_argument("invalid assignment update");
    }
    sqlite3* db = database_.handle();
    const char* scope_sql = "SELECT course_id FROM assignment WHERE assignment_id = ?1 AND lecturer_id = ?2";
    sqlite3_stmt* scope = nullptr;
    if (sqlite3_prepare_v2(db, scope_sql, -1, &scope, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_int(db, scope, 1, assignment_id);
    bind_int(db, scope, 2, lecturer_id);
    if (sqlite3_step(scope) != SQLITE_ROW) {
        sqlite3_finalize(scope);
        throw std::runtime_error("lecturer cannot edit this assignment");
    }
    sqlite3_finalize(scope);

    const char* sql = "UPDATE assignment SET title = ?1, instructions = ?2, effective_due_at = ?3, "
                      "maximum_mark = ?4, active = ?5 WHERE assignment_id = ?6 AND lecturer_id = ?7";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_text(db, statement, 1, title);
    bind_text(db, statement, 2, instructions);
    bind_text(db, statement, 3, due_at);
    bind_double(db, statement, 4, maximum_mark);
    bind_int(db, statement, 5, active ? 1 : 0);
    bind_int(db, statement, 6, assignment_id);
    bind_int(db, statement, 7, lecturer_id);
    require_step(db, statement, "assignment update");
    sqlite3_finalize(statement);
}

void AssignmentService::set_assignment_active(int lecturer_id, int assignment_id, bool active) {
    if (assignment_id <= 0 || lecturer_id <= 0) throw std::invalid_argument("invalid assignment activation");
    const char* sql = "UPDATE assignment SET active = ?1 WHERE assignment_id = ?2 AND lecturer_id = ?3";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_int(db, statement, 1, active ? 1 : 0);
    bind_int(db, statement, 2, assignment_id);
    bind_int(db, statement, 3, lecturer_id);
    require_step(db, statement, "assignment activation");
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(statement);
    if (changed == 0) throw std::runtime_error("lecturer cannot change this assignment");
}

void AssignmentService::grant_individual_extension(int lecturer_id, int assignment_id,
                                                    int student_id,
                                                    const std::string& new_due_at,
                                                    const std::string& reason) {
    if (lecturer_id <= 0 || assignment_id <= 0 || student_id <= 0 || new_due_at.empty() || reason.empty()) {
        throw std::invalid_argument("invalid extension data");
    }
    sqlite3* db = database_.handle();
    const char* lookup_sql =
        "SELECT a.effective_due_at FROM assignment a "
        "JOIN course_lecturer cl ON cl.course_id = a.course_id AND cl.lecturer_id = ?1 "
        "JOIN enrolment e ON e.course_id = a.course_id AND e.student_id = ?2 AND e.withdrawn_at IS NULL "
        "WHERE a.assignment_id = ?3 AND a.lecturer_id = ?1";
    sqlite3_stmt* lookup = nullptr;
    if (sqlite3_prepare_v2(db, lookup_sql, -1, &lookup, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_int(db, lookup, 1, lecturer_id);
    bind_int(db, lookup, 2, student_id);
    bind_int(db, lookup, 3, assignment_id);
    if (sqlite3_step(lookup) != SQLITE_ROW) {
        sqlite3_finalize(lookup);
        throw std::runtime_error("lecturer cannot extend this assignment for this student");
    }
    const auto* original_value = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 0));
    const std::string original_due = original_value ? original_value : "";
    sqlite3_finalize(lookup);
    if (new_due_at <= original_due) throw std::invalid_argument("extension deadline must be later than the original deadline");

    const char* sql = "INSERT INTO extension "
                      "(assignment_id, student_id, original_due_at, new_due_at, reason, granted_by, granted_at) "
                      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_int(db, statement, 1, assignment_id);
    bind_int(db, statement, 2, student_id);
    bind_text(db, statement, 3, original_due);
    bind_text(db, statement, 4, new_due_at);
    bind_text(db, statement, 5, reason);
    bind_int(db, statement, 6, lecturer_id);
    bind_text(db, statement, 7, utc_timestamp());
    require_step(db, statement, "extension creation");
    sqlite3_finalize(statement);
}

} // namespace lms
