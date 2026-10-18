// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "montezuma/board.hpp"

namespace montezuma::godot_binding::bench {

// One beat of the benchmark replay (docs/reference/benchmark.md#replay).
// Godot-free: planned on montezuma::Board, then replayed on a Match3Board
// built with the same size and seed, which resolves every beat the same way.
struct Action {
    enum class Kind {
        Swap,    // try_swap(a, b)
        Clear,   // clear_cells(cells): a power-up or totem going off
        Marker,  // a totem whose effect the Board does not model; no board change
    };
    Kind kind = Kind::Swap;
    Cell a;
    Cell b;
    std::vector<Cell> cells;
    // "swap", "powerup:tnt", "powerup:lightning", "totem:fire", "totem:time", ...
    std::string label;
    // Cascade steps the beat resolved (0 for a marker).
    int steps = 0;
    // Board::cells() hash after the beat, so a replay can check it has not
    // drifted from the plan.
    std::uint64_t board_hash = 0;
};

struct ReplayConfig {
    int width = 8;
    int height = 8;
    int kinds = 7;
    std::uint64_t seed = 0;
    // Rounds of the beat pattern; the runner loops the whole replay if it
    // runs out before the capture ends.
    int rounds = 6;
};

struct Replay {
    std::vector<Action> actions;
    // Beats that resolved four or more cascade steps.
    int deep_cascades = 0;
    int max_steps = 0;
    int power_ups = 0;
    int totems = 0;
};

// FNV-1a over the cells, the same on every platform.
std::uint64_t hash_cells(const std::vector<int>& cells);

// The busiest-board script: every round plays swaps that resolve the deepest
// cascade available, two power-ups (TNT and Lightning at tier 3) and every
// totem at its top tier. Deterministic for a config. Throws
// std::invalid_argument for a board Board::try_create refuses.
Replay plan_replay(const ReplayConfig& config);

}  // namespace montezuma::godot_binding::bench
