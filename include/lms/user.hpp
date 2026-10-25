#pragma once

#include <memory>
#include <stdexcept>
#include <string>

namespace lms {

enum class Role { Student, Lecturer, HeadOfDepartment, SystemAdministrator };

std::string role_name(Role role);

class User {
public:
    User(int id, std::string username) : id_(id), username_(std::move(username)) {}
    virtual ~User() = default;
    virtual Role role() const = 0;
    int id() const noexcept { return id_; }
    const std::string& username() const noexcept { return username_; }

private:
    int id_;
    std::string username_;
};

class Student final : public User { public: using User::User; Role role() const override { return Role::Student; } };
class Lecturer final : public User { public: using User::User; Role role() const override { return Role::Lecturer; } };
class HeadOfDepartment final : public User { public: using User::User; Role role() const override { return Role::HeadOfDepartment; } };
class SystemAdministrator final : public User { public: using User::User; Role role() const override { return Role::SystemAdministrator; } };

class UserFactory {
public:
    static std::unique_ptr<User> create(int id, const std::string& username, Role role);
};

} // namespace lms
