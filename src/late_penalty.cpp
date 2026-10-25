#include "lms/late_penalty.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lms {

double PerDayLatePenaltyStrategy::final_mark(double raw_mark, double maximum_mark,
                                             int late_days) const {
    if (maximum_mark <= 0.0 || raw_mark < 0.0 || raw_mark > maximum_mark || late_days < 0) {
        throw std::invalid_argument("invalid mark or lateness value");
    }
    if (late_days > penalty_days_) return 0.0;
    const double deduction = maximum_mark * daily_rate_ * static_cast<double>(late_days);
    const double result = std::max(0.0, raw_mark - deduction);
    return std::round(result * 100.0) / 100.0;
}

} // namespace lms
