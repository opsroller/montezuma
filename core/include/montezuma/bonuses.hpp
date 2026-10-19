// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// On-board power-ups (WP-07), per docs/reference/rules-power-ups.md
// (PWR-01..11). A power-up sits on a plain piece, moves with it through
// falls and shuffles, and goes off when that piece is cleared. Its cells join
// the same step's clear. This header computes everything as data; the board
// and the game layer apply it. See docs/reference/bonuses.md.

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/rng.hpp"

namespace montezuma {

// Power-up ids. The enum order is the stable index used by PowerUpTiers and
// the defs table; append new ids at the end, never reorder.
enum class PowerUpId { Tnt, ColorClear, Lightning, Clock, Multiplier, Hint };

inline constexpr std::size_t kPowerUpCount = 6;
inline constexpr int kMinPowerUpTier = 1;
inline constexpr int kMaxPowerUpTier = 3;

// rule: PWR-01 power-ups earned per cleared run of 4, and of 5 or more.
inline constexpr int kPowerUpsPerRunOf4 = 1;
inline constexpr int kPowerUpsPerRunOf5 = 2;
// rule: PWR-02 at this many on the board, earned power-ups are not placed.
inline constexpr int kMaxPowerUpsOnBoard = 4;
// rule: PWR-02, TOT-15 the cap for the gift totem's own placements.
inline constexpr int kMaxPowerUpsOnBoardGift = 8;

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
    // && |dx| + |dy| <= area_manhattan. -1 means no area. Includes steel.
    int area_chebyshev = -1;
    int area_manhattan = 0;
    // Lines from the cell (PowerUpRay mask). Each ray includes the cell.
    // Includes steel.
    unsigned rays = kRayNone;
    // Every non-steel piece of the power-up's piece's kind.
    bool kind_clear = false;
    // Also that kind's steel pieces.
    bool kind_steel = false;
    // Also every non-steel piece orthogonally next to a cleared one.
    bool kind_neighbours = false;
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
// neighbours only with kind_clear). The built-in table is checked at compile
// time.
void validate_power_up_defs(std::span<const PowerUpDef> defs);

// The player's tier for each power-up, by PowerUpId index: 0 = not unlocked,
// 1..3. A runtime input from player progress (WP-08).
using PowerUpTiers = std::array<int, kPowerUpCount>;
// Every power-up unlocked at tier 1: the default for tests, the sim and the
// web demo until progress supplies the real tiers.
inline constexpr PowerUpTiers kAllPowerUpsTier1{1, 1, 1, 1, 1, 1};

// Throws std::invalid_argument when a tier is outside [0, 3].
void validate_power_up_tiers(const PowerUpTiers& tiers);

// Piece state the Board does not model yet, row-major (y * width + x). An
// empty vector means no piece has the mark.
struct PieceMarks {
    std::vector<bool> steel;  // OBS-01
    std::vector<bool> gem;    // GEM-01

