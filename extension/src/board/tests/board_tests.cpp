// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Board view layout and camera framing tests. No Godot, no framework; run via ctest.
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

#include "board_layout.hpp"
#include "feel.hpp"

using namespace montezuma::godot_binding;

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

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

void test_cell_centres_are_symmetric() {
    const layout::Grid g{8, 8, 1.0};
    const auto first = layout::cell_center(g, 0, 0);
    const auto last = layout::cell_center(g, 7, 7);
    check(near(first.x, -3.5) && near(first.z, -3.5), "cell (0,0) is the far left corner");
    check(near(last.x, 3.5) && near(last.z, 3.5), "cell (7,7) is the near right corner");
    const layout::Grid odd{5, 3, 2.0};
    const auto mid = layout::cell_center(odd, 2, 1);
    check(near(mid.x, 0.0) && near(mid.z, 0.0), "odd board centres its middle cell");
}

void test_cell_at_round_trips_every_cell() {
    for (const layout::Grid g : {layout::Grid{8, 8, 1.0}, layout::Grid{6, 9, 0.75}, layout::Grid{3, 3, 2.0}}) {
        for (int y = 0; y < g.height; ++y) {
            for (int x = 0; x < g.width; ++x) {
                const auto c = layout::cell_center(g, x, y);
                int ox = -1;
                int oy = -1;
                // Hit near each corner of the cell, not just the centre.
                for (double dx : {-0.45, 0.0, 0.45}) {
                    for (double dz : {-0.45, 0.0, 0.45}) {
                        const bool hit = layout::cell_at(g, {c.x + dx * g.cell, c.z + dz * g.cell}, ox, oy);
                        check(hit && ox == x && oy == y,
                              "cell_at finds (" + std::to_string(x) + "," + std::to_string(y) + ")");
                    }
                }
            }
        }
    }
}

void test_cell_at_rejects_off_board_points() {
    const layout::Grid g{8, 8, 1.0};
    int x = 0;
    int y = 0;
    check(!layout::cell_at(g, {-4.01, 0.0}, x, y), "left of board");
    check(!layout::cell_at(g, {4.01, 0.0}, x, y), "right of board");
    check(!layout::cell_at(g, {0.0, -4.01}, x, y), "beyond far edge");
    check(!layout::cell_at(g, {0.0, 4.01}, x, y), "before near edge");
}

layout::Fit board_fit(double aspect) {
    layout::Fit f;
    f.half_width = 4.2;
    f.half_depth = 4.2;
    f.height = 0.6;
    f.tilt_degrees = feel::kCameraTiltDegrees;
    f.vfov_degrees = feel::kCameraFovDegrees;
    f.aspect = aspect;
    f.margin = feel::kCameraMargin;
    return f;
}

void test_fit_distance_is_tight() {
    // Narrowest phone to widest desktop, plus square and tablet.
    for (double aspect : {9.0 / 21.0, 9.0 / 19.5, 9.0 / 16.0, 3.0 / 4.0, 1.0, 4.0 / 3.0, 16.0 / 9.0, 21.0 / 9.0}) {
        const auto f = board_fit(aspect);
        const double d = layout::fit_distance(f);
        check(layout::fits(f, d), "board fits at aspect " + std::to_string(aspect));
        check(!layout::fits(f, d * 0.99), "fit is tight at aspect " + std::to_string(aspect));
    }
}

void test_tall_screens_need_more_distance() {
    const double phone = layout::fit_distance(board_fit(9.0 / 19.5));
    const double square = layout::fit_distance(board_fit(1.0));
    const double desktop = layout::fit_distance(board_fit(16.0 / 9.0));
    const double ultrawide = layout::fit_distance(board_fit(21.0 / 9.0));
    check(phone > square, "portrait phone fits by width, so the camera backs off");
    check(square > desktop, "a square board fits a wide screen by height");
    check(near(desktop, ultrawide), "past a wide enough screen, height alone decides");
}

void test_fit_handles_straight_down_and_margin() {
    auto f = board_fit(1.0);
    f.tilt_degrees = 0.0;
    f.height = 0.0;
    f.margin = 0.0;
    // Straight down at a square aspect: half the board over tan(fov/2).
    const double expected = 4.2 / std::tan(f.vfov_degrees * std::numbers::pi / 360.0);
    check(std::abs(layout::fit_distance(f) - expected) < 1e-6, "top-down fit matches the closed form");
    f.margin = 0.1;
    check(layout::fit_distance(f) > expected, "margin pushes the camera back");
}

}  // namespace

int main() {
    test_cell_centres_are_symmetric();
    test_cell_at_round_trips_every_cell();
    test_cell_at_rejects_off_board_points();
    test_fit_distance_is_tight();
    test_tall_screens_need_more_distance();
    test_fit_handles_straight_down_and_margin();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
