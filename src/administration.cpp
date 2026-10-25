#include "lms/administration.hpp"

#include "lms/password_hasher.hpp"

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

void check_step(sqlite3* db, sqlite3_stmt* statement, const char* operation) {
    const int result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        const std::string message = std::string(operation) + ": " + sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        throw std::runtime_error(message);
    }
}

void bind_text(sqlite3* db, sqlite3_stmt* statement, int index,
               const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

bool table_exists(sqlite3* db, const std::string& table_name) {
    const char* sql = "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_text(db, statement, 1, table_name);
    const bool exists = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return exists;
}

} // namespace

int AdministrationService::create_account(const std::string& username,
                                           const std::string& password, Role role) {
    if (username.empty()) throw std::invalid_argument("username cannot be empty");
    const std::string hash = PasswordHasher::hash(password);
    const std::string role_value = role_name(role);
    sqlite3* db = database_.handle();
    database_.execute_script("BEGIN TRANSACTION;");
    try {
        const char* sql = "INSERT INTO user_account "
                          "(username, password_hash, role, active, created_at) "
                          "VALUES (?1, ?2, ?3, 1, ?4)";
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }
        bind_text(db, statement, 1, username);
        bind_text(db, statement, 2, hash);
        bind_text(db, statement, 3, role_value);
        bind_text(db, statement, 4, utc_timestamp());
        check_step(db, statement, "account creation");
        sqlite3_finalize(statement);
        const int id = static_cast<int>(sqlite3_last_insert_rowid(db));
        if (role == Role::Student && table_exists(db, "student_profile")) {
            const char* profile_sql = "INSERT INTO student_profile (user_id, student_number) VALUES (?1, ?2)";
            sqlite3_stmt* profile = nullptr;
            if (sqlite3_prepare_v2(db, profile_sql, -1, &profile, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
            if (sqlite3_bind_int(profile, 1, id) != SQLITE_OK) { sqlite3_finalize(profile); throw std::runtime_error(sqlite3_errmsg(db)); }
            bind_text(db, profile, 2, username);
            check_step(db, profile, "student profile creation");
            sqlite3_finalize(profile);
        } else if (role == Role::Lecturer && table_exists(db, "lecturer_profile")) {
            const char* profile_sql = "INSERT INTO lecturer_profile (user_id, staff_number) VALUES (?1, ?2)";
            sqlite3_stmt* profile = nullptr;
            if (sqlite3_prepare_v2(db, profile_sql, -1, &profile, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
            if (sqlite3_bind_int(profile, 1, id) != SQLITE_OK) { sqlite3_finalize(profile); throw std::runtime_error(sqlite3_errmsg(db)); }
            bind_text(db, profile, 2, username);
            check_step(db, profile, "lecturer profile creation");
            sqlite3_finalize(profile);
        }
        database_.execute_script("COMMIT;");
        return id;
    } catch (...) {
        database_.execute_script("ROLLBACK;");
        throw;
    }
}

void AdministrationService::update_account(int user_id, const std::string& username,
                                            Role role) {
    if (user_id <= 0) throw std::invalid_argument("user id must be positive");
    if (username.empty()) throw std::invalid_argument("username cannot be empty");
    const char* sql = "UPDATE user_account SET username = ?1, role = ?2 WHERE user_id = ?3";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    bind_text(db, statement, 1, username);
    bind_text(db, statement, 2, role_name(role));
    if (sqlite3_bind_int(statement, 3, user_id) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    check_step(db, statement, "account update");
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(statement);
    if (changed == 0) throw std::runtime_error("account not found");
}

void AdministrationService::deactivate_account(int user_id) {
    if (user_id <= 0) throw std::invalid_argument("user id must be positive");
    const char* sql = "UPDATE user_account SET active = 0 WHERE user_id = ?1";
    sqlite3_stmt* statement = nullptr;
    sqlite3* db = database_.handle();
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    if (sqlite3_bind_int(statement, 1, user_id) != SQLITE_OK) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    check_step(db, statement, "account deactivation");
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(statement);
    if (changed == 0) throw std::runtime_error("account not found");
}

} // namespace lms
