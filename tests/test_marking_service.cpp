#include "lms/database.hpp"
#include "lms/late_penalty.hpp"
#include "lms/marking_service.hpp"

#include <gtest/gtest.h>

namespace {
const char* kSchema = R"SQL(
CREATE TABLE assignment (assignment_id INTEGER PRIMARY KEY, course_id INTEGER, lecturer_id INTEGER, effective_due_at TEXT, maximum_mark REAL);
CREATE TABLE extension (assignment_id INTEGER, student_id INTEGER, new_due_at TEXT, UNIQUE(assignment_id, student_id));
CREATE TABLE submission (submission_id INTEGER PRIMARY KEY, assignment_id INTEGER, student_id INTEGER, status TEXT);
CREATE TABLE submission_version (version_id INTEGER PRIMARY KEY, submission_id INTEGER, version_no INTEGER, received_at TEXT);
CREATE TABLE mark (mark_id INTEGER PRIMARY KEY, submission_id INTEGER UNIQUE, version_id INTEGER, raw_mark REAL, final_mark REAL, feedback TEXT, marked_by INTEGER, marked_at TEXT);
CREATE TABLE audit_log (audit_id INTEGER PRIMARY KEY, changed_by INTEGER, entity_type TEXT, entity_id INTEGER, action TEXT, old_value TEXT, new_value TEXT, reason TEXT, changed_at TEXT);
INSERT INTO assignment VALUES (1, 5, 10, '2026-11-07T00:00:00Z', 100.0);
INSERT INTO submission VALUES (1, 1, 20, 'LATE');
INSERT INTO submission_version VALUES (1, 1, 1, '2026-11-08T00:00:00Z');
)SQL";
}

TEST(MarkingService, CalculatesLatePenaltyAndStoresFeedbackAndAudit) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::PerDayLatePenaltyStrategy penalty;
    lms::MarkingService service(database, penalty);

    const auto result = service.mark_submission(10, 1, 1, 80.0, "Good work");
    EXPECT_EQ(result.late_days, 1);
    EXPECT_DOUBLE_EQ(result.final_mark, 70.0);
    EXPECT_EQ(result.feedback, "Good work");

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT COUNT(*) FROM audit_log WHERE entity_type = 'MARK' AND entity_id = ?1", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, result.mark_id), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 1);
    sqlite3_finalize(statement);

    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT changed_at, old_value, new_value, reason FROM audit_log WHERE entity_id = ?1", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, result.mark_id), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_STRNE(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), "2026-01-01T00:00:00Z");
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)), "70.00");
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement, 3)), "Lecturer marking");
    sqlite3_finalize(statement);
}

TEST(MarkingService, HandlesExactBoundaryAndUpdatesExistingMark) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    database.execute_script("UPDATE submission_version SET received_at = '2026-11-10T00:00:00Z';");
    lms::PerDayLatePenaltyStrategy penalty;
    lms::MarkingService service(database, penalty);

    const auto first = service.mark_submission(10, 1, 1, 80.0, "First feedback");
    EXPECT_EQ(first.late_days, 3);
    EXPECT_DOUBLE_EQ(first.final_mark, 50.0);
    const auto second = service.mark_submission(10, 1, 1, 90.0, "Updated feedback");
    EXPECT_EQ(second.mark_id, first.mark_id);
    EXPECT_DOUBLE_EQ(second.final_mark, 60.0);
}

TEST(MarkingService, RejectsWrongLecturerAndOutOfRangeMark) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::PerDayLatePenaltyStrategy penalty;
    lms::MarkingService service(database, penalty);

    EXPECT_THROW(service.mark_submission(99, 1, 1, 50.0, "Feedback"), std::runtime_error);
    EXPECT_THROW(service.mark_submission(10, 1, 1, 101.0, "Feedback"), std::invalid_argument);
    EXPECT_THROW(service.mark_submission(10, 1, 1, 50.0, ""), std::invalid_argument);
}

TEST(MarkingService, RollsBackMarkAndStatusWhenAuditWriteFails) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    database.execute_script("CREATE TRIGGER fail_audit BEFORE INSERT ON audit_log BEGIN SELECT RAISE(ABORT, 'audit unavailable'); END;");
    lms::PerDayLatePenaltyStrategy penalty;
    lms::MarkingService service(database, penalty);

    EXPECT_THROW(service.mark_submission(10, 1, 1, 80.0, "Should roll back"), std::runtime_error);

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT COUNT(*) FROM mark", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 0);
    sqlite3_finalize(statement);
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT status FROM submission WHERE submission_id = 1", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), "LATE");
    sqlite3_finalize(statement);
}
