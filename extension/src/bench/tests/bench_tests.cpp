// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Benchmark replay and frame statistics tests. No Godot, no framework; run via ctest.
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "frame_stats.hpp"
#include "montezuma/board.hpp"
#include "replay.hpp"

using namespace montezuma;
using namespace montezuma::godot_binding::bench;

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

// The seed game/scenes/bench/bench.tscn uses.
constexpr std::uint64_t kSceneSeed = 52;

void test_nearest_rank_percentiles() {
    std::vector<double> v;
    for (int i = 1; i <= 100; ++i) v.push_back(static_cast<double>(101 - i));  // 100 .. 1, unsorted
    const FrameSummary s = summarize(v, 90.0);
    check(s.frames == 100, "frame count");
    check(s.p50 == 50.0, "p50 of 1..100 is 50");
    check(s.p95 == 95.0, "p95 of 1..100 is 95");
    check(s.p99 == 99.0, "p99 of 1..100 is 99");
    check(s.max == 100.0, "max");
    check(s.mean == 50.5, "mean");
    check(s.over_budget == 10, "frames over a 90 ms budget");
    check(summarize({}, 16.6).frames == 0 && summarize({}, 16.6).p95 == 0.0, "empty capture is all zero");
    check(summarize({7.0}, 0.0).p50 == 7.0 && summarize({7.0}, 0.0).over_budget == 0, "single sample, no budget");
}

void test_replay_is_deterministic() {
    ReplayConfig c;
    c.seed = kSceneSeed;
    const Replay a = plan_replay(c);
    const Replay b = plan_replay(c);
    check(a.actions.size() == b.actions.size(), "same length");
    bool same = true;
    for (std::size_t i = 0; i < a.actions.size() && i < b.actions.size(); ++i) {
        same = same && a.actions[i].board_hash == b.actions[i].board_hash && a.actions[i].label == b.actions[i].label;
    }
    check(same, "same beats and boards for the same seed");
}

// The capture covers about the first three rounds; they must hold the busy
// parts of the scenario on their own.
void test_scene_seed_is_the_busiest_board() {
    ReplayConfig c;
    c.seed = kSceneSeed;
    c.rounds = 3;
    const Replay r = plan_replay(c);
    check(r.deep_cascades >= 5, "at least five four-step cascades in three rounds");
    check(r.power_ups == 6, "two power-ups a round");
    check(r.totems == 21, "every totem fires each round");
    std::set<std::string> labels;
    for (const Action& a : r.actions) labels.insert(a.label);
    for (const char* l : {"totem:fire", "totem:time", "totem:gold", "totem:harvest", "totem:ward", "totem:gift",
                          "totem:paint", "powerup:tnt", "powerup:lightning", "swap"}) {
        check(labels.count(l) == 1, std::string("beat present: ") + l);
    }
}

// Replaying the plan through Board's public moves, as the Godot runner does
// through Match3Board, lands on the planned board after every beat.
void test_replay_matches_a_fresh_board() {
    ReplayConfig c;
    c.seed = kSceneSeed;
    const Replay r = plan_replay(c);
    auto board = Board::try_create(c.width, c.height, c.kinds, c.seed);
    check(board.has_value(), "scene board is valid");
    if (!board) return;
    bool match = true;
    for (const Action& a : r.actions) {
        if (a.kind == Action::Kind::Swap) match = match && board->try_swap(a.a, a.b).has_value();
        if (a.kind == Action::Kind::Clear) match = match && board->clear_cells(a.cells).has_value();
        match = match && hash_cells(board->cells()) == a.board_hash;
    }
    check(match, "every beat replays to the planned board");
}

void test_bad_board_throws() {
    bool threw = false;
    try {
        ReplayConfig c;
        c.width = 2;
        plan_replay(c);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "a board try_create refuses throws invalid_argument");
}

}  // namespace

int main() {
    test_nearest_rank_percentiles();
    test_replay_is_deterministic();
    test_scene_seed_is_the_busiest_board();
    test_replay_matches_a_fresh_board();
    test_bad_board_throws();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
