#pragma once

namespace lms {

class ILatePenaltyStrategy {
public:
    virtual ~ILatePenaltyStrategy() = default;
    virtual double final_mark(double raw_mark, double maximum_mark,
                              int late_days) const = 0;
};

class PerDayLatePenaltyStrategy final : public ILatePenaltyStrategy {
public:
    explicit PerDayLatePenaltyStrategy(int penalty_days = 3,
                                       double daily_rate = 0.10)
        : penalty_days_(penalty_days), daily_rate_(daily_rate) {}

    double final_mark(double raw_mark, double maximum_mark,
                      int late_days) const override;

private:
    int penalty_days_;
    double daily_rate_;
};

} // namespace lms
