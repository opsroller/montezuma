// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// On-board power-ups (WP-07). A power-up sits on a board piece, moves with it
// through falls and shuffles, and triggers when that piece is cleared. This
// header computes everything as data; the game layer applies it (clears
// cells through Board::clear_cells, changes the timer, scales the score,
// shows hints). See docs/reference/bonuses.md.

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/rng.hpp"

namespace montezuma {

// Power-up ids. The enum order is the stable index used by PowerUpSet,
// PowerUpTiers and the defs table; append new ids at the end, never reorder.
enum class PowerUpId { Tnt, ColorClear, Lightning, Clock, Multiplier, Hint };

inline constexpr std::size_t kPowerUpCount = 6;
inline constexpr int kMinPowerUpTier = 1;
inline constexpr int kMaxPowerUpTier = 3;

constexpr std::size_t power_up_index(PowerUpId id) { return static_cast<std::size_t>(id); }

// Contract id string: "tnt", "color_clear", "lightning", "clock",
// "multiplier", "hint". Case-sensitive.
std::string_view power_up_name(PowerUpId id) noexcept;
std::optional<PowerUpId> power_up_from_name(std::string_view name) noexcept;

// Straight lines a power-up clears from its cell, as a bit mask.
enum PowerUpRay : unsigned {
    kRayNone = 0,
    kRayUp = 1u << 0,
    kRayDown = 1u << 1,
    kRayLeft = 1u << 2,
    kRayRight = 1u << 3,
    kRayAll = kRayUp | kRayDown | kRayLeft | kRayRight,
};

// What a power-up does at one tier. Fields that do not apply keep their
// defaults; the effect is the union of every part that is set.
struct PowerUpTierParams {
    // Area around the cell: |dx| <= area_chebyshev && |dy| <= area_chebyshev
    // && |dx| + |dy| <= area_manhattan. -1 means no area.
    int area_chebyshev = -1;
    int area_manhattan = 0;
    // Lines from the cell (PowerUpRay mask). Each ray includes the cell.
    unsigned rays = kRayNone;
    // Every piece of the triggering piece's colour.
    bool colour = false;
    // Also the colour's steel pieces (reported as a flag, see PowerUpEffect).
    bool colour_steel = false;
    // Also every piece orthogonally next to a colour cell.
    bool colour_adjacent = false;
    double time_seconds = 0.0;
    int score_multiplier = 1;
    double hint_seconds = 0.0;
};

// One row of the defs table.
struct PowerUpDef {
    PowerUpId id;
    std::string_view name;
    // Index 0 is tier 1.
    std::array<PowerUpTierParams, kMaxPowerUpTier> tiers;

    // `tier` is clamped to [kMinPowerUpTier, kMaxPowerUpTier].
    const PowerUpTierParams& at_tier(int tier) const noexcept;
};

// The defs table, in PowerUpId order, and one row of it.
std::span<const PowerUpDef, kPowerUpCount> power_up_defs() noexcept;
const PowerUpDef& power_up_def(PowerUpId id) noexcept;

// Throws std::invalid_argument unless `defs` has every id once in PowerUpId
// order with a non-empty unique name, and every tier is well formed (area
// sizes >= -1/0, known ray bits, multiplier >= 1, times >= 0, steel and
// adjacency only with colour). The built-in table is checked at compile time.
void validate_power_up_defs(std::span<const PowerUpDef> defs);

// Which power-ups the player has unlocked, by PowerUpId index.
using PowerUpSet = std::array<bool, kPowerUpCount>;
// The tier the player owns for each power-up, by PowerUpId index. Upgrades
// (golden stars) belong to WP-08; here the tier is only an input.
using PowerUpTiers = std::array<int, kPowerUpCount>;

// Throws std::invalid_argument when a tier is outside [1, 3].
void validate_power_up_tiers(const PowerUpTiers& tiers);

// A power-up on a piece. `kind` is the colour of the piece it sits on,
// recorded at placement (pieces never change colour); color_clear uses it
// because by the time the effect is computed the piece is gone.
struct PowerUp {
    PowerUpId id = PowerUpId::Tnt;
    int tier = kMinPowerUpTier;
    int kind = Board::kEmpty;
    bool operator==(const PowerUp&) const = default;
};

struct PowerUpPlacement {
    Cell cell;
    PowerUp power_up;
    bool operator==(const PowerUpPlacement&) const = default;
};

// A power-up whose piece was cleared. `cell` is where the piece was when it
// was cleared, `step` the index into MoveResult::cascades that cleared it.
struct TriggeredPowerUp {
    Cell cell;
    PowerUp power_up;
    int step = 0;
    bool operator==(const TriggeredPowerUp&) const = default;
};

// What a triggered power-up should do. Computed from the board but never
// applied to it.
struct PowerUpEffect {
    PowerUpId id = PowerUpId::Tnt;
    int tier = kMinPowerUpTier;
    // Occupied, in bounds, row-major, no duplicates. Pass to
    // Board::clear_cells.
    std::vector<Cell> cells_to_clear;
    double time_delta_seconds = 0.0;
    // Multiplies the score of the move that triggered it; 1 means unchanged.
    int score_multiplier = 1;
    // Highlight every legal move for this long; 0 means no hint.
    double hint_seconds = 0.0;
    // Also clear the steel pieces of the colour (color_clear L2+). The board
    // has no steel pieces yet; the game layer acts on this flag.
    bool clear_colour_steel = false;
    bool operator==(const PowerUpEffect&) const = default;
};

// The effect of `id` at `tier` triggered at `at`. `colour` is the triggering
// piece's kind (TriggeredPowerUp::power_up.kind); when negative, the piece
// now at `at` is used. `tier` is clamped to [1, 3]; an out-of-bounds `at`
// gives no cells. Never throws for game reasons.
PowerUpEffect power_up_effect(PowerUpId id, int tier, const Board& board, Cell at,
                              int colour = Board::kEmpty) noexcept;
PowerUpEffect power_up_effect(const TriggeredPowerUp& triggered, const Board& board) noexcept;

// Which cells hold a power-up: plain data, one optional entry per cell in
// row-major order (y * width + x). A cell without one holds a plain piece
// (or nothing).
//
// Per move, the game layer does (see docs/reference/bonuses.md):
//   1. fired = layer.triggered_by(result);   // before remap
//   2. layer.remap(result);
//   3. for each fired power-up: e = power_up_effect(...); apply time,
//      multiplier and hint; if e.cells_to_clear is non-empty, r =
//      board.clear_cells(e.cells_to_clear) and repeat 1-3 with r. That is
//      how chained triggers resolve: a power-up whose cells hold another
//      power-up fires it through the next triggered_by.
//   4. layer.place_from_move(swap_result, board, rng, unlocked, tiers) once
//      the chain is done.
struct PowerUpLayer {
    int width = 0;
    int height = 0;
    std::vector<std::optional<PowerUp>> cells;

