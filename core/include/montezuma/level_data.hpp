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

// Power Totem ids a level may unlock (contracts: Totem and bonus ids).
inline constexpr std::array<std::string_view, 6> kTotemIds{
    "row_clear", "random_blast", "time", "lightning", "color_bomb", "multiplier"};

// Kind index for a kind name, or empty for an unknown name.
std::optional<int> kind_index(std::string_view name);

enum class Difficulty { Relaxed, Normal };

// "relaxed" or "normal"; empty for anything else.
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

struct TimeLimit {
    double relaxed = 0.0;  // seconds
    double normal = 0.0;   // seconds
};

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
    std::map<int, int> quotas;  // kind index -> pieces to clear
    TimeLimit time_limit;
    std::vector<std::string> totems_unlocked;
    std::vector<Cell> blocked_cells;
    std::uint64_t seed = 0;

    // Parses and validates a level file's JSON text. Never throws: on failure
    // the result holds every problem found, each with its field path.
    static LevelParseResult parse(std::string_view json) noexcept;
    // Same as parse(), but throws std::invalid_argument listing the errors,
    // one per line, for core callers that use exceptions.
    static LevelData load(std::string_view json);

    double time_limit_for(Difficulty difficulty) const;
    // Fresh level state for this level's quotas and the difficulty's timer.
    LevelState make_state(Difficulty difficulty) const;
};

struct LevelParseResult {
    std::optional<LevelData> level;     // set only when errors is empty
    std::vector<LevelDataError> errors;

    bool ok() const { return level.has_value(); }
    // All errors, one "path: message" per line.
    std::string error_text() const;
};

}  // namespace montezuma
