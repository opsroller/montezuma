// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// Balance simulator (WP-22): bots play levels through montezuma::LevelSession
// and the results are checked against docs/reference/balance.md. See
// docs/how-to/run-the-balance-simulator.md.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "montezuma/level_data.hpp"
#include "montezuma/level_session.hpp"

namespace montezuma::sim {

// balance.md: a bot moves every 1.5 s of game time; 200 seeds per level.
inline constexpr double kBotMoveSeconds = 1.5;
inline constexpr int kDefaultSeeds = 200;
// Stream key of a bot's own generator (derive_seed of the run seed).
inline constexpr std::uint64_t kBotStream = 3;
// Exit codes.
inline constexpr int kExitOk = 0;
inline constexpr int kExitFail = 1;
inline constexpr int kExitUsage = 2;

enum class Bot { Random, Greedy, Planner };
std::string_view bot_name(Bot bot) noexcept;
std::optional<Bot> bot_from_name(std::string_view name) noexcept;
std::string_view difficulty_name(Difficulty d) noexcept;

// --- Bots -----------------------------------------------------------------

// Greedy's score of a swap: cells its step-0 runs clear, a gem counting 2.
int greedy_score(const LevelSession& s, const Swap& swap);
// Planner: the swap charges an owned totem with uses left to firing.
bool fires_totem(const LevelSession& s, const Swap& swap);
// The bot's pick among `moves` (non-empty, legal_moves() order). Random
// draws one rng.below(moves.size()); the others draw nothing. Ties go to
// the first in order.
Swap choose_move(Bot bot, const LevelSession& s, const std::vector<Swap>& moves, Rng& rng);

// --- Runs -----------------------------------------------------------------

struct RunResult {
    bool won = false;
    bool stuck = false;
    double time_left = 0.0;  // at the win; 0 for a loss
    int moves = 0;
};

// rule: balance.md the session seed of run i depends only on (level seed, i).
std::uint64_t run_seed(std::uint64_t level_seed, int i) noexcept;
// Plays one level to the end: tick(1.5), then one move while Playing.
RunResult play(const LevelData& level, Difficulty d, const Upgrades& up, Bot bot, std::uint64_t seed);

// --- Upgrades ---------------------------------------------------------------

// Campaign number of (episode, index), 1..41 (progression PROG-01): clamped
// into the campaign; episodes past 5 give 41.
int campaign_position(int episode, int index) noexcept;
inline constexpr int kCampaignLevels = 41;
// Every tier a player owns on reaching the level (balance.md; stars never
// bind when bonus levels are passed).
Upgrades auto_upgrades(const LevelData& level);
// Base upgrades: timer 0, gems 5, listed ids at tier 1, Frenzy tier 1 from
// e1-l04 on (it is free, PROG-03).
Upgrades no_upgrades(const LevelData& level);

// --- Targets ----------------------------------------------------------------

enum class Verdict { None, Pass, Fail, Skipped };
std::string_view verdict_name(Verdict v) noexcept;

struct Row {
    std::string level;
    int episode = 0;
    int index = 0;
    Bot bot = Bot::Greedy;
    Difficulty difficulty = Difficulty::Normal;
    int seeds = 0;
    int wins = 0;
    int stuck = 0;
    std::vector<double> win_times;  // time left at each win, run order
    long long total_moves = 0;

    std::optional<double> median_time_left() const;
    double win_rate_pct() const { return seeds > 0 ? 100.0 * wins / seeds : 0.0; }
    double mean_moves() const { return seeds > 0 ? static_cast<double>(total_moves) / seeds : 0.0; }
};

struct TargetRow {
    std::string_view name;
    int greedy_win_pct;       // >=
    double greedy_median;     // >= seconds
    int random_pct;           // >= or <=
    bool random_at_most;
    int planner_hard_pct;     // >=
    int greedy_min_moves;     // >= mean moves a run, greedy/normal; 0 for none
};
// The balance.md row for a level id; empty for ids with no target.
std::optional<TargetRow> target_row(int episode, int index);
// rule: balance.md "Too easy" greedy's mean moves on normal must reach this
// floor (mean over all runs, not the median); 0 where there is none.
int moves_floor(int episode, int index);
// Target text and verdict for one row ("-" and None when it has no target).
std::string target_text(const Row& row);
Verdict evaluate(const Row& row);

struct CurveCheck {
    std::string rule;    // "curve-1" or "curve-2"
    std::string levels;  // e.g. "e1-l02>e1-l03"
    Verdict verdict = Verdict::Skipped;
    std::string detail;
};
// A level that brings a new idea (curve rule 1), against the level before.
bool introduces_idea(const LevelData& level, const LevelData* previous);
// Both curve rules over the greedy/normal rows of the levels given (sorted
// by campaign order).
std::vector<CurveCheck> curve_checks(const std::vector<LevelData>& levels, const std::vector<Row>& rows);

// --- Output -----------------------------------------------------------------

enum class Format { Table, Csv, Json };
std::string format_report(const std::vector<Row>& rows, const std::vector<CurveCheck>& curves, Format f);

}  // namespace montezuma::sim