    PowerUpLayer() = default;
    // Throws std::invalid_argument when width or height is below 1.
    PowerUpLayer(int width, int height);
    explicit PowerUpLayer(const Board& board) : PowerUpLayer(board.width(), board.height()) {}

    bool operator==(const PowerUpLayer&) const = default;

    bool in_bounds(Cell c) const noexcept { return c.x >= 0 && c.y >= 0 && c.x < width && c.y < height; }
    // Empty for a cell without a power-up or out of bounds.
    std::optional<PowerUp> at(Cell c) const noexcept;
    bool has(Cell c) const noexcept { return at(c).has_value(); }
    // Out-of-bounds cells are ignored.
    void set(Cell c, PowerUp p) noexcept;
    void erase(Cell c) noexcept;
    void clear() noexcept;
    int count() const noexcept;
    // Every power-up, row-major.
    std::vector<PowerUpPlacement> entries() const;

    // Power-ups whose pieces `result` cleared, following each piece through
    // the swap and the falls of earlier steps. Ordered by step, then by
    // CascadeStep::cleared order (row-major). Call before remap.
    std::vector<TriggeredPowerUp> triggered_by(const MoveResult& result) const noexcept;

    // Moves power-ups with their pieces through `result`: the swap, then per
    // step the cleared pieces (their power-ups are removed) and the falls,
    // then the shuffle_map. A redeal drops every power-up. Cells outside the
    // layer are ignored.
    void remap(const MoveResult& result) noexcept;

    // Places power-ups earned by a legal swap and returns them in placement
    // order: one per run of exactly 4 and two per run of 5+, over every step
    // of `result`. Nothing for a clear_cells result (no swap). Each one picks
    // an id uniformly among `unlocked`, then a plain piece (occupied, no
    // power-up) uniformly in row-major order, drawing from `rng` in that
    // order. Stops early, without drawing, when no plain piece is left;
    // draws nothing when nothing is earned or nothing is unlocked.
    // Call after remap(result), on the board after the move.
    // Throws std::invalid_argument for a bad `tiers` table or a layer whose
    // size differs from the board's.
    std::vector<PowerUpPlacement> place_from_move(const MoveResult& result, const Board& board, Rng& rng,
                                                  const PowerUpSet& unlocked, const PowerUpTiers& tiers);

    // How many power-ups `result` earns, before capping by plain pieces.
    static int earned_by(const MoveResult& result) noexcept;

private:
    std::size_t index(Cell c) const noexcept {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(c.x);
    }
    // Walks `result`, moving entries in `layer`; appends to `fired` if set.
    static void walk(PowerUpLayer& layer, const MoveResult& result, std::vector<TriggeredPowerUp>* fired) noexcept;
};

}  // namespace montezuma
