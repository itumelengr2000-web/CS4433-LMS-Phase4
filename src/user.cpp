#include "lms/user.hpp"

namespace lms {

std::string role_name(Role role) {
    switch (role) {
        case Role::Student: return "STUDENT";
        case Role::Lecturer: return "LECTURER";
        case Role::HeadOfDepartment: return "HOD";
        case Role::SystemAdministrator: return "ADMIN";
    }
    throw std::invalid_argument("unknown role");
}

std::unique_ptr<User> UserFactory::create(int id, const std::string& username, Role role) {
    switch (role) {
        case Role::Student: return std::make_unique<Student>(id, username);
        case Role::Lecturer: return std::make_unique<Lecturer>(id, username);
        case Role::HeadOfDepartment: return std::make_unique<HeadOfDepartment>(id, username);
        case Role::SystemAdministrator: return std::make_unique<SystemAdministrator>(id, username);
    }
    throw std::invalid_argument("unknown role");
}

} // namespace lms
