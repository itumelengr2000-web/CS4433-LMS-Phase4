#include "lms/database.hpp"
#include "lms/moderation_service.hpp"

#include <gtest/gtest.h>

namespace {
const char* kSchema = R"SQL(
CREATE TABLE department (department_id INTEGER PRIMARY KEY, name TEXT);
CREATE TABLE course (course_id INTEGER PRIMARY KEY, department_id INTEGER);
CREATE TABLE assignment (assignment_id INTEGER PRIMARY KEY, course_id INTEGER);
CREATE TABLE hod_department (hod_user_id INTEGER, department_id INTEGER, PRIMARY KEY(hod_user_id, department_id));
CREATE TABLE enrolment (course_id INTEGER, student_id INTEGER, withdrawn_at TEXT);
CREATE TABLE submission (submission_id INTEGER PRIMARY KEY, assignment_id INTEGER, student_id INTEGER);
CREATE TABLE mark (mark_id INTEGER PRIMARY KEY, submission_id INTEGER, raw_mark REAL, final_mark REAL, feedback TEXT);
CREATE TABLE assessment_batch (batch_id INTEGER PRIMARY KEY, assignment_id INTEGER UNIQUE, status TEXT, released_at TEXT);
CREATE TABLE moderation_decision (decision_id INTEGER PRIMARY KEY, batch_id INTEGER, mark_id INTEGER, hod_user_id INTEGER, decision TEXT, reason TEXT, decided_at TEXT, UNIQUE(batch_id, mark_id));
INSERT INTO department VALUES (1, 'Computing');
INSERT INTO department VALUES (2, 'Science');
INSERT INTO course VALUES (5, 1);
INSERT INTO course VALUES (6, 2);
INSERT INTO assignment VALUES (1, 5);
INSERT INTO assignment VALUES (2, 6);
INSERT INTO hod_department VALUES (100, 1);
INSERT INTO hod_department VALUES (200, 2);
INSERT INTO enrolment VALUES (5, 20, NULL);
INSERT INTO enrolment VALUES (5, 21, NULL);
INSERT INTO submission VALUES (1, 1, 20);
INSERT INTO submission VALUES (2, 1, 21);
INSERT INTO mark VALUES (11, 1, 80.0, 70.0, 'Good');
INSERT INTO mark VALUES (12, 2, 75.0, 65.0, 'Good');
)SQL";
}

TEST(ModerationService, BlocksReleaseUntilEveryMarkIsApproved) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::ModerationService service(database);
    const int batch = service.open_batch(100, 1);
    EXPECT_THROW(service.release_batch(100, batch), std::runtime_error);
    service.approve_mark(100, batch, 11, "Reviewed");
    EXPECT_THROW(service.release_batch(100, batch), std::runtime_error);
    service.approve_mark(100, batch, 12, "Reviewed");
    EXPECT_NO_THROW(service.release_batch(100, batch));
}

TEST(ModerationService, HodsAreLimitedToTheirDepartment) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::ModerationService service(database);
    EXPECT_THROW(service.open_batch(200, 1), std::runtime_error);
    const int batch = service.open_batch(100, 1);
    EXPECT_THROW(service.approve_mark(200, batch, 11, "Wrong department"), std::runtime_error);
}

TEST(ModerationService, RejectionPreventsReleaseUntilReapproved) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::ModerationService service(database);
    const int batch = service.open_batch(100, 1);
    service.reject_mark(100, batch, 11, "Fix feedback");
    service.approve_mark(100, batch, 12, "Reviewed");
    EXPECT_THROW(service.release_batch(100, batch), std::runtime_error);
    service.approve_mark(100, batch, 11, "Corrected and reviewed");
    EXPECT_NO_THROW(service.release_batch(100, batch));
}

TEST(ModerationService, StudentCannotSeeMarkBeforeReleaseButCanAfterRelease) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::ModerationService service(database);
    const int batch = service.open_batch(100, 1);
    EXPECT_FALSE(service.released_mark(20, 1).has_value());
    service.approve_mark(100, batch, 11, "Reviewed");
    service.approve_mark(100, batch, 12, "Reviewed");
    service.release_batch(100, batch);
    const auto result = service.released_mark(20, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->final_mark, 70.0);
    EXPECT_EQ(result->feedback, "Good");
}
