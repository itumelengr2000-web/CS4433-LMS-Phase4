#pragma once

#include <sqlite3.h>
#include <string>

namespace lms {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    sqlite3* handle() const noexcept { return db_; }
    void execute_script(const std::string& script);

private:
    sqlite3* db_{nullptr};
};

} // namespace lms
