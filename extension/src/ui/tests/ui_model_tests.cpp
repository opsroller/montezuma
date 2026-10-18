// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// UI view-logic tests. No framework dependency; run via ctest.
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

#include "model/format.hpp"
#include "model/hud_model.hpp"
#include "model/nav_stack.hpp"
#include "model/settings.hpp"

using namespace montezuma::ui;

namespace {

int checks = 0;
int failures = 0;

void check(bool cond, const std::string& label) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    }
}

void test_nav_stack_push_back_and_root() {
    NavStack s;
    check(s.empty() && s.top().empty(), "new stack is empty");
    check(!s.pop(), "pop on empty stack does nothing");
    s.push("title");
    s.push("settings");
    check(s.depth() == 2 && s.top() == "settings", "push puts screen on top");
    check(s.pop() && s.top() == "title", "back returns to the screen below");
    check(!s.pop() && s.top() == "title", "back never closes the root screen");
}

void test_nav_stack_replace_and_reset() {
    NavStack s;
    s.replace("title");
    check(s.depth() == 1 && s.top() == "title", "replace on empty pushes");
    s.push("intro");
    s.replace("episode_map");
    check(s.depth() == 2 && s.top() == "episode_map" && !s.contains("intro"), "replace swaps the top only");
    s.reset("results");
    check(s.depth() == 1 && s.top() == "results", "reset leaves one screen");
    bool threw = false;
    try {
        s.push("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw && s.depth() == 1, "empty id is rejected and leaves the stack unchanged");
}

void test_settings_sanitize() {
    Settings s;
    s.music_volume = 3.0f;
    s.sfx_volume = -1.0f;
    s.text_scale = std::numeric_limits<float>::quiet_NaN();
    s.difficulty = "nightmare";
    s.sanitize();
    check(s.music_volume == 1.0f, "volume clamps to 1");
    check(s.sfx_volume == 0.0f, "volume clamps to 0");
    check(s.text_scale == 1.0f, "non-finite text scale falls back to default");
    check(s.difficulty == "relaxed", "unknown difficulty falls back to relaxed");
    s.text_scale = 5.0f;
    s.difficulty = "normal";
    s.sanitize();
    check(s.text_scale == Settings::kMaxTextScale && s.difficulty == "normal", "text scale clamps; valid difficulty kept");
}

void test_linear_to_db() {
    check(linear_to_db(0.0f) == -80.0f, "zero volume is silence");
    check(std::fabs(linear_to_db(1.0f)) < 1e-6f, "full volume is 0 dB");
    check(std::fabs(linear_to_db(0.5f) + 6.0206f) < 1e-3f, "half volume is about -6 dB");
    check(linear_to_db(std::numeric_limits<float>::quiet_NaN()) == -80.0f, "NaN volume is silence");
}

void test_format_clock() {
    check(format_clock(0.0) == "0:00", "zero");
    check(format_clock(59.2) == "1:00", "rounds up so 0:00 means time is out");
    check(format_clock(61.0) == "1:01", "minutes and padded seconds");
    check(format_clock(600.0) == "10:00", "ten minutes");
    check(format_clock(-3.0) == "0:00", "negative shows zero");
    check(format_clock(std::numeric_limits<double>::infinity()) == "0:00", "non-finite shows zero");
}

void test_level_id() {
    check(level_id(1, 3) == "e1-l03", "LevelData id format");
    check(level_id(5, 12) == "e5-l12", "two-digit level");
}

void test_hud_time() {
    HudModel h;
    check(h.time_fraction() == 1.0, "full bar before any time is known");
    check(h.set_time(90.0) && h.clock() == "1:30", "first time sets clock");
    check(h.time_fraction() == 1.0, "first time seen is the full time");
    check(!h.set_time(89.5), "same displayed second is not a change");
    check(h.set_time(45.0) && std::fabs(h.time_fraction() - 0.5) < 1e-9, "half time, half bar");
    check(!h.time_warning(), "no warning above threshold");
    check(h.set_time(9.2) && h.time_warning(), "warning at 10 s or less");
    check(h.set_time(0.0) && !h.time_warning() && h.clock() == "0:00", "time out ends warning");
    HudModel g;
    g.set_time_limit(120.0);
    g.set_time(60.0);
    check(std::fabs(g.time_fraction() - 0.5) < 1e-9, "explicit limit wins over first value");
}

void test_hud_values() {
    HudModel h;
    check(h.set_gems(3, 40) && h.gems() == 3 && h.gem_goal() == 40, "gem counter");
    check(!h.set_gems(3, 40), "unchanged gems is not a change");
    check(h.set_quota(2, 12) && !h.set_quota(2, 12), "quota changes once");
    check(h.set_quota(2, -3) && h.quotas().at(2) == 0, "quota clamps to 0");
    check(h.set_score(100) && !h.set_score(100), "score changes once");
    check(h.set_totem("time", 0.5, 2), "totem set");
    check(!h.set_totem("time", 0.505, 2), "sub-step charge change ignored");
    check(h.set_totem("time", 0.505, 1), "uses left change redraws");
    check(h.set_totem("time", 2.0, 1) && h.totems().at("time").charge == 1.0, "charge clamps to full and redraws");
    check(h.set_frenzy(0.5, 0.0) && !h.frenzy_on(), "frenzy filling");
    check(h.set_frenzy(1.0, 14.2) && h.frenzy_on() && h.frenzy_seconds() == 15, "frenzy on, seconds round up");
    check(!h.set_frenzy(1.0, 14.1), "same shown second is not a change");
    check(h.set_frenzy(0.0, 0.0) && !h.frenzy_on(), "frenzy ends");
    check(h.set_shuffles(2) && !h.set_shuffles(2), "shuffles left");
    check(h.set_hint(300, false) && h.set_hint(300, true) && h.hint_affordable(), "hint cost and affordability");
    h.reset();
    check(h.quotas().empty() && h.score() == 0 && h.time_fraction() == 1.0 && h.totems().empty(), "reset clears everything");
}

}  // namespace

int main() {
    test_nav_stack_push_back_and_root();
    test_nav_stack_replace_and_reset();
    test_settings_sanitize();
    test_linear_to_db();
    test_format_clock();
    test_level_id();
    test_hud_time();
    test_hud_values();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
