#include "lms/assignment_service.hpp"
#include "lms/database.hpp"

#include <gtest/gtest.h>

namespace {
const char* kSchema = R"SQL(
CREATE TABLE course_lecturer (course_id INTEGER, lecturer_id INTEGER, assigned_at TEXT, PRIMARY KEY(course_id, lecturer_id));
CREATE TABLE enrolment (course_id INTEGER, student_id INTEGER, enrolled_at TEXT, withdrawn_at TEXT, PRIMARY KEY(course_id, student_id));
CREATE TABLE assignment (
  assignment_id INTEGER PRIMARY KEY,
  course_id INTEGER NOT NULL,
  lecturer_id INTEGER NOT NULL,
  title TEXT NOT NULL,
  instructions TEXT NOT NULL,
  effective_due_at TEXT NOT NULL,
  maximum_mark REAL NOT NULL CHECK(maximum_mark > 0),
  active INTEGER NOT NULL CHECK(active IN (0,1))
);
CREATE TABLE extension (
  extension_id INTEGER PRIMARY KEY,
  assignment_id INTEGER NOT NULL,
  student_id INTEGER NOT NULL,
  original_due_at TEXT NOT NULL,
  new_due_at TEXT NOT NULL,
  reason TEXT NOT NULL,
  granted_by INTEGER NOT NULL,
  granted_at TEXT NOT NULL,
  UNIQUE(assignment_id, student_id)
);
INSERT INTO course_lecturer VALUES (5, 10, '2026-01-01T00:00:00Z');
INSERT INTO enrolment VALUES (5, 20, '2026-01-01T00:00:00Z', NULL);
INSERT INTO enrolment VALUES (5, 21, '2026-01-01T00:00:00Z', '2026-02-01T00:00:00Z');
)SQL";
}

TEST(AssignmentService, CreatesEditsAndDeactivatesAssignment) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AssignmentService service(database);

    const int id = service.create_assignment(10, 5, "Project", "Submit source code", "2026-11-07T23:59:59Z", 100.0);
    ASSERT_GT(id, 0);
    service.update_assignment(10, id, "Final Project", "Submit source and report", "2026-11-08T23:59:59Z", 80.0, true);
    service.set_assignment_active(10, id, false);

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT title, active FROM assignment WHERE assignment_id = ?1", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, id), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), "Final Project");
    EXPECT_EQ(sqlite3_column_int(statement, 1), 0);
    sqlite3_finalize(statement);
}

TEST(AssignmentService, RejectsUnassignedLecturerAndInvalidData) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AssignmentService service(database);

    EXPECT_THROW(service.create_assignment(99, 5, "Project", "Instructions", "2026-11-07T23:59:59Z", 100.0), std::runtime_error);
    EXPECT_THROW(service.create_assignment(10, 5, "", "Instructions", "2026-11-07T23:59:59Z", 100.0), std::invalid_argument);
    EXPECT_THROW(service.create_assignment(10, 5, "Project", "Instructions", "2026-11-07T23:59:59Z", 0.0), std::invalid_argument);
}

TEST(AssignmentService, GrantsOnlyValidIndividualExtension) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AssignmentService service(database);
    const int id = service.create_assignment(10, 5, "Project", "Instructions", "2026-11-07T23:59:59Z", 100.0);

    service.grant_individual_extension(10, id, 20, "2026-11-10T23:59:59Z", "Medical reason");
    EXPECT_THROW(service.grant_individual_extension(10, id, 20, "2026-1109T23:59:59Z", "Second extension"), std::exception);
    EXPECT_THROW(service.grant_individual_extension(10, id, 21, "2026-11-10T23:59:59Z", "Withdrawn student"), std::runtime_error);
    EXPECT_THROW(service.grant_individual_extension(99, id, 20, "2026-11-10T23:59:59Z", "Wrong lecturer"), std::runtime_error);
}
