#include "lms/database.hpp"

#include <stdexcept>

namespace lms {

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "unable to open database";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(message);
    }
    execute_script("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::execute_script(const std::string& script) {
    char* error = nullptr;
    if (sqlite3_exec(db_, script.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "SQLite script failed";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

} // namespace lms
