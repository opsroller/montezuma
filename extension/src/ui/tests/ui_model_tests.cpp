// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// UI view-logic tests. No framework dependency; run via ctest.
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

#include "model/format.hpp"
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

}  // namespace

int main() {
    test_nav_stack_push_back_and_root();
    test_nav_stack_replace_and_reset();
    test_settings_sanitize();
    test_linear_to_db();
    test_format_clock();
    test_level_id();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
