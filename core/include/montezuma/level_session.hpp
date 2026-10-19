// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// One level played by the full classic rules (WP-22): the board, gems,
// steel and chains, power-ups, totems, Score Frenzy, scoring, the goal and
// the timer, in the in-move order RES-01..08 of docs/reference/rules-spec.md.
// Commands in (swap, tick, shuffle, buy_hint), one report out per command.
// Deterministic: every random draw comes from streams derived from the seed;
// no clock is read. See docs/reference/level-session.md.

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/bonuses.hpp"
#include "montezuma/level_data.hpp"
#include "montezuma/rng.hpp"
#include "montezuma/totems.hpp"

namespace montezuma {

// rule: GEM-02 gems on the board at gems tier 0; +1 per tier (progression).
inline constexpr int kBaseGemsOnBoard = 5;
inline constexpr int kMaxGemsTier = 3;
// rule: FRZ-02 Frenzy seconds by tier; tier 0 is locked (FRZ-01).
inline constexpr int kMaxFrenzyTier = 4;
inline constexpr std::array<double, kMaxFrenzyTier + 1> kFrenzySeconds{0.0, 15.0, 20.0, 25.0, 30.0};
// rule: FRZ-02 the bar fills to this many pieces.
inline constexpr int kFrenzyBarMax = 60;
// rule: SCORE-01, SCORE-02
inline constexpr int kPointsPerPiece = 10;
inline constexpr int kPointsPerGem = 50;
// rule: SCORE-03
inline constexpr int kFrenzyPointsFactor = 2;
// rule: OBS-02 steel spawns only while fewer than this many are on the board.
inline constexpr int kMaxSteelOnBoard = 8;
// AMBIGUOUS steel roll: steel when rng.below(kSteelRollScale) <
// round(steel_rate * kSteelRollScale).
inline constexpr int kSteelRollScale = 1000000;
// rule: SHUF-01, SHUF-02, SHUF-04, SHUF-05
inline constexpr int kManualShufflesPerLevel = 2;
inline constexpr double kAutoShufflePenaltySeconds = 5.0;
inline constexpr int kHintCost = 300;
inline constexpr double kFreeHintIdleSeconds = 8.0;

// What the player owns on reaching the level (progression.md Upgrades and
// Unlocks). Totem and power-up tiers apply only to ids the level lists; a
// listed id whose tier here is 0 is owned at tier 1 (PROG-03).
struct Upgrades {
    int timer_tier = 0;   // 0..kMaxTimerTier (GOAL-03)
    int gems_tier = 0;    // 0..kMaxGemsTier (gems_max = 5 + tier)
    int frenzy_tier = 0;  // 0 locked, 1..kMaxFrenzyTier
    std::array<int, kTotemCount> totem_tiers{};     // by TotemId
    std::array<int, kPowerUpCount> power_up_tiers{};  // by PowerUpId
};

using Swap = std::array<Cell, 2>;

// What happened in one cascade step, in display order (rules-spec "Order
// inside one moment").
struct StepReport {
    int step = 0;              // within the move: SCORE-01 uses step + 1
    std::vector<Run> runs;     // empty for an effect's first step
    std::vector<Cell> cleared;  // row-major; runs plus power-up cells
    std::vector<Cell> gems_collected;
    std::map<int, int> cleared_by_kind;
    int points = 0;            // after Frenzy doubling, before the multiplier
    bool frenzy = false;       // points were doubled (SCORE-03)
    int frenzy_bar_after = 0;
    bool frenzy_started = false;  // the bar filled in this step (FRZ-02)
    std::vector<TriggeredPowerUp> power_ups_triggered;
    std::vector<PowerUpEffect> power_up_effects;  // parallel to the above
    std::vector<Fall> falls;
    std::vector<Spawn> spawns;
    std::vector<Cell> steel_spawned;
    std::vector<Cell> gems_placed;
    std::vector<PowerUpPlacement> power_ups_placed;
    std::vector<Cell> chains_broken;  // OBS-05: unchained, the piece stayed (not in cleared)
    std::vector<Cell> steel_removed;
};

// A shuffle of the movable pieces (manual or automatic).
struct ShuffleReport {
    bool shuffled = false;
    bool redealt = false;  // no map: read the board, gems and power-ups anew
    std::vector<std::pair<Cell, Cell>> shuffle_map;
    bool stuck = false;    // nothing playable found; board unchanged
    double time_delta_seconds = 0.0;  // -5 for an automatic one (SHUF-02)
};

// One fired totem and everything its effect set off.
struct TotemFiring {
    TotemEffect effect;
    std::optional<MoveResult> result;  // its board clear, when it cleared any
    std::vector<StepReport> steps;
    int points = 0;                    // gold and harvest gem points, after doubling
    std::vector<Cell> gems_harvested;  // harvest: collected, pieces stay
    std::vector<Cell> gems_placed;     // harvest
    std::vector<Cell> chains_removed;  // harvest tier 3+
    std::vector<PowerUpPlacement> power_ups_placed;  // gift
    bool frenzy_started = false;
    bool frenzy_restarted = false;
};

struct TotemCharge {
    TotemId id = TotemId::Fire;
    double charge = 0.0;  // 0, 0.5, or 1 for a totem that fired this move
};

enum class Outcome { Playing, Won, Lost };

struct MoveReport {
    Swap swap{};
    MoveResult result;  // the board's own result for the swap
    std::vector<StepReport> steps;
    std::vector<TotemCharge> totem_charges;  // every owned totem, kind order
    std::vector<TotemFiring> totems_fired;   // kind order (RES-07)
    int base_points = 0;       // after Frenzy, before the multiplier
    int multiplier = 1;        // PWR-10: the highest in the whole move
    int points = 0;            // added to the score
    int gems_collected = 0;
    double time_delta_seconds = 0.0;  // clocks, time totem, reshuffle penalty
    double hint_seconds = 0.0;        // PWR-11, 0 when none
    bool frenzy_started = false;
    bool frenzy_restarted = false;
    int star_sand_gained = 0;
    Outcome outcome = Outcome::Playing;
    std::optional<ShuffleReport> auto_shuffle;  // SHUF-02
    bool stuck = false;
};

struct TickReport {
    double time_left = 0.0;
    double frenzy_left = 0.0;
    bool frenzy_ended = false;
    std::optional<Swap> free_hint;  // SHUF-05, presentation only
    Outcome outcome = Outcome::Playing;
    bool outcome_changed = false;
};

// Seconds to whole microseconds, rounded to nearest: the session's unit of
// game time.
std::int64_t to_micros(double seconds);

class LevelSession {
public:
    // Throws std::invalid_argument for a tier out of range, an unknown
    // difficulty, a level with blocked_cells (not supported yet), and
    // std::runtime_error when no playable board can be dealt.
    LevelSession(const LevelData& level, Difficulty difficulty, const Upgrades& upgrades, std::uint64_t seed);

