// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Board view layout and camera framing tests. No Godot, no framework; run via ctest.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "animator.hpp"
#include "board_layout.hpp"
#include "feel.hpp"
#include "input.hpp"
#include "montezuma/board.hpp"

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

// Converts the rules' result into the view's events, as the binding's
// Dictionary carries them.
anim::Events events_from(const montezuma::MoveResult& r) {
    anim::Events ev;
    if (r.swap) ev.swap = std::array<Cell, 2>{Cell{(*r.swap)[0].x, (*r.swap)[0].y}, Cell{(*r.swap)[1].x, (*r.swap)[1].y}};
    for (const auto& c : r.cascades) {
        anim::Step s;
        for (const auto& cell : c.cleared) s.cleared.push_back({cell.x, cell.y});
        for (const auto& f : c.falls) s.falls.push_back({{f.from.x, f.from.y}, {f.to.x, f.to.y}, f.kind});
        for (const auto& sp : c.spawns) s.spawns.push_back({{sp.to.x, sp.to.y}, sp.kind, sp.drop});
        ev.steps.push_back(s);
    }
    ev.shuffled = r.shuffled;
    ev.redealt = r.redealt;
    for (const auto& [from, to] : r.shuffle_map) ev.shuffle_map.push_back({{from.x, from.y}, {to.x, to.y}});
    return ev;
}

// Kinds in a grid; a redeal reads the board's new deal, as the view does.
struct BoardHooks : KindHooks {
    const montezuma::Board* board = nullptr;
    std::vector<int>* grid = nullptr;
    void on_redeal(const anim::Phase&) { *grid = board->cells(); }
};

