#include "lms/late_penalty.hpp"
#include "lms/user.hpp"
#include <gtest/gtest.h>

TEST(LatePenalty, AppliesTenPercentOfAvailableMarkPerDay) {
    lms::PerDayLatePenaltyStrategy strategy;
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 1), 70.0);
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 3), 50.0);
}

TEST(LatePenalty, GivesZeroAfterThreeDays) {
    lms::PerDayLatePenaltyStrategy strategy;
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 4), 0.0);
}

TEST(LatePenalty, MatchesPhaseTwoTimeBoundaries) {
    lms::PerDayLatePenaltyStrategy strategy;
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 0), 80.0);
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 1), 70.0);
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 3), 50.0);
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.0, 100.0, 4), 0.0);
}

TEST(LatePenalty, FloorsAtZeroAndRoundsToTwoDecimals) {
    lms::PerDayLatePenaltyStrategy strategy;
    EXPECT_DOUBLE_EQ(strategy.final_mark(5.0, 100.0, 1), 0.0);
    EXPECT_DOUBLE_EQ(strategy.final_mark(80.005, 100.0, 1), 70.01);
}

TEST(UserFactory, CreatesPolymorphicRoles) {
    auto user = lms::UserFactory::create(7, "student1", lms::Role::Student);
    EXPECT_EQ(user->role(), lms::Role::Student);
    EXPECT_EQ(user->username(), "student1");
}
