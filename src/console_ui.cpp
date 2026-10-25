#include "lms/console_ui.hpp"

#include "lms/academic_administration.hpp"
#include "lms/administration.hpp"
#include "lms/assignment_service.hpp"
#include "lms/authentication.hpp"
#include "lms/late_penalty.hpp"
#include "lms/marking_service.hpp"
#include "lms/moderation_service.hpp"
#include "lms/student_service.hpp"
#include "lms/user_repository.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace lms {
namespace {

const bool kNoColour = std::getenv("NO_COLOR") != nullptr;
std::string colour(const char* code) { return kNoColour ? "" : std::string("\033[") + code + "m"; }
const std::string reset = kNoColour ? "" : "\033[0m";
const std::string gray = colour("90");
const std::string cyan = colour("36");
const std::string green = colour("32");
const std::string yellow = colour("33");
const std::string red = colour("31");
const std::string magenta = colour("35");
const std::string white = colour("37");

void title() {
    std::cout << gray << "\n========================================\n"
              << "        MODERATED LEARNING SYSTEM\n"
              << "========================================\n" << reset;
}

void message(const std::string& colour_code, const std::string& text) {
    std::cout << colour_code << text << reset << "\n";
}

bool read_integer(const std::string& prompt, int& value) {
    std::cout << cyan << prompt << reset;
    std::string input;
    if (!std::getline(std::cin, input)) return false;
    try { value = std::stoi(input); return true; }
    catch (...) { message(red, "Please enter a whole number."); return false; }
}

bool read_decimal(const std::string& prompt, double& value) {
    std::cout << cyan << prompt << reset;
    std::string input;
    if (!std::getline(std::cin, input)) return false;
    try { value = std::stod(input); return true; }
    catch (...) { message(red, "Please enter a number."); return false; }
}

bool read_text(const std::string& prompt, std::string& value) {
    std::cout << cyan << prompt << reset;
    return static_cast<bool>(std::getline(std::cin, value));
}

Role read_role() {
    int choice = 0;
    if (!read_integer("Role (1 Student, 2 Lecturer, 3 HoD, 4 Administrator): ", choice)) throw std::invalid_argument("invalid role");
    switch (choice) {
        case 1: return Role::Student;
        case 2: return Role::Lecturer;
        case 3: return Role::HeadOfDepartment;
        case 4: return Role::SystemAdministrator;
        default: throw std::invalid_argument("invalid role");
    }
}

void administrator_menu(Database& database) {
    AdministrationService accounts(database);
    AcademicAdministrationService academic(database);
    while (true) {
        std::cout << "\n" << gray << "[ADMINISTRATOR DASHBOARD]" << reset << "\n"
                  << white << "1. Create account\n2. Edit account\n3. Deactivate account\n"
                  << "4. Create department\n5. Create course\n6. Assign lecturer\n"
                  << "7. Enrol student\n8. Withdraw student\n9. Assign HoD to department\n" << reset
                  << yellow << "0. Sign out" << reset << "\n> ";
        std::string choice;
        if (!std::getline(std::cin, choice)) return;
        if (choice == "0") return;
        try {
            int id = 0;
            std::string first;
            switch (std::stoi(choice)) {
                case 1: {
                    std::string username, password;
                    if (!read_text("Username: ", username) || !read_text("Password: ", password)) return;
                    id = accounts.create_account(username, password, read_role());
                    message(green, "Account created with ID " + std::to_string(id) + ".");
                    break;
                }
                case 2:
                    if (!read_integer("Account ID: ", id) || !read_text("New username: ", first)) return;
                    accounts.update_account(id, first, read_role());
                    message(green, "Account updated.");
                    break;
                case 3:
                    if (!read_integer("Account ID: ", id)) return;
                    accounts.deactivate_account(id);
                    message(green, "Account deactivated. Linked academic records remain stored.");
                    break;
                case 4:
                    if (!read_text("Department name: ", first)) return;
                    id = academic.create_department(first);
                    message(green, "Department created with ID " + std::to_string(id) + ".");
                    break;
                case 5: {
                    std::string code, name; int department = 0;
                    if (!read_text("Course code: ", code) || !read_text("Course name: ", name) || !read_integer("Department ID: ", department)) return;
                    id = academic.create_course(code, name, department);
                    message(green, "Course created with ID " + std::to_string(id) + ".");
                    break;
                }
                case 6: {
                    int course = 0, lecturer = 0;
                    if (!read_integer("Course ID: ", course) || !read_integer("Lecturer user ID: ", lecturer)) return;
                    academic.assign_lecturer(course, lecturer);
                    message(green, "Lecturer assigned to course.");
                    break;
                }
                case 7: {
                    int course = 0, student = 0;
                    if (!read_integer("Course ID: ", course) || !read_integer("Student user ID: ", student)) return;
                    academic.enrol_student(course, student);
                    message(green, "Student enrolled.");
                    break;
                }
                case 8: {
                    int course = 0, student = 0;
                    if (!read_integer("Course ID: ", course) || !read_integer("Student user ID: ", student)) return;
                    academic.withdraw_student(course, student);
                    message(green, "Student withdrawn while preserving enrolment history.");
                    break;
                }
                case 9: {
                    int department = 0, hod = 0;
                    if (!read_integer("Department ID: ", department) || !read_integer("HoD user ID: ", hod)) return;
                    academic.assign_hod_to_department(department, hod);
                    message(green, "HoD assigned to department.");
                    break;
                }
                default: message(yellow, "Choose one of the displayed menu options."); break;
            }
        } catch (const std::exception& error) { message(red, std::string("Operation failed: ") + error.what()); }
    }
}

void lecturer_menu(Database& database, int lecturer_id) {
    AssignmentService assignments(database);
    PerDayLatePenaltyStrategy penalty;
    MarkingService marking(database, penalty);
    while (true) {
        std::cout << "\n" << cyan << "[LECTURER DASHBOARD]" << reset << "\n"
                  << white << "1. Create assignment\n2. Edit assignment\n3. Activate/deactivate assignment\n"
                  << "4. Grant an individual extension\n5. View submissions\n6. Enter marks and feedback\n" << reset
                  << yellow << "0. Sign out" << reset << "\n> ";
        std::string choice;
        if (!std::getline(std::cin, choice)) return;
        if (choice == "0") return;
        try {
            switch (std::stoi(choice)) {
                case 1: {
                    int course = 0; double maximum = 0.0; std::string title_text, instructions, due;
                    if (!read_integer("Course ID: ", course) || !read_text("Title: ", title_text) || !read_text("Instructions: ", instructions) || !read_text("Effective due timestamp (YYYY-MM-DDTHH:MM:SSZ): ", due) || !read_decimal("Maximum mark: ", maximum)) return;
                    const int id = assignments.create_assignment(lecturer_id, course, title_text, instructions, due, maximum);
                    message(green, "Assignment created with ID " + std::to_string(id) + ".");
                    break;
                }
                case 2: {
                    int assignment = 0, active = 0; double maximum = 0.0; std::string title_text, instructions, due;
                    if (!read_integer("Assignment ID: ", assignment) || !read_text("Title: ", title_text) || !read_text("Instructions: ", instructions) || !read_text("Effective due timestamp (YYYY-MM-DDTHH:MM:SSZ): ", due) || !read_decimal("Maximum mark: ", maximum) || !read_integer("Active? (1 yes, 0 no): ", active)) return;
                    assignments.update_assignment(lecturer_id, assignment, title_text, instructions, due, maximum, active != 0);
                    message(green, "Assignment updated.");
                    break;
                }
                case 3: {
                    int assignment = 0, active = 0;
                    if (!read_integer("Assignment ID: ", assignment) || !read_integer("Active? (1 yes, 0 no): ", active)) return;
                    assignments.set_assignment_active(lecturer_id, assignment, active != 0);
                    message(green, "Assignment status updated.");
                    break;
                }
                case 4: {
                    int assignment = 0, student = 0; std::string due, reason;
                    if (!read_integer("Assignment ID: ", assignment) || !read_integer("Student user ID: ", student) || !read_text("New effective due timestamp: ", due) || !read_text("Reason: ", reason)) return;
                    assignments.grant_individual_extension(lecturer_id, assignment, student, due, reason);
                    message(green, "Individual extension recorded.");
                    break;
                }
                case 5: {
                    const auto submissions = marking.submissions_for_lecturer(lecturer_id);
                    if (submissions.empty()) { message(yellow, "No submissions are available."); break; }
                    for (const auto& submission : submissions) {
                        std::cout << white << "Submission " << submission.submission_id << ", version " << submission.version_no
                                  << ", student " << submission.student_id << ", assignment " << submission.assignment_id
                                  << " (" << submission.assignment_title << "), received " << submission.received_at
                                  << (submission.is_late ? " [LATE]" : " [ON TIME]") << "\nFile: " << submission.storage_path << reset << "\n";
                    }
                    break;
                }
                case 6: {
                    int submission = 0, version = 0; double raw = 0.0; std::string feedback;
                    if (!read_integer("Submission ID: ", submission) || !read_integer("Version ID: ", version) || !read_decimal("Raw mark: ", raw) || !read_text("Written feedback: ", feedback)) return;
                    const auto result = marking.mark_submission(lecturer_id, submission, version, raw, feedback);
                    message(green, "Mark saved. Final mark: " + std::to_string(result.final_mark) + ".");
                    break;
                }
                default: message(yellow, "Choose one of the displayed menu options."); break;
            }
        } catch (const std::exception& error) { message(red, std::string("Operation failed: ") + error.what()); }
    }
}

void hod_menu(Database& database, int hod_user_id) {
    ModerationService moderation(database);
    while (true) {
        std::cout << "\n" << magenta << "[HEAD OF DEPARTMENT DASHBOARD]" << reset << "\n"
                  << white << "1. Open or select assessment batch\n2. View batch marks\n3. Approve mark\n"
                  << "4. Reject mark\n5. Release approved batch\n" << reset
                  << yellow << "0. Sign out" << reset << "\n> ";
        std::string choice;
        if (!std::getline(std::cin, choice)) return;
        if (choice == "0") return;
        try {
            switch (std::stoi(choice)) {
                case 1: {
                    const auto batches = moderation.batches_for_hod(hod_user_id);
                    if (batches.empty()) message(yellow, "No batches are open. Enter an assignment ID to open one.");
                    else for (const auto& batch : batches) std::cout << white << "Batch " << batch.batch_id << " | assignment " << batch.assignment_id << " | " << batch.status << reset << "\n";
                    int assignment = 0;
                    if (!read_integer("Assignment ID (0 to cancel): ", assignment) || assignment == 0) break;
                    const int batch = moderation.open_batch(hod_user_id, assignment);
                    message(green, "Assessment batch ID: " + std::to_string(batch));
                    break;
                }
                case 2: {
                    int batch = 0;
                    if (!read_integer("Batch ID: ", batch)) return;
                    const auto marks = moderation.marks_for_batch(hod_user_id, batch);
                    if (marks.empty()) message(yellow, "No marks are available in this batch.");
                    for (const auto& mark : marks) std::cout << white << "Mark " << mark.mark_id << " | student " << mark.student_id << " | raw " << mark.raw_mark << " | final " << mark.final_mark << " | " << mark.decision << reset << "\n";
                    break;
                }
                case 3: {
                    int batch = 0, mark = 0; std::string reason;
                    if (!read_integer("Batch ID: ", batch) || !read_integer("Mark ID: ", mark) || !read_text("Approval reason: ", reason)) return;
                    moderation.approve_mark(hod_user_id, batch, mark, reason);
                    message(green, "Mark approved.");
                    break;
                }
                case 4: {
                    int batch = 0, mark = 0; std::string reason;
                    if (!read_integer("Batch ID: ", batch) || !read_integer("Mark ID: ", mark) || !read_text("Rejection reason: ", reason)) return;
                    moderation.reject_mark(hod_user_id, batch, mark, reason);
                    message(yellow, "Mark rejected and returned for review.");
                    break;
                }
                case 5: {
                    int batch = 0;
                    if (!read_integer("Batch ID: ", batch)) return;
                    moderation.release_batch(hod_user_id, batch);
                    message(green, "Complete approved batch released. Students can now view their marks.");
                    break;
                }
                default: message(yellow, "Choose one of the displayed menu options."); break;
            }
        } catch (const std::exception& error) { message(red, std::string("Operation failed: ") + error.what()); }
    }
}

void student_menu(Database& database, int student_id) {
    StudentService students(database);
    ModerationService moderation(database);
    while (true) {
        std::cout << "\n" << green << "[STUDENT DASHBOARD]" << reset << "\n"
                  << white << "1. View my courses and active assignments\n2. Submit or resubmit an assignment\n"
                  << "3. View my submission status\n4. View released mark and feedback\n" << reset
                  << yellow << "0. Sign out" << reset << "\n> ";
        std::string choice;
        if (!std::getline(std::cin, choice)) return;
        if (choice == "0") return;
        try {
            switch (std::stoi(choice)) {
                case 1: {
                    const auto assignments = students.active_assignments(student_id);
                    if (assignments.empty()) { message(yellow, "No active assignments are available."); break; }
                    for (const auto& assignment : assignments) {
                        std::cout << white << "Assignment " << assignment.assignment_id << " | " << assignment.course_code
                                  << " | " << assignment.title << " | Maximum: " << assignment.maximum_mark
                                  << " | Due: " << assignment.effective_due_at << reset << "\n";
                    }
                    break;
                }
                case 2: {
                    int assignment = 0; std::string path;
                    if (!read_integer("Assignment ID: ", assignment) || !read_text("Path to file: ", path)) return;
                    const auto receipt = students.submit_assignment(student_id, assignment, path);
                    message(green, "Submission version " + std::to_string(receipt.version_no) + " saved as " + (receipt.is_late ? "LATE." : "ON TIME."));
                    break;
                }
                case 3: {
                    const auto submissions = students.submissions_for_student(student_id);
                    if (submissions.empty()) { message(yellow, "You have no submissions."); break; }
                    for (const auto& submission : submissions) {
                        std::cout << white << "Submission " << submission.submission_id << " | assignment " << submission.assignment_id
                                  << " (" << submission.assignment_title << ") | version " << submission.version_no
                                  << " | received " << submission.received_at << (submission.is_late ? " | LATE" : " | ON TIME")
                                  << " | status " << submission.status << reset << "\n";
                    }
                    break;
                }
                case 4: {
                    int assignment = 0;
                    if (!read_integer("Assignment ID: ", assignment)) return;
                    const auto result = moderation.released_mark(student_id, assignment);
                    if (!result) message(yellow, "This mark has not been released yet.");
                    else {
                        message(green, "Raw mark: " + std::to_string(result->raw_mark) + " | Final mark: " + std::to_string(result->final_mark));
                        std::cout << white << "Feedback: " << result->feedback << reset << "\n";
                    }
                    break;
                }
                default: message(yellow, "Choose one of the displayed menu options."); break;
            }
        } catch (const std::exception& error) { message(red, std::string("Operation failed: ") + error.what()); }
    }
}

} // namespace

void ConsoleUi::run() {
    SqliteUserRepository users(database_);
    AuthenticationService authentication(users);
    title();
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::string username, password;
        if (!read_text("Username: ", username) || !read_text("Password: ", password)) return;
        auto session = authentication.sign_in(username, password);
        if (!session) { message(red, "Login failed. Check your username and password."); continue; }
        message(green, "Login successful.");
        switch (session->user().role()) {
            case Role::SystemAdministrator: administrator_menu(database_); break;
            case Role::Lecturer: lecturer_menu(database_, session->user().id()); break;
            case Role::Student: student_menu(database_, session->user().id()); break;
            case Role::HeadOfDepartment: hod_menu(database_, session->user().id()); break;
        }
        message(green, "Signed out.");
        title();
    }
    message(red, "Too many failed login attempts. The application is closing.");
}

} // namespace lms
