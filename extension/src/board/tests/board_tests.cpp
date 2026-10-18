// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Board view layout and camera framing tests. No Godot, no framework; run via ctest.
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include "animator.hpp"
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


// ---- Animator plan ----

using anim::Cell;

// Kinds in a grid, as the view tracks nodes; -1 is empty.
struct KindHooks {
    int clears = 0;
    int redeals = 0;
    void on_move(const anim::Phase&, std::size_t, int, Cell, Cell) {}
    void on_clear(const anim::Phase&, int, Cell) { ++clears; }
    int make_spawn(const anim::Phase&, std::size_t, const anim::Spawn& sp) { return sp.kind; }
    void on_redeal(const anim::Phase&) { ++redeals; }
};

std::vector<int> grid_from(const std::vector<std::string>& rows) {
    std::vector<int> g;
    for (const auto& row : rows)
        for (char c : row) g.push_back(c == '.' ? -1 : c - '0');
    return g;
}

// 3 wide, 4 tall: rows "123", "231", "310", "001". Swapping (2,2) and (2,3)
// lines up three 0s in row 3; they clear, every column drops one row and the
// top row refills with 4, 5, 6.
anim::Events three_column_events() {
    anim::Events ev;
    ev.swap = std::array<Cell, 2>{Cell{2, 2}, Cell{2, 3}};
    anim::Step s;
    s.cleared = {{0, 3}, {1, 3}, {2, 3}};
    // Every column drops one row; the top row is refilled.
    for (int x = 0; x < 3; ++x) {
        for (int y = 2; y >= 0; --y) s.falls.push_back({{x, y}, {x, y + 1}, -1});
        s.spawns.push_back({{x, 0}, 4 + x, 1});
    }
    ev.steps.push_back(s);
    return ev;
}

void test_plan_orders_phases_in_time() {
    anim::Timing t;
    auto ev = three_column_events();
    anim::Step second;
    second.cleared = {{0, 0}};
    second.spawns = {{{0, 0}, 2, 1}};
    ev.steps.push_back(second);
    const auto plan = anim::build_plan(ev, t);
    check(plan.phases.size() == 5, "swap, then clear+fall per step");
    check(plan.phases[0].kind == anim::Phase::Kind::Swap && near(plan.phases[0].start, 0.0), "swap first");
    check(near(plan.phases[1].start, t.swap), "step 0 clears when the swap lands");
    check(near(plan.phases[2].start, t.swap + t.clear()), "step 0 falls after its clear");
    check(near(plan.phases[2].duration, t.fall(1.0)), "one-row falls take one row of gravity");
    const double step1 = plan.phases[2].start + plan.phases[2].duration + t.cascade_gap;
    check(near(plan.phases[3].start, step1) && plan.phases[3].step == 1, "step 1 clears after step 0 lands and a gap");
    check(near(plan.total, plan.phases[4].start + plan.phases[4].duration), "total ends when the last piece lands");
}

void test_fall_timing_follows_gravity() {
    anim::Timing t;
    check(near(t.fall(0.1), t.fall_min), "short drops are floored so they read");
    check(t.fall(2.0) > t.fall(1.0) && t.fall(8.0) > t.fall(2.0), "longer drops take longer");
    check(t.fall(4.0) < 2.0 * t.fall(1.0) + 1e-9, "pieces accelerate: 4 rows take twice one row");
    check(std::abs(t.fall(8.0) - feel::kFallMinSeconds) > 0.1, "a full-board drop is clearly longer");
    // A spawn starting `drop` rows above the board falls to.y + drop rows.
    anim::Events ev;
    anim::Step s;
    s.spawns = {{{0, 2}, 1, 3}};
    ev.steps.push_back(s);
    const auto plan = anim::build_plan(ev, t);
    check(near(plan.phases[1].spawn_seconds[0], t.fall(5.0)), "spawn falls from row -drop");
}

void test_walk_ends_where_the_rules_did() {
    auto grid = grid_from({"123", "231", "310", "001"});
    const auto plan = anim::build_plan(three_column_events());
    KindHooks hooks;
    anim::walk(plan, grid, 3, hooks);
    check(grid == grid_from({"456", "123", "231", "311"}), "walk applies swap, clear, falls and spawns");
    check(hooks.clears == 3, "every cleared cell is reported once");
}

void test_walk_moves_falls_at_once() {
    // Falls listed top-down would overwrite each other if applied one by one.
    anim::Events ev;
    anim::Step s;
    s.cleared = {{0, 2}};
    s.falls = {{{0, 0}, {0, 1}, 1}, {{0, 1}, {0, 2}, 2}};
    s.spawns = {{{0, 0}, 3, 1}};
    ev.steps.push_back(s);
    std::vector<int> grid{1, 2, 0};
    KindHooks hooks;
    anim::walk(anim::build_plan(ev), grid, 1, hooks);
    check(grid == std::vector<int>({3, 1, 2}), "falls are simultaneous whatever their order");
}

void test_shuffle_and_redeal() {
    anim::Events ev;
    ev.shuffled = true;
    ev.shuffle_map = {{{0, 0}, {1, 0}}, {{1, 0}, {0, 0}}};
    auto plan = anim::build_plan(ev);
    check(plan.phases.size() == 1 && plan.phases[0].kind == anim::Phase::Kind::Shuffle, "shuffle phase");
    std::vector<int> grid{1, 2};
    KindHooks hooks;
    anim::walk(plan, grid, 2, hooks);
    check(grid == std::vector<int>({2, 1}), "shuffle map moves pieces");

    ev.redealt = true;
    ev.shuffle_map.clear();
    plan = anim::build_plan(ev);
    check(plan.phases.size() == 1 && plan.phases[0].kind == anim::Phase::Kind::Redeal, "redeal phase");
    check(near(plan.total, feel::kRedealOutSeconds + feel::kRedealInSeconds), "redeal length");
    anim::walk(plan, grid, 2, hooks);
    check(hooks.redeals == 1, "redeal handed to the caller");
}

void test_timing_scales_without_changing_order() {
    anim::Timing fast;
    fast.swap *= 0.5;
    fast.clear_pop *= 0.5;
    fast.clear_shrink *= 0.5;
    fast.gravity *= 4.0;  // half the fall time
    fast.fall_min *= 0.5;
    fast.cascade_gap *= 0.5;
    const auto normal = anim::build_plan(three_column_events());
    const auto quick = anim::build_plan(three_column_events(), fast);
    check(std::abs(quick.total - normal.total * 0.5) < 1e-9, "halving every beat halves the move");
    std::vector<int> a = grid_from({"123", "231", "310", "001"});
    std::vector<int> b = a;
    KindHooks h1;
    KindHooks h2;
    anim::walk(normal, a, 3, h1);
    anim::walk(quick, b, 3, h2);
    check(a == b, "speed never changes the outcome");
}

}  // namespace

int main() {
    test_cell_centres_are_symmetric();
    test_cell_at_round_trips_every_cell();
    test_cell_at_rejects_off_board_points();
    test_fit_distance_is_tight();
    test_tall_screens_need_more_distance();
    test_fit_handles_straight_down_and_margin();
    test_plan_orders_phases_in_time();
    test_fall_timing_follows_gravity();
    test_walk_ends_where_the_rules_did();
    test_walk_moves_falls_at_once();
    test_shuffle_and_redeal();
    test_timing_scales_without_changing_order();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
