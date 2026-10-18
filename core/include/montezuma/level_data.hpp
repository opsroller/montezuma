// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/level_state.hpp"

namespace montezuma {

// Artifact kind names in kind-index order (contracts: LevelData).
inline constexpr std::array<std::string_view, 7> kKindNames{
    "red", "orange", "yellow", "green", "blue", "purple", "white"};

// Kind index for a kind name, or empty for an unknown name.
std::optional<int> kind_index(std::string_view name);

// The original has Adventure (our Normal) and Hard, and no Relaxed mode.
// rule: <tbd>
enum class Difficulty { Normal, Hard };

// "normal" or "hard"; empty for anything else.
std::optional<Difficulty> difficulty_from_name(std::string_view name);

struct BoardSpec {
    static constexpr int kDefaultWidth = 8;
    static constexpr int kDefaultHeight = 8;
    static constexpr int kDefaultKinds = 7;
    static constexpr int kMinKinds = 3;
    static constexpr int kMaxKinds = static_cast<int>(kKindNames.size());

    int width = kDefaultWidth;
    int height = kDefaultHeight;
    int kinds = kDefaultKinds;
};

// Per-difficulty base clock in seconds, when the level sets its own. The
// timer tier's bonus is added on top (GOAL-03).
struct BaseClock {
    double normal = 0.0;  // seconds
    double hard = 0.0;    // seconds
};

// Gems to collect to win, per difficulty. The original's level goal.
// rule: <tbd>
struct GemGoal {
    int normal = 0;
    int hard = 0;
};

// Which goal kinds a level has. A parsed level has at least one.
struct LevelGoal {
    bool gems = false;    // gem_goal is set
    bool quotas = false;  // quotas is non-empty (rule-variant goal)
};

// Level time is the base clock plus 30 s per timer tier (GOAL-03). A level
// without base_clock has 270 s: 270, 300, 330, 360 and 390 s by tier.
// rule: GOAL-03
inline constexpr int kMinTimerTier = 0;
inline constexpr int kMaxTimerTier = 4;
inline constexpr double kBaseTimerSeconds = 270.0;
inline constexpr double kTimerTierStepSeconds = 30.0;

// Defaults for the optional rule-set and theme ids.
inline constexpr std::string_view kDefaultRules = "classic";
inline constexpr std::string_view kDefaultTheme = "temple";

// One validation problem. `path` names the field, e.g. "quotas.pink" or
// "blocked_cells[2][0]"; it is empty for problems with the whole document.
struct LevelDataError {
    std::string path;
    std::string message;

    // "path: message", or just the message when path is empty.
    std::string to_string() const;
};

struct LevelParseResult;

// A level file in the LevelData contract, validated.
struct LevelData {
    std::string id;  // "e<episode>-l<NN>"
    int episode = 0;
    int index = 0;
    BoardSpec board;
    std::optional<GemGoal> gem_goal;
    std::map<int, int> quotas;             // kind index -> pieces to clear; empty when absent
    std::optional<BaseClock> base_clock;   // absent: kBaseTimerSeconds
    std::vector<std::string> totems_unlocked;
    std::vector<std::string> powerups_unlocked;
    double steel_rate = 0.0;               // chance per spawned piece, 0 to 1
    std::vector<Cell> blocked_cells;
    std::vector<Cell> chained_cells;
    std::string rules{kDefaultRules};      // rule-set id
    std::string theme{kDefaultTheme};      // theme id
    std::uint64_t seed = 0;

    // Parses and validates a level file's JSON text. Never throws: on failure
    // the result holds every problem found, each with its field path.
    static LevelParseResult parse(std::string_view json) noexcept;
    // Same as parse(), but throws std::invalid_argument listing the errors,
    // one per line, for core callers that use exceptions.
    static LevelData load(std::string_view json);

    // Which goal kinds this level has.
    LevelGoal goal() const;

    // The difficulty's base clock: base_clock when the level has one,
    // otherwise kBaseTimerSeconds.
    double base_clock_for(Difficulty difficulty) const;
    // Seconds the level starts with: base_clock_for() plus
    // kTimerTierStepSeconds per timer tier. Throws std::invalid_argument when
    // timer_tier is outside kMinTimerTier..kMaxTimerTier, or when the base
    // clock is not a number greater than 0 (set in code; the parser rejects it).
    double time_limit_for(Difficulty difficulty, int timer_tier = 0) const;
    // Fresh level state for this level's quotas and time_limit_for()'s timer.
    // Throws std::invalid_argument for a bad timer_tier, and for a level with
    // a gem goal and no quotas: LevelState cannot track gems yet.
    LevelState make_state(Difficulty difficulty, int timer_tier = 0) const;
};

struct LevelParseResult {
    std::optional<LevelData> level;     // set only when errors is empty
    std::vector<LevelDataError> errors;

    bool ok() const { return level.has_value(); }
    // All errors, one "path: message" per line.
    std::string error_text() const;
};

}  // namespace montezuma
