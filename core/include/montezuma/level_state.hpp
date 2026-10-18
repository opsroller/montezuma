// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <map>

#include "montezuma/board.hpp"

namespace montezuma {

// Quotas, timer, score and the same-kind streak that charges Power Totems.
class LevelState {
public:
    enum class Outcome { Playing, Won, Lost };

    // Throws std::invalid_argument for empty or non-positive quotas or time.
    LevelState(std::map<int, int> quotas, double time_limit);

    void apply_move(const MoveResult& result);
    void tick(double delta);

    const std::map<int, int>& quotas() const { return quotas_; }
    double time_left() const { return time_left_; }
    int score() const { return score_; }
    int streak_kind() const { return streak_kind_; }
    int streak() const { return streak_; }
    Outcome outcome() const { return outcome_; }

private:
    std::map<int, int> quotas_;  // kind -> pieces still needed
    double time_left_;
    int score_ = 0;
    int streak_kind_ = -1;
    int streak_ = 0;
    Outcome outcome_ = Outcome::Playing;
};

}  // namespace montezuma