    // --- Commands ---------------------------------------------------------
    // A legal swap and everything it sets off (RES-01..08). Empty, with
    // nothing changed, for an illegal swap or once the level is over.
    std::optional<MoveReport> swap(Cell a, Cell b);
    // Time passes (GOAL-04, FRZ-05). Throws std::invalid_argument for a
    // negative dt. Empty once the level is over.
    std::optional<TickReport> tick(double dt);
    // Manual shuffle (SHUF-01). Empty when none is left or the level is over.
    std::optional<ShuffleReport> shuffle();
    // SHUF-04: one legal move for kHintCost points; empty below that.
    std::optional<Swap> buy_hint();

    // --- Queries ----------------------------------------------------------
    // Every legal swap once, (a, b) with b right of or below a, row-major
    // by a, right before down. Steel and chain aware.
    std::vector<Swap> legal_moves() const;
    bool is_legal(Cell a, Cell b) const;
    // Every run on the board now, steel-aware (empty on a still board).
    std::vector<Run> runs() const;
    // The runs a swap would make in its step 0 (empty if illegal).
    std::vector<Run> runs_for_swap(Cell a, Cell b) const;

    const LevelData& level() const { return level_; }
    Difficulty difficulty() const { return difficulty_; }
    const Board& board() const { return board_; }
    const PowerUpLayer& power_ups() const { return power_ups_; }
    const TotemSystem& totems() const { return totems_; }
    const PowerUpTiers& power_up_tiers() const { return power_up_tiers_; }
    bool steel(Cell c) const { return mark(marks_.steel, c); }
    bool gem(Cell c) const { return mark(marks_.gem, c); }
    bool chained(Cell c) const { return mark(chains_, c); }
    const std::vector<bool>& steel_marks() const { return marks_.steel; }
    const std::vector<bool>& gem_marks() const { return marks_.gem; }
    const std::vector<bool>& chain_marks() const { return chains_; }
    int gems_on_board() const;
    int steel_on_board() const;
    int gems_max() const { return gems_max_; }
    int gems_collected() const { return gems_collected_; }
    std::optional<int> gem_goal() const { return gem_goal_; }
    const std::map<int, int>& quotas() const { return level_.quotas; }
    const std::map<int, int>& cleared_by_kind() const { return cleared_by_kind_; }
    double time_left() const { return static_cast<double>(time_left_us_) / 1e6; }
    int score() const { return score_; }
    int frenzy_tier() const { return frenzy_tier_; }
    int frenzy_bar() const { return frenzy_bar_; }
    bool frenzy_on() const { return frenzy_on_; }
    double frenzy_left() const { return static_cast<double>(frenzy_left_us_) / 1e6; }
    int star_sand() const { return star_sand_; }
    int manual_shuffles_left() const { return manual_shuffles_left_; }
    std::optional<Swap> hint() const { return hint_; }
    Outcome outcome() const { return outcome_; }
    int moves() const { return moves_; }
    bool steel_stopped() const { return steel_stopped_; }

