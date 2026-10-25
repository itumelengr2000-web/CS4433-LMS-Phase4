cmake_minimum_required(VERSION 3.20)
project(moderated_lms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)

add_library(lms_core
    src/academic_administration.cpp
    src/administration.cpp
    src/assignment_service.cpp
    src/authentication.cpp
    src/console_ui.cpp
    src/database.cpp
    src/late_penalty.cpp
    src/marking_service.cpp
    src/moderation_service.cpp
    src/password_hasher.cpp
    src/student_service.cpp
    src/user_repository.cpp
    src/user.cpp
)
target_include_directories(lms_core PUBLIC include)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto)
target_compile_options(lms_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_executable(lms_app src/main.cpp)
target_link_libraries(lms_app PRIVATE lms_core)

enable_testing()
add_executable(lms_tests
    tests/test_academic_administration.cpp
    tests/test_administration.cpp
    tests/test_assignment_service.cpp
    tests/test_authentication.cpp
    tests/test_console_ui.cpp
    tests/test_late_penalty.cpp
    tests/test_marking_service.cpp
    tests/test_moderation_service.cpp
    tests/test_password_hasher.cpp
    tests/test_student_service.cpp
)
target_link_libraries(lms_tests PRIVATE lms_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(lms_tests)

install(TARGETS lms_app RUNTIME DESTINATION bin)
install(FILES schema.sql DESTINATION share/moderated-lms)
