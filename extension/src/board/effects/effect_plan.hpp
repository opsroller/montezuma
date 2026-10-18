// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "../animator.hpp"
#include "../feel.hpp"

// Totem and power-up effects as data (WP-29). What each display step of a
// move set off (power-ups going off, markers placed or removed, a totem
// firing) arrives with the move's events; this header times it against the
// animation plan and tracks the piece markers through the plan's walk. No
// Godot types and no rules: it only re-times and re-places what the rules
// reported, so it is tested on its own and the board view plays it.

namespace montezuma::godot_binding::fx {

using anim::Cell;

// Contract ids (docs/reference/contracts.md, "Totem and power-up ids"), in
// contract order; the index is the id used below.
inline constexpr std::array<std::string_view, 7> kTotemNames{"fire", "time", "gold", "harvest",
                                                             "ward", "gift", "paint"};
inline constexpr std::array<std::string_view, 6> kPowerUpNames{"tnt",   "color_clear", "lightning",
                                                               "clock", "multiplier",  "hint"};
inline constexpr int kTotemCount = static_cast<int>(kTotemNames.size());
inline constexpr int kPowerUpCount = static_cast<int>(kPowerUpNames.size());
enum Totem { kFire, kTime, kGold, kHarvest, kWard, kGift, kPaint };
enum PowerUp { kTnt, kColorClear, kLightning, kClock, kMultiplier, kHint };

template <std::size_t N>
int index_of(const std::array<std::string_view, N>& names, std::string_view name) {
    for (std::size_t i = 0; i < N; ++i)
        if (names[i] == name) return static_cast<int>(i);
    return -1;
}
inline int totem_from_name(std::string_view name) { return index_of(kTotemNames, name); }
inline int power_up_from_name(std::string_view name) { return index_of(kPowerUpNames, name); }

// What a piece carries, packed in one int per cell: flag bits, plus the
// power-up id + 1 in bits 4..7 (0: none).
namespace marks {
inline constexpr int kNone = 0;
inline constexpr int kGem = 1;
inline constexpr int kSteel = 2;
inline constexpr int kChain = 4;
inline constexpr int kFlags = kGem | kSteel | kChain;
inline constexpr int kPowerUpShift = 4;
inline constexpr int power_up(int m) { return ((m >> kPowerUpShift) & 0xF) - 1; }
inline constexpr int with_power_up(int m, int id) { return (m & kFlags) | (id < 0 ? 0 : (id + 1) << kPowerUpShift); }
}  // namespace marks

struct Placement {
    Cell cell;
    int id = -1;  // power-up id
};

// A power-up whose piece was cleared, and what it does.
struct PowerUpGoOff {
    Cell cell;
    int id = -1;
    std::vector<Cell> cells;  // what it clears, row-major
    double time_seconds = 0.0;
    int multiplier = 1;
    double hint_seconds = 0.0;
};

// A totem firing, shown at the start of its first display step.
struct TotemFire {
    int id = -1;
    std::vector<Cell> targets;            // fire: fireball cells
    std::vector<anim::Repaint> repaints;  // paint
    double time_seconds = 0.0;            // time
    int points = 0;                       // gold, harvest gem points
    std::vector<Cell> gems_harvested;     // harvest: collected, pieces stay
    std::vector<Cell> gems_placed;        // harvest
    std::vector<Cell> chains_removed;     // harvest tier 3+
    std::vector<Placement> power_ups_placed;  // gift
};

// Everything one display step set off, in rules order: the totem (if this
// step starts a firing), power-ups going off and markers lost as it clears,
// markers placed after its refill.
struct StepFx {
    std::optional<TotemFire> totem;
    std::vector<PowerUpGoOff> power_ups;
    std::vector<Placement> power_ups_placed;
    std::vector<Cell> gems_collected;
    std::vector<Cell> gems_placed;
    std::vector<Cell> steel_spawned;
    std::vector<Cell> steel_removed;
    std::vector<Cell> chains_broken;
};

// One timed effect for the view to play.
struct Beat {
    enum class Kind { Totem, PowerUp, GemCollect, SteelBreak, ChainBreak, MarkPlaced };
    Kind kind = Kind::Totem;
    int id = -1;       // totem or power-up id; MarkPlaced: the marks placed
    int step = -1;
    double start = 0.0;
    double duration = 0.0;
    Cell cell;                 // where it centres (a totem: none, the board)
    std::vector<Cell> cells;   // what it touches
    double value = 0.0;        // seconds or points or multiplier, for labels
};

// The time window of each step in a plan: from its lead (or clear) to its
// last landing. Transient beats of a step end inside it.
struct Window {
    double start = 0.0;
    double clear = 0.0;  // when its clear starts
    double end = 0.0;    // its last piece lands
};

inline std::vector<Window> step_windows(const anim::Plan& plan, std::size_t steps) {
    std::vector<Window> w(steps);
    std::vector<bool> seen(steps, false);
    for (const anim::Phase& p : plan.phases) {
        if (p.step < 0 || static_cast<std::size_t>(p.step) >= steps) continue;
        Window& win = w[static_cast<std::size_t>(p.step)];
        if (!seen[static_cast<std::size_t>(p.step)]) {
            win.start = p.start;
            seen[static_cast<std::size_t>(p.step)] = true;
        }
        if (p.kind == anim::Phase::Kind::Clear) win.clear = p.start;
        win.end = std::max(win.end, p.start + p.duration);
    }
    return w;
}

// How long each totem's own beat would like to last, before clamping.
inline double totem_seconds(int id) {
    switch (id) {
        case kTime:
        case kGold: return feel::kLabelSeconds;
        default: return feel::kTotemLeadSeconds + feel::kPowerUpBurstSeconds;
    }
}

inline double power_up_seconds(int id) {
    return id == kClock || id == kMultiplier ? feel::kLabelSeconds : feel::kPowerUpBurstSeconds;
}

// Every beat of a move, in step order. Transient beats are clamped to end by
// the end of their step (readability rule 3); a marker popping in is not
// transient (the marker stays) and is never clamped.
inline std::vector<Beat> build_beats(const anim::Plan& plan, const std::vector<StepFx>& steps) {
    std::vector<Beat> beats;
    const std::vector<Window> windows = step_windows(plan, steps.size());
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const StepFx& s = steps[i];
        const Window& w = windows[i];
        const int step = static_cast<int>(i);
        auto transient = [&](Beat::Kind kind, int id, double start, double want, Cell cell, std::vector<Cell> cells,
                             double value) {
            Beat b;
            b.kind = kind;
            b.id = id;
            b.step = step;
            b.start = start;
            b.duration = std::max(0.0, std::min(want, w.end - start));
            b.cell = cell;
            b.cells = std::move(cells);
            b.value = value;
            beats.push_back(std::move(b));
        };
        auto placed = [&](double start, Cell cell, int m) {
            Beat b;
            b.kind = Beat::Kind::MarkPlaced;
            b.id = m;
            b.step = step;
            b.start = start;
            b.duration = feel::kMarkPopSeconds;
            b.cell = cell;
            beats.push_back(b);
        };
        if (s.totem) {
            const TotemFire& t = *s.totem;
            std::vector<Cell> cells = t.targets;
            for (const anim::Repaint& rp : t.repaints) cells.push_back(rp.cell);
            for (const Cell& c : t.gems_harvested) cells.push_back(c);
            for (const Placement& p : t.power_ups_placed) cells.push_back(p.cell);
            const double value = t.id == kTime ? t.time_seconds : static_cast<double>(t.points);
            transient(Beat::Kind::Totem, t.id, w.start, totem_seconds(t.id), Cell{-1, -1}, std::move(cells), value);
            for (const Cell& c : t.gems_harvested)
                transient(Beat::Kind::GemCollect, -1, w.clear, feel::kGemFlySeconds, c, {c}, 0.0);
            for (const Cell& c : t.chains_removed)
                transient(Beat::Kind::ChainBreak, -1, w.clear, feel::kBreakSeconds, c, {c}, 0.0);
            for (const Cell& c : t.gems_placed) placed(w.clear, c, marks::kGem);
            for (const Placement& p : t.power_ups_placed) placed(w.clear, p.cell, marks::with_power_up(0, p.id));
        }
        for (const PowerUpGoOff& p : s.power_ups) {
            const double value = p.id == kClock ? p.time_seconds : p.id == kMultiplier ? p.multiplier : p.hint_seconds;
            transient(Beat::Kind::PowerUp, p.id, w.clear, power_up_seconds(p.id), p.cell, p.cells, value);
        }
        for (const Cell& c : s.gems_collected)
            transient(Beat::Kind::GemCollect, -1, w.clear, feel::kGemFlySeconds, c, {c}, 0.0);
        for (const Cell& c : s.steel_removed)
            transient(Beat::Kind::SteelBreak, -1, w.clear, feel::kBreakSeconds, c, {c}, 0.0);
        for (const Cell& c : s.chains_broken)
            transient(Beat::Kind::ChainBreak, -1, w.clear, feel::kBreakSeconds, c, {c}, 0.0);
        for (const Placement& p : s.power_ups_placed) placed(w.end, p.cell, marks::with_power_up(0, p.id));
        for (const Cell& c : s.gems_placed) placed(w.end, c, marks::kGem);
    }
    return beats;
}