    bool is_steel(const Board& board, Cell c) const noexcept;
    bool has_gem(const Board& board, Cell c) const noexcept;
};

// A power-up on a piece. The piece's kind is read from the board when it
// goes off (the paint totem can repaint it, TOT-16).
struct PowerUp {
    PowerUpId id = PowerUpId::Tnt;
    int tier = kMinPowerUpTier;
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

// What a triggered power-up does. Computed from the board but never applied.
struct PowerUpEffect {
    PowerUpId id = PowerUpId::Tnt;
    int tier = kMinPowerUpTier;
    // Occupied, in bounds, row-major, no duplicates. Always holds the
    // power-up's own cell when it is in bounds and occupied.
    std::vector<Cell> cells_to_clear;
    double time_delta_seconds = 0.0;
    // Multiplies the move's points (see move_score_multiplier); 1 = unchanged.
    int score_multiplier = 1;
    // Highlight every legal swap for this long; 0 means no hint.
    double hint_seconds = 0.0;
    bool operator==(const PowerUpEffect&) const = default;
};

// The effect of `id` at `tier` going off at `at`, on the board as it is just
// before the step's clear (the piece is still there; color_clear reads its
// kind). `tier` is clamped to [1, 3]; an out-of-bounds `at` gives no cells.
// Never throws for game reasons.
PowerUpEffect power_up_effect(PowerUpId id, int tier, const Board& board, Cell at,
                              const PieceMarks& marks = {}) noexcept;
PowerUpEffect power_up_effect(const TriggeredPowerUp& triggered, const Board& board,
                              const PieceMarks& marks = {}) noexcept;

// rule: PWR-10 several multipliers in one move do not stack: the highest
// applies. 1 when none went off.
int move_score_multiplier(std::span<const PowerUpEffect> effects) noexcept;

// rule: PWR-01 power-ups earned by one step's runs: 1 per run of 4, 2 per run
// of 5+. An L or T counts as its two runs (find_runs lists both).
int power_ups_earned_by(std::span<const Run> runs) noexcept;

// One step's clear after power-ups have been added to it (PWR-04).
struct StepClear {
    // Run cells plus every power-up's cells: occupied, row-major, unique.
    std::vector<Cell> cells;
    // The cells only power-ups added (not in a run), row-major. They collect,
    // score and fill Frenzy but do not charge totems (PWR-05).
    std::vector<Cell> power_up_cells;
    // In the order they went off: first those on run cells (row-major), then
    // those each later effect reached.
    std::vector<TriggeredPowerUp> triggered;
    // Pieces the clear reached that stay in place, row-major: chained
    // pieces lose their chain instead (OBS-05). Not in `cells`; a power-up
    // on one does not go off.
    std::vector<Cell> held;
    // Parallel to `triggered`.
    std::vector<PowerUpEffect> effects;
};

// Which cells hold a power-up, plus power-ups earned but waiting for a plain
// piece. Plain data (public members, operator==), so it can be saved with a
// level in progress. A layer is per level (PWR-03): start a new one.
//
// swap_with_power_ups and clear_cells_with_power_ups below drive a move
// through Board's StepHooks. Per step they do:
//   1. extend_clear: sc = layer.expand_clear(board, cleared, step)
//   2. the board clears sc.cells, falls and refills
//   3. after_refill: layer.apply_step(step); layer.place_for_step(...)
// then, after the move, layer.apply_shuffle(result, board, rng).
struct PowerUpLayer {
    int width = 0;
    int height = 0;
    std::vector<std::optional<PowerUp>> cells;
    // rule: PWR-02 earned power-ups that found no plain piece, oldest first;
    // they are placed after the next refill. An empty id is drawn then.
    std::vector<std::optional<PowerUpId>> pending;

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
    // Removes every power-up and every pending one.
    void clear() noexcept;
    // Power-ups on the board (not pending).
    int count() const noexcept;
    // Every power-up, row-major.
    std::vector<PowerUpPlacement> entries() const;

    // rule: PWR-04, RES-02 the step's full clear: starts from `run_cells` on
    // `board` before they are cleared, adds the cells of every power-up on a
    // cleared piece, and repeats for power-ups those cells reach until none
    // is new. `step` is recorded in each TriggeredPowerUp. Does not change
    // the layer or the board; apply_step removes the power-ups.
    // `holds`, row-major like the board, marks pieces a clear only unchains
    // (OBS-05): they go to StepClear::held. Empty: none.
    StepClear expand_clear(const Board& board, std::span<const Cell> run_cells, int step,
                           const PieceMarks& marks = {}, const std::vector<bool>& holds = {}) const;

    // rule: PWR-03 moves power-ups with their pieces: the swap exchanges two
    // cells' entries; a step removes power-ups on cleared cells and moves
    // those on falling pieces. Cells outside the layer are ignored.
    void apply_swap(Cell a, Cell b) noexcept;
    void apply_step(const CascadeStep& step) noexcept;

    // rule: SHUF-03, GEM-05, PWR-03 after a shuffle, power-ups stay on their
    // pieces (`shuffle_map`). After a redeal, which no map describes, the
    // same power-ups are put back on random plain pieces of the new board:
    // per power-up in row-major order of the old layer, one
    // rng.below(plain pieces). Ones that find no plain piece wait in
    // `pending`. Does nothing unless `result.shuffled`.
    void apply_shuffle(const MoveResult& result, const Board& board, Rng& rng, const PieceMarks& marks = {});

    // apply_swap, apply_step for each step, then apply_shuffle: replays a
    // whole result (for example a clear_cells effect) on the layer.
    void remap(const MoveResult& result, const Board& board, Rng& rng, const PieceMarks& marks = {});

    // Power-ups whose pieces `result` cleared, following each piece through
    // the swap and the falls of earlier steps. Ordered by step, then by
    // CascadeStep::cleared order (row-major). Call before remap.
    std::vector<TriggeredPowerUp> triggered_by(const MoveResult& result) const;

