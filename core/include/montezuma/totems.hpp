// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/rng.hpp"

namespace montezuma {

// Power Totems (docs/rules-spec.md, "Power Totems"). Ids follow the contract
// in docs/reference/contracts.md; the enum order is the stable order in which
// totems are charged, fired and reported.
enum class TotemId { RowClear, RandomBlast, Time, Lightning, ColorBomb, Multiplier };
inline constexpr std::size_t kTotemCount = 6;

// Contract id string ("row_clear", ...) for a totem.
std::string_view totem_name(TotemId id);
// Inverse of totem_name; empty for an unknown id.
std::optional<TotemId> totem_from_name(std::string_view name);

// Shape of the lightning totem's strike.
enum class LightningShape {
    Column,  // one whole column
    Cross,   // one whole row plus one whole column through a centre cell
};

// Tuning for one totem. Only the fields its effect reads matter; the rest
// keep their neutral defaults.
struct TotemDef {
    TotemId id;
    // Consecutive same-kind matches (the swap's own match) needed to fire.
    int threshold = 3;
    // random_blast: pieces destroyed (capped at the pieces on the board).
    int blast_count = 0;
    // time: seconds added to the level timer.
    double time_delta_seconds = 0.0;
    // lightning: strike shape.
    LightningShape lightning_shape = LightningShape::Column;
    // multiplier: score factor and how many following moves it lasts.
    int score_multiplier = 1;
    int multiplier_moves = 0;
};

using TotemTable = std::array<TotemDef, kTotemCount>;

// Default tuning, one row per totem in TotemId order. The spec gives no
// numbers yet, so every value is a placeholder.
inline constexpr TotemTable kTotemDefs{{
    // [verify] placeholder until WP-03: threshold; row chosen at random.
    {.id = TotemId::RowClear, .threshold = 3},
    // [verify] placeholder until WP-03: threshold, blast_count.
    {.id = TotemId::RandomBlast, .threshold = 4, .blast_count = 8},
    // [verify] placeholder until WP-03: threshold, time_delta_seconds.
    {.id = TotemId::Time, .threshold = 3, .time_delta_seconds = 10.0},
    // [verify] placeholder until WP-03: threshold, lightning_shape.
    {.id = TotemId::Lightning, .threshold = 5, .lightning_shape = LightningShape::Cross},
    // [verify] placeholder until WP-03: threshold; targets the streak's kind.
    {.id = TotemId::ColorBomb, .threshold = 6},
    // [verify] placeholder until WP-03: threshold, score_multiplier, multiplier_moves.
    {.id = TotemId::Multiplier, .threshold = 4, .score_multiplier = 2, .multiplier_moves = 3},
}};

// What a fired totem does, as data. Nothing here has touched the board: the
// game layer clears `cells_to_clear` through the board's clear-and-cascade
// API, adds `time_delta_seconds` to the timer and applies the multiplier.
struct TotemEffect {
    TotemId totem = TotemId::RowClear;
    // Unique, in bounds, sorted row-major (y, then x).
    std::vector<Cell> cells_to_clear;
    double time_delta_seconds = 0.0;
    int score_multiplier = 1;
    int multiplier_moves = 0;
};

// Per-level totem charge. Pure state: fed the kind of each move's own match,
// it charges unlocked totems along the same-kind streak and reports which
// ones fired. Locked totems never charge.
class TotemSystem {
public:
    // Throws std::invalid_argument if `defs` is out of order or a value is
    // out of range (threshold < 1, blast_count < 0, score_multiplier < 1,
    // multiplier_moves < 0, time_delta_seconds not finite).
    explicit TotemSystem(const std::vector<TotemId>& unlocked = {}, const TotemTable& defs = kTotemDefs);
    // From LevelData's `totems_unlocked` strings. Throws std::invalid_argument
    // naming an unknown id.
    static TotemSystem from_names(const std::vector<std::string_view>& unlocked, const TotemTable& defs = kTotemDefs);

    bool unlocked(TotemId id) const { return unlocked_[index(id)]; }
    // Unlocking keeps the current streak; the totem charges from the next match.
    void unlock(TotemId id) { unlocked_[index(id)] = true; }
    const TotemDef& def(TotemId id) const { return defs_[index(id)]; }

    // Same-kind matches counted toward this totem since the streak began or
    // the totem last fired. Always below the threshold between calls.
    int charge_count(TotemId id) const { return charge_[index(id)]; }
    // charge_count / threshold, in [0, 1); 0 for a locked totem. Feeds the
    // HUD's totem_charge_changed signal.
    double charge(TotemId id) const;

    int streak_kind() const { return streak_kind_; }
    int streak() const { return streak_; }

    // Records one move's own match. The same kind as the last match extends
    // the streak; another kind starts a new streak of 1, resetting every
    // charge to that 1; a negative kind (no match) clears the streak and all
    // charges. Each unlocked totem whose charge reaches its threshold fires
    // and drops to 0. Returns the fired totems in TotemId order.
    std::vector<TotemId> record_match(int kind);

    // Builds a fired totem's effect against the board as it is after the
    // move resolved. Target choices draw from `rng` in a fixed order.
    // `target_kind` is the kind that charged it (color_bomb aims at it).
    TotemEffect make_effect(TotemId id, const Board& board, Rng& rng, int target_kind) const;

    // record_match(result.matched_kind), then make_effect for each fired
    // totem, in TotemId order.
    std::vector<TotemEffect> apply_move(const MoveResult& result, const Board& board, Rng& rng);

private:
    static std::size_t index(TotemId id) { return static_cast<std::size_t>(id); }

    TotemTable defs_;
    std::array<bool, kTotemCount> unlocked_{};
    std::array<int, kTotemCount> charge_{};
    int streak_kind_ = -1;
    int streak_ = 0;
};

}  // namespace montezuma
