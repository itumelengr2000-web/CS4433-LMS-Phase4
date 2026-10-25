#include "lms/console_ui.hpp"
#include "lms/database.hpp"
#include <gtest/gtest.h>

TEST(ConsoleUi, CanBeConstructedWithDatabase) {
    lms::Database database(":memory:");
    lms::ConsoleUi ui(database);
    (void)ui;
    SUCCEED();
}
