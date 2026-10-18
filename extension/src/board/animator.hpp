// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "feel.hpp"

// Turns a move's events (contract "MoveResult events" in
// docs/reference/contracts.md) into a timed plan of animation phases. No
// Godot types: the plan is tested on its own, and the board view plays it
// with tweens. The plan only re-times what the rules reported; it never
// decides anything.

namespace montezuma::godot_binding::anim {

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

struct Fall {
    Cell from;
    Cell to;
    int kind = -1;
};

struct Spawn {
    Cell to;
    int kind = -1;
    int drop = 1;  // rows above the top edge it starts: row -drop
};

struct Step {
    std::vector<Cell> cleared;
    std::vector<Fall> falls;
    std::vector<Spawn> spawns;
};

// What the view needs from a MoveResult, in the contract's order.
struct Events {
    std::optional<std::array<Cell, 2>> swap;
    std::vector<Step> steps;
    bool shuffled = false;
    bool redealt = false;
    std::vector<std::pair<Cell, Cell>> shuffle_map;  // {from, to}
};

// Seconds for each beat; defaults are the feel targets in feel.hpp.
struct Timing {
    double swap = feel::kSwapSeconds;
    double clear_pop = feel::kClearPopSeconds;
    double clear_shrink = feel::kClearShrinkSeconds;
    double gravity = feel::kFallGravityCells;
    double fall_min = feel::kFallMinSeconds;
    double cascade_gap = feel::kCascadeGapSeconds;
    double shuffle = feel::kShuffleSeconds;
    double redeal_out = feel::kRedealOutSeconds;
    double redeal_in = feel::kRedealInSeconds;

    // Time to fall `rows` rows from rest under constant gravity, never
    // shorter than fall_min so one-row drops still read.
    double fall(double rows) const { return std::max(fall_min, std::sqrt(2.0 * std::max(rows, 0.0) / gravity)); }
    double clear() const { return clear_pop + clear_shrink; }
};

// One phase of the plan. Items in a phase play together; phases follow each
// other in time. `step` is the cascade step a Clear/Fall phase belongs to.
struct Phase {
    enum class Kind { Swap, Clear, Fall, Shuffle, Redeal };
    Kind kind = Kind::Swap;
    int step = -1;
    double start = 0.0;
    double duration = 0.0;
    std::vector<Cell> cells;                    // Clear: cells emptied
    std::vector<std::pair<Cell, Cell>> moves;   // Swap, Fall, Shuffle: {from, to}
    std::vector<double> move_seconds;           // Fall: per move (gravity)
    std::vector<Spawn> spawns;                  // Fall: new pieces
    std::vector<double> spawn_seconds;          // Fall: per spawn
};

struct Plan {
    std::vector<Phase> phases;
    double total = 0.0;
};

inline Plan build_plan(const Events& ev, const Timing& t = {}) {
    Plan plan;
    double now = 0.0;
    if (ev.swap) {
        Phase p;
        p.kind = Phase::Kind::Swap;
        p.start = now;
        p.duration = t.swap;
        p.moves = {{(*ev.swap)[0], (*ev.swap)[1]}, {(*ev.swap)[1], (*ev.swap)[0]}};
        plan.phases.push_back(std::move(p));
        now += t.swap;
    }
    for (std::size_t i = 0; i < ev.steps.size(); ++i) {
        const Step& s = ev.steps[i];
        if (i > 0) now += t.cascade_gap;
        Phase clear;
        clear.kind = Phase::Kind::Clear;
        clear.step = static_cast<int>(i);
        clear.start = now;
        clear.duration = t.clear();
        clear.cells = s.cleared;
        plan.phases.push_back(std::move(clear));
        now += t.clear();

        Phase fall;
        fall.kind = Phase::Kind::Fall;
        fall.step = static_cast<int>(i);
        fall.start = now;
        for (const Fall& f : s.falls) {
            fall.moves.emplace_back(f.from, f.to);
            fall.move_seconds.push_back(t.fall(f.to.y - f.from.y));
            fall.duration = std::max(fall.duration, fall.move_seconds.back());
        }
        for (const Spawn& sp : s.spawns) {
            fall.spawns.push_back(sp);
            fall.spawn_seconds.push_back(t.fall(sp.to.y + sp.drop));
            fall.duration = std::max(fall.duration, fall.spawn_seconds.back());
        }
        now += fall.duration;
        plan.phases.push_back(std::move(fall));
    }
    if (ev.shuffled && ev.redealt) {
        Phase p;
        p.kind = Phase::Kind::Redeal;
        p.start = now;
        p.duration = t.redeal_out + t.redeal_in;
        now += p.duration;
        plan.phases.push_back(std::move(p));
    } else if (ev.shuffled && !ev.shuffle_map.empty()) {
        Phase p;
        p.kind = Phase::Kind::Shuffle;
        p.start = now;
        p.duration = t.shuffle;
        p.moves = ev.shuffle_map;
        plan.phases.push_back(std::move(p));
        now += t.shuffle;
    }
    plan.total = now;
    return plan;
}

// Walks a plan over a grid of slots (whatever the caller tracks per cell: a
// kind in tests, a scene node in the view), in the same order the view
// animates it, so tests prove the view ends where the rules did.
//
// Hooks, each called in plan order:
//   on_move(phase, index, slot, from, to)     slot is moving from -> to
//   on_clear(phase, slot, cell)               slot leaves the board (return nothing)
//   make_spawn(phase, index, spawn) -> Slot   a new slot for a spawn
//   on_redeal(phase)                          the caller rebuilds the grid itself
template <class Slot, class Hooks>
void walk(const Plan& plan, std::vector<Slot>& grid, int width, Hooks& hooks) {
    auto at = [&](Cell c) -> Slot& { return grid[static_cast<std::size_t>(c.y * width + c.x)]; };
    for (const Phase& p : plan.phases) {
        switch (p.kind) {
            case Phase::Kind::Clear:
                for (const Cell& c : p.cells) {
                    hooks.on_clear(p, at(c), c);
                    at(c) = Slot{};
                }
                break;
            case Phase::Kind::Redeal:
                hooks.on_redeal(p);
                break;
            case Phase::Kind::Swap:
            case Phase::Kind::Shuffle:
            case Phase::Kind::Fall: {
                // Moves in a phase happen at once: lift every mover, then place.
                std::vector<Slot> lifted;
                lifted.reserve(p.moves.size());
                for (std::size_t i = 0; i < p.moves.size(); ++i) {
                    lifted.push_back(at(p.moves[i].first));
                    hooks.on_move(p, i, lifted.back(), p.moves[i].first, p.moves[i].second);
                }
                for (const auto& m : p.moves) at(m.first) = Slot{};
                for (std::size_t i = 0; i < p.moves.size(); ++i) at(p.moves[i].second) = lifted[i];
                for (std::size_t i = 0; i < p.spawns.size(); ++i) at(p.spawns[i].to) = hooks.make_spawn(p, i, p.spawns[i]);
                break;
            }
        }
    }
}

}  // namespace montezuma::godot_binding::anim
