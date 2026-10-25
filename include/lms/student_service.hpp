#pragma once

#include "lms/database.hpp"

#include <string>
#include <vector>

namespace lms {

struct AssignmentSummary {
    int assignment_id;
    std::string course_code;
    std::string title;
    double maximum_mark;
    std::string effective_due_at;
};

struct SubmissionReceipt {
    int submission_id;
    int version_no;
    std::string received_at;
    std::string storage_path;
    std::string checksum;
    bool is_late;
};

struct StudentSubmissionSummary {
    int submission_id;
    int assignment_id;
    std::string assignment_title;
    int version_no;
    std::string received_at;
    bool is_late;
    std::string status;
};

class StudentService {
public:
    explicit StudentService(Database& database) : database_(database) {}

    std::vector<AssignmentSummary> active_assignments(int student_id) const;
    std::vector<StudentSubmissionSummary> submissions_for_student(int student_id) const;
    SubmissionReceipt submit_assignment(int student_id, int assignment_id,
                                        const std::string& source_file,
                                        const std::string& received_at = "");

private:
    Database& database_;
};

} // namespace lms
