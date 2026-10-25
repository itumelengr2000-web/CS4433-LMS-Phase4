#pragma once

#include "lms/database.hpp"
#include "lms/user.hpp"

#include <memory>
#include <string>

namespace lms {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual std::unique_ptr<User> find_active_by_username(const std::string& username,
                                                           const std::string& password) const = 0;
};

class SqliteUserRepository final : public IUserRepository {
public:
    explicit SqliteUserRepository(const Database& database) : database_(database) {}
    std::unique_ptr<User> find_active_by_username(const std::string& username,
                                                  const std::string& password) const override;

private:
    const Database& database_;
};

} // namespace lms
