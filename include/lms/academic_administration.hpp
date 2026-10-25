#pragma once

#include "lms/database.hpp"

#include <string>

namespace lms {

class AcademicAdministrationService {
public:
    explicit AcademicAdministrationService(Database& database) : database_(database) {}

    int create_department(const std::string& name);
    void update_department(int department_id, const std::string& name);
    int create_course(const std::string& course_code, const std::string& name,
                      int department_id);
    void update_course(int course_id, const std::string& course_code,
                       const std::string& name, int department_id);
    void assign_lecturer(int course_id, int lecturer_id);
    void assign_hod_to_department(int department_id, int hod_user_id);
    void enrol_student(int course_id, int student_id);
    void withdraw_student(int course_id, int student_id);

private:
    Database& database_;
};

} // namespace lms
