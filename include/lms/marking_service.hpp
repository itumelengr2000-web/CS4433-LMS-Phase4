#pragma once

#include "lms/database.hpp"
#include "lms/late_penalty.hpp"

#include <string>
#include <vector>

namespace lms {

struct MarkResult {
    int mark_id;
    double raw_mark;
    double final_mark;
    int late_days;
    std::string feedback;
};

struct SubmissionSummary {
    int submission_id;
    int version_id;
    int student_id;
    int assignment_id;
    std::string assignment_title;
    int version_no;
    std::string received_at;
    std::string storage_path;
    bool is_late;
};

class MarkingService {
public:
    MarkingService(Database& database, const ILatePenaltyStrategy& penalty)
        : database_(database), penalty_(penalty) {}

    MarkResult mark_submission(int lecturer_id, int submission_id, int version_id,
                               double raw_mark, const std::string& feedback);
    std::vector<SubmissionSummary> submissions_for_lecturer(int lecturer_id) const;

private:
    Database& database_;
    const ILatePenaltyStrategy& penalty_;
};

} // namespace lms
