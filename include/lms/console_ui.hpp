#pragma once

#include "lms/database.hpp"

namespace lms {

class ConsoleUi {
public:
    explicit ConsoleUi(Database& database) : database_(database) {}
    void run();

private:
    Database& database_;
};

} // namespace lms
