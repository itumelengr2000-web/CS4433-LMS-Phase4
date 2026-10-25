#include "lms/database.hpp"
#include "lms/student_service.hpp"

#include <gtest/gtest.h>

#include <fstream>

namespace {
const char* kSchema = R"SQL(
CREATE TABLE course (course_id INTEGER PRIMARY KEY, course_code TEXT NOT NULL);
CREATE TABLE enrolment (course_id INTEGER, student_id INTEGER, enrolled_at TEXT, withdrawn_at TEXT, PRIMARY KEY(course_id, student_id));
CREATE TABLE assignment (assignment_id INTEGER PRIMARY KEY, course_id INTEGER, title TEXT, effective_due_at TEXT, maximum_mark REAL, active INTEGER);
CREATE TABLE extension (extension_id INTEGER PRIMARY KEY, assignment_id INTEGER, student_id INTEGER, original_due_at TEXT, new_due_at TEXT, reason TEXT, granted_by INTEGER, granted_at TEXT, UNIQUE(assignment_id, student_id));
CREATE TABLE submission (submission_id INTEGER PRIMARY KEY, assignment_id INTEGER, student_id INTEGER, status TEXT, UNIQUE(assignment_id, student_id));
CREATE TABLE submission_version (version_id INTEGER PRIMARY KEY, submission_id INTEGER, version_no INTEGER, received_at TEXT, storage_path TEXT, checksum TEXT, is_late INTEGER, UNIQUE(submission_id, version_no));
INSERT INTO course VALUES (1, 'CS4433');
INSERT INTO enrolment VALUES (1, 20, '2026-01-01T00:00:00Z', NULL);
INSERT INTO enrolment VALUES (1, 21, '2026-01-01T00:00:00Z', '2026-02-01T00:00:00Z');
INSERT INTO assignment VALUES (1, 1, 'Active Project', '2026-11-07T23:59:59Z', 100.0, 1);
INSERT INTO assignment VALUES (2, 1, 'Inactive Project', '2026-11-07T23:59:59Z', 100.0, 0);
INSERT INTO extension VALUES (1, 1, 20, '2026-11-07T23:59:59Z', '2026-11-10T23:59:59Z', 'Approved reason', 10, '2026-01-01T00:00:00Z');
)SQL";
}

TEST(StudentService, ShowsOnlyEnrolledActiveAssignmentsAndExtensionDeadline) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::StudentService service(database);
    const auto assignments = service.active_assignments(20);
    ASSERT_EQ(assignments.size(), 1U);
    EXPECT_EQ(assignments[0].assignment_id, 1);
    EXPECT_EQ(assignments[0].effective_due_at, "2026-11-10T23:59:59Z");
}

TEST(StudentService, StoresOnTimeLateAndResubmissionVersions) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::StudentService service(database);
    const std::string file = "/tmp/lms-student-submission.txt";
    { std::ofstream output(file); output << "version one"; }

    const auto first = service.submit_assignment(20, 1, file, "2026-11-09T12:00:00Z");
    EXPECT_EQ(first.version_no, 1);
    EXPECT_FALSE(first.is_late);
    EXPECT_EQ(first.checksum.size(), 64U);

    { std::ofstream output(file); output << "version two"; }
    const auto second = service.submit_assignment(20, 1, file, "2026-11-11T12:00:00Z");
    EXPECT_EQ(second.version_no, 2);
    EXPECT_TRUE(second.is_late);
    EXPECT_NE(first.storage_path, second.storage_path);
}

TEST(StudentService, RejectsNonEnrolledStudentAndMissingFile) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::StudentService service(database);
    EXPECT_THROW(service.submit_assignment(21, 1, "/tmp/does-not-exist.txt", "2026-11-09T12:00:00Z"), std::invalid_argument);
    EXPECT_THROW(service.submit_assignment(20, 1, "/tmp/does-not-exist.txt", "2026-11-09T12:00:00Z"), std::invalid_argument);
}