// Tracks piece markers through anim::walk, so they move with their pieces.
// Slot is whatever the caller walks with; `marks_of(slot)` gives its marks
// as a mutable int. The caller forwards the walk's optional hooks here.
struct MarkTracker {
    const std::vector<StepFx>* steps = nullptr;
    int width = 0;

    const StepFx* at(int step) const {
        if (!steps || step < 0 || static_cast<std::size_t>(step) >= steps->size()) return nullptr;
        return &(*steps)[static_cast<std::size_t>(step)];
    }

    // As a step's clear starts: markers the step removes go, a totem's own
    // placements arrive. `set(cell, marks)` applies each change.
    template <class Set>
    void before_clear(int step, Set&& set) const {
        const StepFx* s = at(step);
        if (!s) return;
        auto drop = [&](const Cell& c, int flag) { set(c, flag, false); };
        for (const Cell& c : s->gems_collected) drop(c, marks::kGem);
        for (const Cell& c : s->steel_removed) drop(c, marks::kSteel);
        for (const Cell& c : s->chains_broken) drop(c, marks::kChain);
        if (s->totem) {
            for (const Cell& c : s->totem->gems_harvested) drop(c, marks::kGem);
            for (const Cell& c : s->totem->chains_removed) drop(c, marks::kChain);
            for (const Cell& c : s->totem->gems_placed) set(c, marks::kGem, true);
            for (const Placement& p : s->totem->power_ups_placed) set(p.cell, marks::with_power_up(0, p.id), true);
        }
    }

    // After a step's refill: the markers it placed.
    template <class Set>
    void after_fall(int step, Set&& set) const {
        const StepFx* s = at(step);
        if (!s) return;
        for (const Placement& p : s->power_ups_placed) set(p.cell, marks::with_power_up(0, p.id), true);
        for (const Cell& c : s->gems_placed) set(c, marks::kGem, true);
    }

    // Marks a piece spawned in this step starts with.
    int spawn_marks(int step, Cell to) const {
        const StepFx* s = at(step);
        if (!s) return marks::kNone;
        for (const Cell& c : s->steel_spawned)
            if (c == to) return marks::kSteel;
        return marks::kNone;
    }

    // Applies one change to a marks value: a flag on or off, or a power-up
    // badge (with_power_up) put on.
    static int apply(int m, int change, bool on) {
        if (marks::power_up(change) >= 0) return marks::with_power_up(m, on ? marks::power_up(change) : -1);
        return on ? (m | change) : (m & ~change);
    }
};

}  // namespace montezuma::godot_binding::fx
