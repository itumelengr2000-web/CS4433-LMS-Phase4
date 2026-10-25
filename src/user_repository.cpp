#include "lms/user_repository.hpp"

#include "lms/password_hasher.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace lms {
namespace {
Role parse_role(const std::string& value) {
    if (value == "STUDENT") return Role::Student;
    if (value == "LECTURER") return Role::Lecturer;
    if (value == "HOD") return Role::HeadOfDepartment;
    if (value == "ADMIN") return Role::SystemAdministrator;
    throw std::runtime_error("invalid role stored in user_account");
}
} // namespace

std::unique_ptr<User> SqliteUserRepository::find_active_by_username(
    const std::string& username, const std::string& password) const {
    const char* sql = "SELECT user_id, username, password_hash, role "
                      "FROM user_account WHERE username = ?1 AND active = 1";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_.handle(), sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(database_.handle()));
    }
    const int bind_result = sqlite3_bind_text(statement, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (bind_result != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error("could not bind username");
    }

    const int step_result = sqlite3_step(statement);
    if (step_result == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return nullptr;
    }
    if (step_result != SQLITE_ROW) {
        const std::string error = sqlite3_errmsg(database_.handle());
        sqlite3_finalize(statement);
        throw std::runtime_error(error);
    }

    const int id = sqlite3_column_int(statement, 0);
    const auto* username_value = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    const auto* hash_value = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    const auto* role_value = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    const bool valid = username_value != nullptr && hash_value != nullptr && role_value != nullptr &&
                       PasswordHasher::verify(password, hash_value);
    std::unique_ptr<User> user;
    if (valid) user = UserFactory::create(id, username_value, parse_role(role_value));
    sqlite3_finalize(statement);
    return user;
}

} // namespace lms