    // rule: PWR-01, PWR-02, RES-05 after one step's refill: places
    // the `pending` power-ups, then those `runs` earn. Each takes the next
    // pending id or draws one with rng.below(unlocked count) over unlocked
    // ids (tier >= 1) in PowerUpId order, then a cell with
    // rng.below(plain count) over plain pieces in row-major order. A plain
    // piece is occupied, not steel, with no gem and no power-up (GEM-01,
    // OBS-04). At `max_on_board` power-ups the rest are dropped without
    // drawing; with no plain piece left they wait in `pending`. Nothing is
    // drawn or kept when no power-up is unlocked. Returns the placements in
    // order. Throws std::invalid_argument for a bad `tiers` table or a layer
    // whose size differs from the board's.
    std::vector<PowerUpPlacement> place_for_step(std::span<const Run> runs, const Board& board, Rng& rng,
                                                 const PowerUpTiers& tiers, const PieceMarks& marks = {},
                                                 int max_on_board = kMaxPowerUpsOnBoard);

    // rule: TOT-15 places up to `count` new power-ups the same way, without
    // pending: ones that find no plain piece or meet `max_on_board` are
    // dropped. For the gift totem (max_on_board = kMaxPowerUpsOnBoardGift).
    // Throws like place_for_step.
    std::vector<PowerUpPlacement> place_random(int count, const Board& board, Rng& rng, const PowerUpTiers& tiers,
                                               const PieceMarks& marks = {},
                                               int max_on_board = kMaxPowerUpsOnBoardGift);

private:
    std::size_t index(Cell c) const noexcept {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(c.x);
    }
    void check_size(const Board& board) const;
    std::vector<Cell> plain_cells(const Board& board, const PieceMarks& marks) const;
    // Places `wanted` (fixed or drawn ids) in order; returns placements and
    // the ones left over for lack of a plain piece.
    std::vector<PowerUpPlacement> place(std::vector<std::optional<PowerUpId>> wanted, const Board& board, Rng& rng,
                                        const PowerUpTiers& tiers, const PieceMarks& marks, int max_on_board,
                                        std::vector<std::optional<PowerUpId>>* left_over);
};

// What power-ups did in one cascade step of a move.
struct PowerUpStep {
    StepClear clear;
    // Placed after this step's refill (PWR-01, RES-05).
    std::vector<PowerUpPlacement> placed;
};

// What power-ups did during one move.
struct PowerUpMoveReport {
    // One per MoveResult::cascades entry.
    std::vector<PowerUpStep> steps;
    // rule: PWR-10 the highest multiplier that went off; 1 if none. Applies
    // to the whole move's points, after Frenzy doubling (SCORE-04).
    int score_multiplier = 1;
    // rule: PWR-09, GOAL-05 seconds every clock that went off adds, summed.
    double time_delta_seconds = 0.0;
    // rule: PWR-11 the hint duration to (re)start: the last hint that went
    // off, since a hint restarts a running one; 0 if none.
    double hint_seconds = 0.0;
};

// A legal swap resolved with power-ups (PWR-01..05): every step's clear is
// extended by the power-ups it reaches, and earned power-ups are placed after
// each step's refill. Empty for an illegal swap: board, layer and rng are
// unchanged and `report` is reset. `rng` is the level's generator for
// power-up draws; the board draws refills from its own. `report` is
// overwritten. `marks` are read as given for every step (Board does not move
// steel or gems yet). Throws std::invalid_argument, before touching the board
// or the layer, for a bad `tiers` table or a layer whose size differs from
// the board's.
std::optional<MoveResult> swap_with_power_ups(Board& board, PowerUpLayer& layer, Cell a, Cell b, Rng& rng,
                                              const PowerUpTiers& tiers, PowerUpMoveReport& report,
                                              const PieceMarks& marks = {});

// The same for an effect's clear (a totem, RES-07): step 0 clears `cells`
// plus the power-ups they reach; its cascades earn power-ups like any step.
std::optional<MoveResult> clear_cells_with_power_ups(Board& board, PowerUpLayer& layer,
                                                     const std::vector<Cell>& cells, Rng& rng,
                                                     const PowerUpTiers& tiers, PowerUpMoveReport& report,
                                                     const PieceMarks& marks = {});

}  // namespace montezuma
