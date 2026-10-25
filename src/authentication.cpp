#include "lms/authentication.hpp"

#include <stdexcept>

namespace lms {

const User& Session::user() const {
    if (!user_) throw std::logic_error("session is signed out");
    return *user_;
}

std::unique_ptr<Session> AuthenticationService::sign_in(
    const std::string& username, const std::string& password) const {
    if (username.empty() || password.empty()) return nullptr;
    auto user = users_.find_active_by_username(username, password);
    if (!user) return nullptr;
    return std::make_unique<Session>(std::move(user));
}

} // namespace lms
