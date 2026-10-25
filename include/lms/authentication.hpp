#pragma once

#include "lms/user.hpp"
#include "lms/user_repository.hpp"

#include <memory>
#include <string>

namespace lms {

class Session {
public:
    explicit Session(std::unique_ptr<User> user) : user_(std::move(user)) {}
    bool active() const noexcept { return static_cast<bool>(user_); }
    const User& user() const;
    void sign_out() noexcept { user_.reset(); }

private:
    std::unique_ptr<User> user_;
};

class AuthenticationService {
public:
    explicit AuthenticationService(const IUserRepository& users) : users_(users) {}
    std::unique_ptr<Session> sign_in(const std::string& username,
                                     const std::string& password) const;

private:
    const IUserRepository& users_;
};

} // namespace lms
