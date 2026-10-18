// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "hud_model.hpp"

#include <algorithm>
#include <cmath>

#include "format.hpp"

namespace montezuma::ui {

namespace {
// Meter steps the player can see; smaller changes are not redrawn.
constexpr double kMeterStep = 0.01;

double unit(double v) { return std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.0; }

// Whether a 0..1 meter moved enough to redraw (ends always redraw).
bool meter_changed(double before, double after) {
    if (before == after) return false;
    return after == 0.0 || after == 1.0 || std::fabs(before - after) >= kMeterStep;
}
}  // namespace

void HudModel::reset() { *this = HudModel{}; }

void HudModel::set_time_limit(double seconds) {
    if (std::isfinite(seconds) && seconds > 0.0) time_limit_ = seconds;
}

bool HudModel::set_time(double seconds_left) {
    if (!std::isfinite(seconds_left)) seconds_left = 0.0;
    seconds_left = std::max(0.0, seconds_left);
    // Without a known limit, the first value seen is the full time.
    if (time_limit_ <= 0.0 && seconds_left > 0.0) time_limit_ = seconds_left;
    time_left_ = seconds_left;
    std::string clock = format_clock(seconds_left);
    const bool warning = time_warning();
    const bool changed = clock != clock_ || warning != warning_;
    clock_ = std::move(clock);
    warning_ = warning;
    return changed;
}

double HudModel::time_fraction() const {
    if (time_limit_ <= 0.0) return 1.0;
    return std::clamp(time_left_ / time_limit_, 0.0, 1.0);
}

bool HudModel::set_score(int score) {
    if (score == score_) return false;
    score_ = score;
    return true;
}

bool HudModel::set_gems(int collected, int goal) {
    collected = std::max(0, collected);
    goal = std::max(0, goal);
    if (collected == gems_ && goal == gem_goal_) return false;
    gems_ = collected;
    gem_goal_ = goal;
    return true;
}

bool HudModel::set_quota(int kind, int remaining) {
    remaining = std::max(0, remaining);
    auto it = quotas_.find(kind);
    if (it != quotas_.end() && it->second == remaining) return false;
    quotas_[kind] = remaining;
    return true;
}

bool HudModel::set_totem(const std::string& id, double charge, int uses_left) {
    charge = unit(charge);
    uses_left = std::max(0, uses_left);
    auto it = totems_.find(id);
    if (it != totems_.end() && it->second.uses_left == uses_left && !meter_changed(it->second.charge, charge)) return false;
    totems_[id] = {charge, uses_left};
    return true;
}

bool HudModel::set_frenzy(double bar, double seconds_left) {
    bar = unit(bar);
    const int seconds = std::isfinite(seconds_left) && seconds_left > 0.0 ? static_cast<int>(std::ceil(seconds_left)) : 0;
    if (seconds == frenzy_seconds_ && !meter_changed(frenzy_bar_, bar)) return false;
    frenzy_bar_ = bar;
    frenzy_seconds_ = seconds;
    return true;
}

bool HudModel::set_shuffles(int left) {
    left = std::max(0, left);
    if (left == shuffles_) return false;
    shuffles_ = left;
    return true;
}

bool HudModel::set_hint(int cost, bool affordable) {
    cost = std::max(0, cost);
    if (cost == hint_cost_ && affordable == hint_affordable_) return false;
    hint_cost_ = cost;
    hint_affordable_ = affordable;
    return true;
}

}  // namespace montezuma::ui
