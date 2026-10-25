#include "lms/authentication.hpp"
#include "lms/database.hpp"
#include "lms/password_hasher.hpp"
#include "lms/user_repository.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {
const char* kAuthSchema = R"SQL(
CREATE TABLE user_account (
  user_id INTEGER PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('STUDENT','LECTURER','HOD','ADMIN')),
  active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
  created_at TEXT NOT NULL
);
)SQL";

void add_user(lms::Database& database, int id, const std::string& username,
              const std::string& password, const std::string& role, int active) {
    const std::string sql = "INSERT INTO user_account "
        "(user_id, username, password_hash, role, active, created_at) VALUES (" +
        std::to_string(id) + ", '" + username + "', '" +
        lms::PasswordHasher::hash(password) + "', '" + role + "', " +
        std::to_string(active) + ", '2026-01-01T00:00:00Z')";
    database.execute_script(sql);
}
} // namespace

TEST(Authentication, ValidCredentialsCreateRoleSession) {
    lms::Database database(":memory:");
    database.execute_script(kAuthSchema);
    add_user(database, 1, "student1", "CorrectHorseBattery", "STUDENT", 1);
    lms::SqliteUserRepository repository(database);
    lms::AuthenticationService service(repository);

    auto session = service.sign_in("student1", "CorrectHorseBattery");
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(session->active());
    EXPECT_EQ(session->user().role(), lms::Role::Student);
}

TEST(Authentication, InvalidCredentialsDoNotCreateSession) {
    lms::Database database(":memory:");
    database.execute_script(kAuthSchema);
    add_user(database, 1, "student1", "CorrectHorseBattery", "STUDENT", 1);
    lms::SqliteUserRepository repository(database);
    lms::AuthenticationService service(repository);

    EXPECT_EQ(service.sign_in("student1", "wrong-password"), nullptr);
    EXPECT_EQ(service.sign_in("unknown", "CorrectHorseBattery"), nullptr);
}

TEST(Authentication, InactiveAccountCannotSignIn) {
    lms::Database database(":memory:");
    database.execute_script(kAuthSchema);
    add_user(database, 1, "student1", "CorrectHorseBattery", "STUDENT", 0);
    lms::SqliteUserRepository repository(database);
    lms::AuthenticationService service(repository);

    EXPECT_EQ(service.sign_in("student1", "CorrectHorseBattery"), nullptr);
}

TEST(Authentication, SignOutInvalidatesSession) {
    lms::Database database(":memory:");
    database.execute_script(kAuthSchema);
    add_user(database, 1, "admin1", "CorrectHorseBattery", "ADMIN", 1);
    lms::SqliteUserRepository repository(database);
    lms::AuthenticationService service(repository);

    auto session = service.sign_in("admin1", "CorrectHorseBattery");
    ASSERT_NE(session, nullptr);
    session->sign_out();
    EXPECT_FALSE(session->active());
    EXPECT_THROW(session->user(), std::logic_error);
}