    // --- Scenario setup, for tests and tools --------------------------------
    // Change state between commands with no rule checks: the caller keeps
    // the board stable (no run showing). set_steel, set_gem and set_chain
    // throw std::out_of_range for a cell off the board.
    void set_piece(Cell c, int kind);
    void set_steel(Cell c, bool on);
    void set_gem(Cell c, bool on);
    void set_chain(Cell c, bool on);
    void set_power_up(Cell c, std::optional<PowerUp> p);
    void set_gems_collected(int n) { gems_collected_ = n; }
    void set_time_left(double t) { time_left_us_ = to_micros(t); }
    void set_score(int s) { score_ = s; }
    void set_frenzy_bar(int b) { frenzy_bar_ = b; }
    void set_frenzy(bool on, double left);
    TotemSystem& totems_for_setup() { return totems_; }

private:
    struct MoveContext;

    bool mark(const std::vector<bool>& m, Cell c) const {
        if (!board_.in_bounds(c.x, c.y)) return false;
        const auto i = idx(c);
        return i < m.size() && m[i];
    }
    std::size_t idx(Cell c) const {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(board_.width()) +
               static_cast<std::size_t>(c.x);
    }
    std::size_t checked_idx(Cell c, const char* what) const;
    bool movable(Cell c) const { return !steel(c) && !chained(c); }
    bool in_run_after_swap(const std::vector<int>& kinds, Cell c) const;
    int kind_for_run(const std::vector<int>& kinds, Cell c) const;
    StepHooks make_hooks(MoveContext& ctx);
    std::vector<Cell> plain_cells() const;
    void place_initial_gems();
    void on_step_clear(MoveContext& ctx, const std::vector<Cell>& cells, const StepClear& sc, int step);
    void on_step_refill(MoveContext& ctx, const CascadeStep& step, int index);
    void collect_gem_points(MoveContext& ctx, int gems, int& points);
    void start_frenzy(MoveContext& ctx, bool& started, bool& restarted);
    std::optional<MoveResult> clear_effect(MoveContext& ctx, const std::vector<Cell>& cells,
                                           std::vector<StepReport>& steps);
    void fire_totem(MoveContext& ctx, TotemId id, TotemFiring& firing);
    bool goal_met() const;
    ShuffleReport do_shuffle();

    LevelData level_;
    Difficulty difficulty_;
    Board board_;
    Rng rng_;  // the level's generator (gems, steel, power-ups, totems)
    PowerUpLayer power_ups_;
    PowerUpTiers power_up_tiers_{};
    TotemSystem totems_;
    PieceMarks marks_;
    std::vector<bool> chains_;
    int gems_max_ = kBaseGemsOnBoard;
    std::optional<int> gem_goal_;
    std::map<int, int> cleared_by_kind_;
    int gems_collected_ = 0;
    std::int64_t time_left_us_ = 0;  // game time counts in whole microseconds
    int score_ = 0;
    int frenzy_tier_ = 0;
    int frenzy_bar_ = 0;
    bool frenzy_on_ = false;
    std::int64_t frenzy_left_us_ = 0;
    int star_sand_ = 0;
    int manual_shuffles_left_ = kManualShufflesPerLevel;
    std::optional<Swap> hint_;
    Outcome outcome_ = Outcome::Playing;
    int moves_ = 0;
    bool steel_stopped_ = false;
    bool free_hints_ = false;
    std::int64_t idle_us_ = 0;
    bool free_hint_shown_ = false;
};

// Seeds of the session's streams, derived from its seed.
std::uint64_t derive_seed(std::uint64_t seed, std::uint64_t stream) noexcept;

}  // namespace montezuma
