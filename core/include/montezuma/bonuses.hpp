// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/rng.hpp"

namespace montezuma {

// Bonus ids from the contracts doc ("Totem and bonus ids"). The enum order is
// the stable index used by BonusInventory and the serialised form; append new
// ids at the end, never reorder.
enum class BonusId { Hammer, Lightning, ExtraTime, Shuffle, Dynamite, Freeze };

inline constexpr std::size_t kBonusCount = 6;
inline constexpr int kMinBonusLevel = 1;
inline constexpr int kMaxBonusLevel = 3;  // [verify] placeholder until WP-03

// Fewest pieces of one kind a cascade step must clear to earn any pickup.
// The rules spec says "a match of 4 or more" ([verify] there too).
inline constexpr int kBonusEarnThreshold = 4;

// Contract id string: "hammer", "lightning", "extra_time", "shuffle",
// "dynamite", "freeze".
std::string_view bonus_name(BonusId id);
std::optional<BonusId> bonus_from_name(std::string_view name);

// How a targeted bonus picks its cells.
enum class BonusShape {
    None,     // untargeted: time, freeze or reshuffle
    Diamond,  // cells within Manhattan distance `extent` of the target (hammer)
    Line,     // the target's row or column, `extent` lines either side (lightning)
    Square,   // cells within Chebyshev distance `extent` of the target (dynamite)
};

// Effect parameters at one upgrade level.
struct BonusLevelParams {
    int max_stock = 0;          // how many of this bonus the player may hold
    int extent = 0;             // shape size, see BonusShape
    bool both_axes = false;     // Line only: clear the row and the column
    double time_seconds = 0.0;  // ExtraTime: seconds added to the level timer
    double freeze_seconds = 0.0;  // Freeze: seconds the timer stops
    bool reshuffle = false;     // Shuffle: reorder the board's pieces
};

// One row of the tuning table: how a bonus is earned and what it does.
struct BonusDef {
    BonusId id;
    BonusShape shape;
    // Earned from a cascade step that clears at least this many of one kind.
    int earn_min_cleared;
    // Relative chance among the bonuses eligible for a pickup.
    int earn_weight;
    // Index 0 is level 1.
    std::array<BonusLevelParams, kMaxBonusLevel> levels;

    bool targeted() const { return shape != BonusShape::None; }
    const BonusLevelParams& at_level(int level) const;
};

// The whole tuning table, in BonusId order, and one row of it.
std::span<const BonusDef, kBonusCount> bonus_defs();
const BonusDef& bonus_def(BonusId id);

enum class Axis { Row, Column };

// Where a targeted bonus is aimed. Untargeted bonuses ignore it.
struct BonusTarget {
    Cell cell{};
    Axis axis = Axis::Row;  // Lightning only
};

// What using a bonus should do. Computed from the board but never applied to
// it: the game layer clears the cells (scoring and refilling them like a
// match), changes the timer and reshuffles.
struct Effect {
    BonusId bonus = BonusId::Hammer;
    std::vector<Cell> cells_to_clear;  // row-major order, unique, in bounds
    double time_delta_seconds = 0.0;
    double freeze_seconds = 0.0;
    bool reshuffle = false;
};

// The effect of `id` at `level` aimed at `target`, clipped to the board.
// Does not touch any inventory; useful to preview a target. `level` is
// clamped to [kMinBonusLevel, kMaxBonusLevel].
Effect bonus_effect(BonusId id, int level, const Board& board, BonusTarget target = {});

enum class UseError {
    None,
    NoStock,            // none of this bonus held
    TargetOutOfBounds,  // targeted bonus aimed outside the board
};

struct UseResult {
    std::optional<Effect> effect;  // set exactly when error == None
    UseError error = UseError::None;
    explicit operator bool() const { return effect.has_value(); }
};

// The player's bonuses: plain data so the progress package can save it
// between levels. Invariant: 0 <= stock[i] <= cap and
// kMinBonusLevel <= level[i] <= kMaxBonusLevel.
struct BonusInventory {
    std::array<int, kBonusCount> stock{};
    std::array<int, kBonusCount> level{1, 1, 1, 1, 1, 1};

    bool operator==(const BonusInventory&) const = default;

    int count(BonusId id) const { return stock[index(id)]; }
    int level_of(BonusId id) const { return level[index(id)]; }
    // Max stock at the current upgrade level.
    int cap(BonusId id) const;

    // Adds one pickup. False (and no change) when already at the cap.
    bool add(BonusId id);

    // Awards pickups for one legal move: one per (cascade step, kind) that
    // cleared at least kBonusEarnThreshold pieces, chosen by weight among the
    // bonuses that step qualifies for and that are below their cap. Returns
    // the awarded ids in award order (for bonus_earned / bonus_changed).
    std::vector<BonusId> earn_from(const MoveResult& result, Rng& rng);

    // Spends one `id` and returns its effect. On error nothing changes.
    UseResult use(BonusId id, const Board& board, BonusTarget target = {});

    // Raises the upgrade level by one (between levels; cost is the progress
    // package's concern). False at kMaxBonusLevel.
    bool upgrade(BonusId id);

    // Text form, one line per bonus: "<name> <stock> <level>\n", in BonusId
    // order. from_text accepts lines in any order, requires every id once and
    // validates the invariant; it throws std::invalid_argument otherwise.
    std::string to_text() const;
    static BonusInventory from_text(std::string_view text);
    static std::optional<BonusInventory> try_from_text(std::string_view text) noexcept;

    static std::size_t index(BonusId id) { return static_cast<std::size_t>(id); }
};

}  // namespace montezuma
