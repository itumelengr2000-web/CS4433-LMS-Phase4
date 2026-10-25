#include "lms/password_hasher.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace lms {
namespace {
constexpr int kIterations = 210000;
constexpr int kSaltBytes = 16;
constexpr int kHashBytes = 32;

std::string hex(const unsigned char* data, std::size_t size) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) out << std::setw(2) << static_cast<int>(data[i]);
    return out.str();
}

std::vector<unsigned char> unhex(const std::string& input) {
    if (input.size() % 2 != 0) throw std::invalid_argument("invalid encoded hash");
    std::vector<unsigned char> result(input.size() / 2);
    for (std::size_t i = 0; i < result.size(); ++i) {
        unsigned int value = 0;
        std::istringstream stream(input.substr(i * 2, 2));
        stream >> std::hex >> value;
        if (stream.fail()) throw std::invalid_argument("invalid encoded hash");
        result[i] = static_cast<unsigned char>(value);
    }
    return result;
}

std::vector<unsigned char> derive(const std::string& password,
                                  const std::vector<unsigned char>& salt) {
    std::vector<unsigned char> output(kHashBytes);
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()), kIterations,
                          EVP_sha256(), static_cast<int>(output.size()), output.data()) != 1) {
        throw std::runtime_error("password derivation failed");
    }
    return output;
}
} // namespace

std::string PasswordHasher::hash(const std::string& password) {
    if (password.size() < 8) throw std::invalid_argument("password must contain at least 8 characters");
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) throw std::runtime_error("salt generation failed");
    const auto derived = derive(password, salt);
    return "pbkdf2-sha256$" + std::to_string(kIterations) + "$" + hex(salt.data(), salt.size()) + "$" + hex(derived.data(), derived.size());
}

bool PasswordHasher::verify(const std::string& password, const std::string& encoded_hash) {
    const std::string prefix = "pbkdf2-sha256$";
    if (encoded_hash.rfind(prefix, 0) != 0) return false;
    const auto first = encoded_hash.find('$', prefix.size());
    const auto second = encoded_hash.find('$', first + 1);
    if (first == std::string::npos || second == std::string::npos) return false;
    try {
        const int iterations = std::stoi(encoded_hash.substr(prefix.size(), first - prefix.size()));
        if (iterations != kIterations) return false;
        const auto salt = unhex(encoded_hash.substr(first + 1, second - first - 1));
        const auto expected = unhex(encoded_hash.substr(second + 1));
        const auto actual = derive(password, salt);
        return expected.size() == actual.size() && CRYPTO_memcmp(expected.data(), actual.data(), actual.size()) == 0;
    } catch (...) {
        return false;
    }
}

} // namespace lms
