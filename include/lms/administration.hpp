#pragma once

#include "lms/database.hpp"
#include "lms/user.hpp"

#include <string>

namespace lms {

class AdministrationService {
public:
    explicit AdministrationService(Database& database) : database_(database) {}

    int create_account(const std::string& username, const std::string& password,
                       Role role);
    void update_account(int user_id, const std::string& username, Role role);
    void deactivate_account(int user_id);

private:
    Database& database_;
};

} // namespace lms
