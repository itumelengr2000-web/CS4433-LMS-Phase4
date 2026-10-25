#include "lms/academic_administration.hpp"

#include <sqlite3.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace lms {
namespace {
std::string timestamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

void text(sqlite3* db, sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

void integer(sqlite3* db, sqlite3_stmt* statement, int index, int value) {
    if (sqlite3_bind_int(statement, index, value) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

int insert(sqlite3* db, const char* sql,
           const std::initializer_list<std::string>& values,
           const std::initializer_list<int>& numbers) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    int index = 1;
    for (const auto& value : values) text(db, statement, index++, value);
    for (const auto value : numbers) integer(db, statement, index++, value);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        const std::string error = sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(statement);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

void update(sqlite3* db, const char* sql,
            const std::initializer_list<std::string>& values,
            const std::initializer_list<int>& numbers) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    int index = 1;
    for (const auto& value : values) text(db, statement, index++, value);
    for (const auto value : numbers) integer(db, statement, index++, value);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        const std::string error = sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        throw std::runtime_error(error);
    }
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(statement);
    if (changed == 0) throw std::runtime_error("record not found or unchanged");
}
} // namespace

int AcademicAdministrationService::create_department(const std::string& name) {
    if (name.empty()) throw std::invalid_argument("department name cannot be empty");
    return insert(database_.handle(), "INSERT INTO department (name) VALUES (?1)", {name}, {});
}

void AcademicAdministrationService::update_department(int department_id, const std::string& name) {
    if (department_id <= 0 || name.empty()) throw std::invalid_argument("invalid department update");
    update(database_.handle(), "UPDATE department SET name = ?1 WHERE department_id = ?2", {name}, {department_id});
}

int AcademicAdministrationService::create_course(const std::string& course_code,
                                                  const std::string& name,
                                                  int department_id) {
    if (course_code.empty() || name.empty() || department_id <= 0) {
        throw std::invalid_argument("invalid course data");
    }
    return insert(database_.handle(),
                  "INSERT INTO course (course_code, name, department_id) VALUES (?1, ?2, ?3)",
                  {course_code, name}, {department_id});
}

void AcademicAdministrationService::update_course(int course_id, const std::string& course_code,
                                                   const std::string& name, int department_id) {
    if (course_id <= 0 || course_code.empty() || name.empty() || department_id <= 0) {
        throw std::invalid_argument("invalid course update");
    }
    update(database_.handle(),
           "UPDATE course SET course_code = ?1, name = ?2, department_id = ?3 WHERE course_id = ?4",
           {course_code, name}, {department_id, course_id});
}

void AcademicAdministrationService::assign_lecturer(int course_id, int lecturer_id) {
    if (course_id <= 0 || lecturer_id <= 0) throw std::invalid_argument("invalid lecturer assignment");
    insert(database_.handle(),
           "INSERT INTO course_lecturer (course_id, lecturer_id, assigned_at) VALUES (?2, ?3, ?1)",
           {timestamp()}, {course_id, lecturer_id});
}

void AcademicAdministrationService::assign_hod_to_department(int department_id, int hod_user_id) {
    if (department_id <= 0 || hod_user_id <= 0) throw std::invalid_argument("invalid HoD department assignment");
    insert(database_.handle(),
           "INSERT INTO hod_department (department_id, hod_user_id) VALUES (?2, ?3)",
           {timestamp()}, {department_id, hod_user_id});
}

void AcademicAdministrationService::enrol_student(int course_id, int student_id) {
    if (course_id <= 0 || student_id <= 0) throw std::invalid_argument("invalid enrolment");
    insert(database_.handle(),
           "INSERT INTO enrolment (course_id, student_id, enrolled_at) VALUES (?2, ?3, ?1)",
           {timestamp()}, {course_id, student_id});
}

void AcademicAdministrationService::withdraw_student(int course_id, int student_id) {
    if (course_id <= 0 || student_id <= 0) throw std::invalid_argument("invalid withdrawal");
    update(database_.handle(),
           "UPDATE enrolment SET withdrawn_at = ?1 WHERE course_id = ?2 AND student_id = ?3 AND withdrawn_at IS NULL",
           {timestamp()}, {course_id, student_id});
}

} // namespace lms
