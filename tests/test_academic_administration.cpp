#include "lms/academic_administration.hpp"
#include "lms/database.hpp"

#include <gtest/gtest.h>

namespace {
const char* kSchema = R"SQL(
CREATE TABLE department (department_id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);
CREATE TABLE lecturer_profile (user_id INTEGER PRIMARY KEY);
CREATE TABLE student_profile (user_id INTEGER PRIMARY KEY);
CREATE TABLE course (
  course_id INTEGER PRIMARY KEY,
  course_code TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  department_id INTEGER NOT NULL REFERENCES department(department_id)
);
CREATE TABLE course_lecturer (
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  lecturer_id INTEGER NOT NULL REFERENCES lecturer_profile(user_id),
  assigned_at TEXT NOT NULL,
  PRIMARY KEY (course_id, lecturer_id)
);
CREATE TABLE enrolment (
  course_id INTEGER NOT NULL REFERENCES course(course_id),
  student_id INTEGER NOT NULL REFERENCES student_profile(user_id),
  enrolled_at TEXT NOT NULL,
  withdrawn_at TEXT,
  PRIMARY KEY (course_id, student_id)
);
INSERT INTO lecturer_profile(user_id) VALUES (10);
INSERT INTO student_profile(user_id) VALUES (20);
)SQL";
}

TEST(AcademicAdministration, CreatesAndUpdatesDepartmentAndCourse) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AcademicAdministrationService admin(database);

    const int department_id = admin.create_department("Computing");
    const int course_id = admin.create_course("CS4433", "Software Engineering", department_id);
    admin.update_department(department_id, "Mathematics and Computer Science");
    admin.update_course(course_id, "CS4433-F", "Software Engineering Final", department_id);

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT name FROM course WHERE course_id = ?1", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, course_id), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), "Software Engineering Final");
    sqlite3_finalize(statement);
}

TEST(AcademicAdministration, RejectsDuplicateCourseCode) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AcademicAdministrationService admin(database);
    const int department_id = admin.create_department("Computing");
    admin.create_course("CS4433", "Software Engineering", department_id);
    EXPECT_THROW(admin.create_course("CS4433", "Duplicate", department_id), std::exception);
}

TEST(AcademicAdministration, AssignsLecturerAndEnrolsThenWithdrawsStudent) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AcademicAdministrationService admin(database);
    const int department_id = admin.create_department("Computing");
    const int course_id = admin.create_course("CS4433", "Software Engineering", department_id);

    admin.assign_lecturer(course_id, 10);
    admin.enrol_student(course_id, 20);

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT COUNT(*) FROM course_lecturer WHERE course_id = ?1 AND lecturer_id = ?2", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, course_id), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 2, 10), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 1);
    sqlite3_finalize(statement);

    admin.withdraw_student(course_id, 20);
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT withdrawn_at IS NOT NULL FROM enrolment WHERE course_id = ?1 AND student_id = ?2", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, course_id), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 2, 20), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 1);
    sqlite3_finalize(statement);
}
