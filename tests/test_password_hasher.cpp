#include "lms/password_hasher.hpp"
#include <gtest/gtest.h>

TEST(PasswordHasher, HashesAndVerifiesPassword) {
    const auto encoded = lms::PasswordHasher::hash("CorrectHorseBattery");
    EXPECT_NE(encoded, "CorrectHorseBattery");
    EXPECT_TRUE(lms::PasswordHasher::verify("CorrectHorseBattery", encoded));
    EXPECT_FALSE(lms::PasswordHasher::verify("wrong-password", encoded));
}

TEST(PasswordHasher, SamePasswordGetsDifferentSaltedHashes) {
    EXPECT_NE(lms::PasswordHasher::hash("CorrectHorseBattery"),
              lms::PasswordHasher::hash("CorrectHorseBattery"));
}
