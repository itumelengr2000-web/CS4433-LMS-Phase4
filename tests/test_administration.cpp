#include "lms/administration.hpp"
#include "lms/authentication.hpp"
#include "lms/database.hpp"
#include "lms/user_repository.hpp"

#include <gtest/gtest.h>

namespace {
const char* kSchema = R"SQL(
CREATE TABLE user_account (
  user_id INTEGER PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('STUDENT','LECTURER','HOD','ADMIN')),
  active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
  created_at TEXT NOT NULL
);
)SQL";
}

TEST(Administration, CreatesAccountWithHashedPassword) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AdministrationService admin(database);

    const int id = admin.create_account("lecturer1", "CorrectHorseBattery", lms::Role::Lecturer);
    ASSERT_GT(id, 0);

    lms::SqliteUserRepository users(database);
    lms::AuthenticationService auth(users);
    auto session = auth.sign_in("lecturer1", "CorrectHorseBattery");
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->user().role(), lms::Role::Lecturer);
}

TEST(Administration, RejectsDuplicateUsernameAndShortPassword) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AdministrationService admin(database);
    admin.create_account("student1", "CorrectHorseBattery", lms::Role::Student);

    EXPECT_THROW(admin.create_account("student1", "AnotherPassword", lms::Role::Student), std::exception);
    EXPECT_THROW(admin.create_account("student2", "short", lms::Role::Student), std::invalid_argument);
}

TEST(Administration, UpdatesAccountAndDeactivatesIt) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AdministrationService admin(database);
    const int id = admin.create_account("student1", "CorrectHorseBattery", lms::Role::Student);

    admin.update_account(id, "admin1", lms::Role::SystemAdministrator);
    lms::SqliteUserRepository users(database);
    lms::AuthenticationService auth(users);
    auto updated = auth.sign_in("admin1", "CorrectHorseBattery");
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->user().role(), lms::Role::SystemAdministrator);

    admin.deactivate_account(id);
    EXPECT_EQ(auth.sign_in("admin1", "CorrectHorseBattery"), nullptr);
}

TEST(Administration, KeepsAcademicRowsSafeByOnlyDeactivatingAccount) {
    lms::Database database(":memory:");
    database.execute_script(kSchema);
    lms::AdministrationService admin(database);
    const int id = admin.create_account("student1", "CorrectHorseBattery", lms::Role::Student);
    admin.deactivate_account(id);

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database.handle(), "SELECT COUNT(*) FROM user_account WHERE user_id = ?1", -1, &statement, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(statement, 1, id), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 1);
    sqlite3_finalize(statement);
}
