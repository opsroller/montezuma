// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Core rules tests. No framework dependency; run via ctest.
#include <cstdio>
#include <string>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/level_state.hpp"

using namespace montezuma;

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

Board board_from(const std::vector<std::string>& rows) {
    Board b(static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), 7, 1);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) b.set(x, y, rows[y][x] - '0');
    return b;
}

void test_fresh_board_has_no_matches_and_a_move() {
    for (int s = 0; s < 200; ++s) {
        Board b(8, 8, 7, s);
        check(b.find_matches().empty(), "fresh board seed " + std::to_string(s) + " has no matches");
        check(b.has_legal_move(), "fresh board seed " + std::to_string(s) + " has a move");
    }
}

void test_illegal_swaps_are_rejected() {
    Board b = board_from({"0123", "1230", "2301", "3012"});
    auto before = b.cells();
    check(!b.try_swap({0, 0}, {2, 0}), "non-adjacent swap rejected");
    check(!b.try_swap({0, 0}, {1, 0}), "non-matching swap rejected");
    check(!b.try_swap({3, 3}, {4, 3}), "out of bounds rejected");
    check(b.cells() == before, "rejected swaps leave board unchanged");
}

void test_legal_swap_clears_and_refills() {
    Board b = board_from({"0010", "1234", "2345", "3456"});
    auto r = b.try_swap({2, 0}, {3, 0});
    check(r.has_value(), "swap making 000 is legal");
    check(r && !r->steps.empty() && r->steps[0].at(0) == 3, "first step clears three of kind 0");
    check(r && r->matched_kind == 0, "matched kind is 0");
    bool full = true;
    for (int c : b.cells()) full = full && c != Board::kEmpty;
    check(full, "board fully refilled");
    check(b.find_matches().empty(), "no matches left after resolve");
}

void test_matches_count_overlapping_cells_once() {
    Board b = board_from({"00012", "01345", "02456", "13561", "24612"});
    check(b.find_matches().size() == 5, "L-shape of 5 cells counted once each");
}

void test_random_play_keeps_invariants() {
    Board b(8, 8, 7, 7);
    int moves = 0;
    for (int i = 0; i < 300; ++i) {
        bool moved = false;
        for (int y = 0; y < 8 && !moved; ++y)
            for (int x = 0; x < 8 && !moved; ++x)
                for (Cell n : {Cell{x + 1, y}, Cell{x, y + 1}})
                    if (!moved && b.try_swap({x, y}, n)) moved = true;
        check(moved, "a legal move always exists");
        if (!moved) break;
        ++moves;
        check(b.find_matches().empty(), "board settles after move " + std::to_string(i));
    }
    check(moves == 300, "300 moves played");
}

void test_cascade_scoring() {
    MoveResult r;
    r.steps = {{{0, 3}}, {{1, 4}}};
    check(r.score() == 3 * 10 * 1 + 4 * 10 * 2, "cascade multiplier applied");
    check(r.cleared_by_kind() == std::map<int, int>{{0, 3}, {1, 4}}, "cleared totals per kind");
}

void test_level_win_and_streak() {
    LevelState level({{0, 5}, {1, 3}}, 120.0);
    MoveResult a;
    a.steps = {{{0, 3}}};
    a.matched_kind = 0;
    level.apply_move(a);
    level.apply_move(a);
    check(level.streak() == 2 && level.streak_kind() == 0, "same-kind matches build a streak");
    check(level.quotas().at(0) == 0 && level.outcome() == LevelState::Outcome::Playing,
          "partial quotas keep playing");
    MoveResult b;
    b.steps = {{{1, 3}}};
    b.matched_kind = 1;
    level.apply_move(b);
    check(level.streak() == 1 && level.streak_kind() == 1, "different kind resets the streak");
    check(level.outcome() == LevelState::Outcome::Won, "all quotas met wins");
    check(level.score() == 90, "score accumulates");
}

void test_level_loses_on_timeout() {
    LevelState level({{0, 5}}, 2.0);
    level.tick(1.5);
    check(level.outcome() == LevelState::Outcome::Playing, "still playing with time left");
    level.tick(1.0);
    check(level.outcome() == LevelState::Outcome::Lost && level.time_left() == 0.0, "timeout loses");
}

void test_seed_is_deterministic() {
    check(Board(8, 8, 7, 42).cells() == Board(8, 8, 7, 42).cells(), "same seed, same board");
    check(Board(8, 8, 7, 42).cells() != Board(8, 8, 7, 43).cells(), "different seed, different board");
}

}  // namespace

int main() {
    test_fresh_board_has_no_matches_and_a_move();
    test_illegal_swaps_are_rejected();
    test_legal_swap_clears_and_refills();
    test_matches_count_overlapping_cells_once();
    test_random_play_keeps_invariants();
    test_cascade_scoring();
    test_level_win_and_streak();
    test_level_loses_on_timeout();
    test_seed_is_deterministic();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
