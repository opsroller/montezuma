// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Fire light flicker noise tests. No Godot, no framework; run via ctest.
#include <cmath>
#include <cstdio>
#include <string>

#include "flicker.hpp"

using namespace montezuma::godot_binding::flicker;

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

void test_range_and_mean() {
    double sum = 0.0;
    float lo = 1.0f;
    float hi = -1.0f;
    const int n = 60 * 120;  // two minutes at 60 fps
    for (int i = 0; i < n; ++i) {
        const float v = fire(static_cast<float>(i) / 60.0f, 7.0f, 1234U);
        lo = std::fmin(lo, v);
        hi = std::fmax(hi, v);
        sum += v;
    }
    check(lo >= -1.0f && hi <= 1.0f, "fire stays in [-1, 1]");
    check(hi - lo > 0.6f, "fire swings visibly");
    check(std::fabs(sum / n) < 0.1, "fire averages about 0, so the light keeps its authored energy");
}

void test_smooth() {
    // At 60 fps no step is a jump: a flame flickers, it does not strobe.
    float worst = 0.0f;
    for (int i = 0; i < 600; ++i) {
        const float t = static_cast<float>(i) / 60.0f;
        worst = std::fmax(worst, std::fabs(fire(t + 1.0f / 60.0f, 7.0f, 9U) - fire(t, 7.0f, 9U)));
    }
    check(worst < 0.35f, "consecutive frames differ by less than 0.35");
}

void test_deterministic_and_seeded() {
    check(fire(3.25f, 7.0f, 42U) == fire(3.25f, 7.0f, 42U), "same time and seed give the same value");
    int same = 0;
    for (int i = 0; i < 100; ++i) {
        const float t = static_cast<float>(i) * 0.1f + 0.05f;
        if (std::fabs(fire(t, 7.0f, 1U) - fire(t, 7.0f, 2U)) < 0.02f) ++same;
    }
    check(same < 20, "different seeds flicker differently");
    check(gradient_noise(5.0f, 7U) == 0.0f, "gradient noise is 0 at whole numbers");
}

void test_position_seed() {
    const auto a = seed_from_position(-5.5f, 0.85f, -5.5f);
    check(a == seed_from_position(-5.5f, 0.85f, -5.5f), "position seed is stable");
    check(a != seed_from_position(5.5f, 0.85f, -5.5f), "mirrored braziers get different seeds");
    check(a != seed_from_position(-5.5f, 0.85f, 5.5f), "braziers front and back get different seeds");
}

}  // namespace

int main() {
    test_range_and_mean();
    test_smooth();
    test_deterministic_and_seeded();
    test_position_seed();
    std::printf("flicker_tests: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
