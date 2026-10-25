#pragma once

#include <string>

namespace lms {

class PasswordHasher {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& encoded_hash);
};

} // namespace lms
