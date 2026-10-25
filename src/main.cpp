#include "lms/database.hpp"
#include "lms/console_ui.hpp"
#include "lms/administration.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    const std::string database_path = argc > 1 ? argv[1] : "lms.db";
    try {
        lms::Database database(database_path);
        std::ifstream schema_file("schema.sql");
        if (!schema_file) {
            std::cerr << "Cannot open schema.sql. Run the application from the project root.\n";
            return 1;
        }
        std::stringstream schema;
        schema << schema_file.rdbuf();
        database.execute_script(schema.str());
        std::cout << "LMS database is ready: " << database_path << "\n";
        if (argc > 2 && std::string(argv[2]) == "--init-only") return 0;
        if (argc > 2 && std::string(argv[2]) == "--create-admin") {
            std::string username;
            std::string password;
            std::cout << "Initial Administrator username: ";
            std::getline(std::cin, username);
            std::cout << "Initial Administrator password: ";
            std::getline(std::cin, password);
            lms::AdministrationService administration(database);
            const int id = administration.create_account(username, password, lms::Role::SystemAdministrator);
            std::cout << "Administrator account created with ID " << id << "\n";
            return 0;
        }
        lms::ConsoleUi ui(database);
        ui.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Startup error: " << error.what() << "\n";
        return 1;
    }
}
