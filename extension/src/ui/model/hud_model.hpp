// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <map>
#include <string>

namespace montezuma::ui {

// Display state of the in-level HUD, fed by the game signals. Each update
// returns whether anything visible changed, so the view only touches
// widgets when it must (time_changed can arrive every frame).
class HudModel {
public:
    static constexpr double kDefaultWarningSeconds = 10.0;

    struct Totem {
        double charge = 0.0;  // 0..1
        int uses_left = 0;
    };

    void reset();
    // Sets the full time for the timer bar; non-positive values are ignored.
    void set_time_limit(double seconds);
    bool set_time(double seconds_left);
    bool set_score(int score);
    bool set_gems(int collected, int goal);
    bool set_quota(int kind, int remaining);
    bool set_totem(const std::string& id, double charge, int uses_left);
    // bar 0..1 fills toward Frenzy; seconds_left > 0 while Frenzy is on.
    bool set_frenzy(double bar, double seconds_left);
    bool set_shuffles(int left);
    bool set_hint(int cost, bool affordable);

    const std::string& clock() const { return clock_; }
    // 0..1 of the time limit; 1 until a limit is known.
    double time_fraction() const;
    bool time_warning() const { return time_left_ > 0.0 && time_left_ <= warning_seconds_; }
    void set_warning_seconds(double s) { warning_seconds_ = s; }
    int score() const { return score_; }
    int gems() const { return gems_; }
    int gem_goal() const { return gem_goal_; }
    const std::map<int, int>& quotas() const { return quotas_; }
    const std::map<std::string, Totem>& totems() const { return totems_; }
    double frenzy_bar() const { return frenzy_bar_; }
    bool frenzy_on() const { return frenzy_seconds_ > 0; }
    int frenzy_seconds() const { return frenzy_seconds_; }
    int shuffles() const { return shuffles_; }
    int hint_cost() const { return hint_cost_; }
    bool hint_affordable() const { return hint_affordable_; }

private:
    double time_limit_ = 0.0;
    double time_left_ = 0.0;
    double warning_seconds_ = kDefaultWarningSeconds;
    bool warning_ = false;
    std::string clock_ = "0:00";
    int score_ = 0;
    int gems_ = 0;
    int gem_goal_ = 0;
    std::map<int, int> quotas_;
    std::map<std::string, Totem> totems_;
    double frenzy_bar_ = 0.0;
    int frenzy_seconds_ = 0;  // shown whole seconds, rounded up
    int shuffles_ = 0;
    int hint_cost_ = 0;
    bool hint_affordable_ = false;
};

}  // namespace montezuma::ui