void test_walk_matches_the_real_board() {
    // Drives the rules for hundreds of moves; after each, walking the
    // reported events over the old grid must give exactly the new board.
    int moves = 0;
    int shuffles = 0;
    int effects = 0;
    int redeals = 0;
    bool all_match = true;
    // A small board runs out of moves often, so shuffles get played too.
    struct Size {
        int w, h, kinds;
    };
    for (const Size size : {Size{8, 8, 5}, Size{4, 4, 5}}) {
        const int w = size.w;
        const int h = size.h;
        for (std::uint64_t seed = 1; seed <= 20; ++seed) {
            auto made = montezuma::Board::try_create(w, h, size.kinds, seed);
            if (!made) continue;
            montezuma::Board& board = *made;
            std::mt19937 rng(static_cast<unsigned>(seed));
            for (int turn = 0; turn < 40; ++turn) {
                std::vector<int> grid = board.cells();
                std::optional<montezuma::MoveResult> result;
                if (turn % 7 == 6) {
                    // An effect clearing three random cells (bonuses, totems).
                    std::vector<montezuma::Cell> cells;
                    for (int i = 0; i < 3; ++i) cells.push_back({static_cast<int>(rng() % static_cast<unsigned>(w)), static_cast<int>(rng() % static_cast<unsigned>(h))});
                    result = board.clear_cells(cells);
                    if (result) ++effects;
                } else {
                    const int n = w * h;
                    const int start = static_cast<int>(rng() % static_cast<unsigned>(n));
                    for (int i = 0; i < n && !result; ++i) {
                        const int x = (start + i) % w;
                        const int y = (start + i) % n / w;
                        for (const montezuma::Cell to : {montezuma::Cell{x + 1, y}, montezuma::Cell{x, y + 1}}) {
                            if (!board.in_bounds(to.x, to.y) || !board.is_legal_swap({x, y}, to)) continue;
                            result = board.try_swap({x, y}, to);
                            break;
                        }
                    }
                }
                if (!result || result->stuck) break;
                if (result->shuffled) ++(result->redealt ? redeals : shuffles);
                BoardHooks hooks;
                hooks.board = &board;
                hooks.grid = &grid;
                anim::walk(anim::build_plan(events_from(*result)), grid, w, hooks);
                all_match = all_match && grid == board.cells();
                ++moves;
            }
        }
    }
    check(moves >= 300, "played hundreds of real moves");
    check(effects > 0, "including clear_cells effects");
    check(shuffles > 0, "and shuffles");
    check(all_match, "every walk ends on the rules' board");
    std::printf("real-board walk: %d moves, %d shuffles, %d redeals, %d effects\n", moves, shuffles, redeals, effects);
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

// ---- Input gestures ----

using input::Action;

input::SwapGesture gesture() {
    input::SwapGesture g;
    g.reset(8, 8);
    g.set_swipe_threshold(20.0);
    return g;
}

bool is(const input::Output& o, Action a, input::Cell ca = {}, input::Cell cb = {}) {
    return o.action == a && o.a == ca && o.b == cb;
}

void test_tap_tap_swaps_neighbours() {
    auto g = gesture();
    check(is(g.press(-1, {0, 0}, {3, 3}), Action::Select, {3, 3}), "first tap selects");
    check(is(g.release(-1), Action::None), "lifting keeps the selection");
    check(is(g.press(-1, {50, 0}, {4, 3}), Action::Swap, {3, 3}, {4, 3}), "tapping a neighbour swaps");
    check(!g.selected().valid(), "swap clears the selection");
    check(is(g.release(-1), Action::None), "the swap's release does nothing");
}

void test_tap_selected_again_deselects() {
    auto g = gesture();
    g.press(-1, {0, 0}, {2, 2});
    g.release(-1);
    check(is(g.press(-1, {0, 0}, {2, 2}), Action::None), "second press on the selection waits");
    check(is(g.release(-1), Action::Deselect), "and deselects on release");
}

void test_tap_far_piece_moves_selection() {
    auto g = gesture();
    g.press(-1, {0, 0}, {0, 0});
    g.release(-1);
    check(is(g.press(-1, {0, 0}, {5, 5}), Action::Select, {5, 5}), "a far piece takes the selection");
    g.release(-1);
    check(is(g.press(-1, {0, 0}, {}), Action::Deselect), "tapping off the board deselects");
    check(is(g.press(-1, {0, 0}, {}), Action::None), "off the board with nothing selected does nothing");
}

void test_swipe_in_each_direction() {
    struct Case {
        double dx, dy;
        input::Cell to;
    };
    for (const Case c : {Case{30, 4, {4, 3}}, Case{-30, -4, {2, 3}}, Case{3, 30, {3, 4}}, Case{-3, -30, {3, 2}}}) {
        auto g = gesture();
        g.press(0, {100, 100}, {3, 3});
        check(is(g.move(0, {100 + c.dx * 0.5, 100 + c.dy * 0.5}), Action::None), "short drag is not a swipe");
        check(is(g.move(0, {100 + c.dx, 100 + c.dy}), Action::Swap, {3, 3}, c.to), "swipe swaps toward the drag");
        check(is(g.move(0, {100 + c.dx * 2, 100 + c.dy * 2}), Action::None), "one swipe swaps once");
        check(is(g.release(0), Action::None), "release after a swipe does nothing");
    }
}

void test_swipe_off_the_edge_and_from_selection() {
    auto g = gesture();
    g.press(0, {0, 0}, {0, 0});
    check(is(g.move(0, {-40, 0}), Action::Deselect), "swipe off the board only deselects");
    check(is(g.press(1, {0, 0}, {5, 5}), Action::None), "the spent press holds until it lifts");
    g.release(0);
    // A swipe that starts on the selected piece still swipes.
    g.press(0, {0, 0}, {6, 6});
    g.release(0);
    g.press(0, {0, 0}, {6, 6});
    check(is(g.move(0, {0, 25}), Action::Swap, {6, 6}, {6, 7}), "swipe from the selected piece");
}

void test_phone_tap_pair_counts_once() {
    // A phone tap reaches Godot as an emulated mouse press and a touch. The
    // view drops emulated mouse events; even if one got through, the first
    // pointer down owns the press, so each tap acts once.
    auto g = gesture();
    check(is(g.press(-1, {0, 0}, {3, 3}), Action::Select, {3, 3}), "tap 1 selects");
    check(is(g.press(0, {0, 0}, {3, 3}), Action::None), "its touch twin is ignored");
    check(is(g.release(-1), Action::None) && is(g.release(0), Action::None), "both releases keep the selection");
    check(is(g.press(-1, {50, 0}, {4, 3}), Action::Swap, {3, 3}, {4, 3}), "tap 2 swaps");
    check(is(g.press(0, {50, 0}, {4, 3}), Action::None), "its touch twin does not select again");
    g.release(-1);
    g.release(0);
    check(!g.selected().valid() && !g.pressing(), "nothing left selected or held");
    // A swipe: the pair of presses, then drags from both streams.
    check(is(g.press(-1, {100, 100}, {2, 2}), Action::Select, {2, 2}), "swipe press selects");
    g.press(0, {100, 100}, {2, 2});
    check(is(g.move(0, {130, 100}), Action::None), "the twin's drag is ignored");
    check(is(g.move(-1, {130, 100}), Action::Swap, {2, 2}, {3, 2}), "one swipe, one swap");
}

void test_pointer_rules() {
    auto g = gesture();
    g.press(1, {0, 0}, {3, 3});
    check(is(g.press(2, {0, 0}, {4, 3}), Action::None), "a second finger is ignored");
    check(is(g.move(2, {50, 0}), Action::None), "the second finger cannot swipe");
    check(is(g.release(2), Action::None), "nor release the first");
    g.cancel();
    check(!g.pressing() && g.selected() == input::Cell{3, 3}, "cancel ends the press, keeps the selection");
    check(is(g.move(1, {50, 0}), Action::None), "no swipe after cancel");
    check(is(g.press(1, {0, 0}, {3, 4}), Action::Swap, {3, 3}, {3, 4}), "tap-tap still works after cancel");
}

}  // namespace

void test_board_padding_measures_the_frame() {
    layout::Grid g{8, 8, 1.0};
    check(near(layout::board_padding(g, -4.2, -4.2, 4.2, 4.2), 0.2), "slab 0.2 past the cells");
    check(near(layout::board_padding(g, -4.0, -4.7, 4.0, 4.1), 0.7), "widest side wins");
    check(near(layout::board_padding(g, -3.0, -3.0, 3.0, 3.0), 0.0), "inside the cells is no padding");
    g.cell = 2.0;
    check(near(layout::board_padding(g, -9.0, -8.0, 8.0, 8.0), 0.5), "padding is in cells");
}

int main() {
    test_cell_centres_are_symmetric();
    test_cell_at_round_trips_every_cell();
    test_cell_at_rejects_off_board_points();
    test_fit_distance_is_tight();
    test_tall_screens_need_more_distance();
    test_fit_handles_straight_down_and_margin();
    test_board_padding_measures_the_frame();
    test_plan_orders_phases_in_time();
    test_fall_timing_follows_gravity();
    test_walk_ends_where_the_rules_did();
    test_walk_matches_the_real_board();
    test_walk_moves_falls_at_once();
    test_shuffle_and_redeal();
    test_timing_scales_without_changing_order();
    test_tap_tap_swaps_neighbours();
    test_tap_selected_again_deselects();
    test_tap_far_piece_moves_selection();
    test_swipe_in_each_direction();
    test_swipe_off_the_edge_and_from_selection();
    test_phone_tap_pair_counts_once();
    test_pointer_rules();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
