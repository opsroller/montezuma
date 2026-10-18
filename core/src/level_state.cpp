// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/level_state.hpp"

#include <algorithm>
#include <utility>

namespace montezuma {

LevelState::LevelState(std::map<int, int> quotas, double time_limit)
    : quotas_(std::move(quotas)), time_left_(time_limit) {}

void LevelState::apply_move(const MoveResult& result) {
    if (outcome_ != Outcome::Playing) return;
    score_ += result.score();
    auto cleared = result.cleared_by_kind();
    for (auto& [kind, needed] : quotas_) {
        auto it = cleared.find(kind);
        if (it != cleared.end()) needed = std::max(0, needed - it->second);
    }
    if (result.matched_kind == streak_kind_) {
        ++streak_;
    } else {
        streak_kind_ = result.matched_kind;
        streak_ = 1;
    }
    if (std::all_of(quotas_.begin(), quotas_.end(), [](const auto& q) { return q.second == 0; }))
        outcome_ = Outcome::Won;
}

void LevelState::tick(double delta) {
    if (outcome_ != Outcome::Playing) return;
    time_left_ = std::max(0.0, time_left_ - delta);
    if (time_left_ == 0.0) outcome_ = Outcome::Lost;
}

}  // namespace montezuma
