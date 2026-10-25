#pragma once

#include "lms/database.hpp"

#include <string>

namespace lms {

class AssignmentService {
public:
    explicit AssignmentService(Database& database) : database_(database) {}

    int create_assignment(int lecturer_id, int course_id, const std::string& title,
                          const std::string& instructions, const std::string& due_at,
                          double maximum_mark, bool active = true);
    void update_assignment(int lecturer_id, int assignment_id, const std::string& title,
                           const std::string& instructions, const std::string& due_at,
                           double maximum_mark, bool active);
    void set_assignment_active(int lecturer_id, int assignment_id, bool active);
    void grant_individual_extension(int lecturer_id, int assignment_id, int student_id,
                                    const std::string& new_due_at, const std::string& reason);

private:
    Database& database_;
};

} // namespace lms
