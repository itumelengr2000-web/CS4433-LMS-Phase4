#pragma once

#include "lms/database.hpp"

#include <optional>
#include <string>
#include <vector>

namespace lms {

struct ReleasedMark {
    double raw_mark;
    double final_mark;
    std::string feedback;
};

struct BatchSummary {
    int batch_id;
    int assignment_id;
    std::string status;
};

struct ModerationMarkSummary {
    int mark_id;
    int student_id;
    double raw_mark;
    double final_mark;
    std::string decision;
};

class ModerationService {
public:
    explicit ModerationService(Database& database) : database_(database) {}

    int open_batch(int hod_user_id, int assignment_id);
    std::vector<BatchSummary> batches_for_hod(int hod_user_id) const;
    std::vector<ModerationMarkSummary> marks_for_batch(int hod_user_id, int batch_id) const;
    void approve_mark(int hod_user_id, int batch_id, int mark_id, const std::string& reason);
    void reject_mark(int hod_user_id, int batch_id, int mark_id, const std::string& reason);
    void release_batch(int hod_user_id, int batch_id);
    std::optional<ReleasedMark> released_mark(int student_id, int assignment_id) const;

private:
    Database& database_;
};

} // namespace lms
