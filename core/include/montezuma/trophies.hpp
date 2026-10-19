// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// Trophies (WP-08; progression.md#trophies). The nine ids, and TrophyWatch,
// which follows one level play and reports what it earned inside the level.
// Profile-wide trophies (totem_keeper, sharp_eye, explorer, legend,
// first_find) are decided by Progress. See docs/reference/progress.md.

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "montezuma/level_session.hpp"
#include "montezuma/totems.hpp"

namespace montezuma {

// The enum order is the stable index and the order trophies are reported in.
enum class TrophyId : int {
    FirstFind,
    ChainReaction,
    Demolition,
    TotemKeeper,
    QuickHands,
    Frenzy,
    SharpEye,
    Explorer,
    Legend,
};
inline constexpr std::size_t kTrophyCount = 9;

// rule: progression Trophies (chosen)
// chain_reaction: a move's cascade reaches this step index (six clears).
// AMBIGUOUS: only the swap's own cascade steps count, not a totem's.
inline constexpr int kChainReactionStep = 5;
// demolition: one move clears this many pieces.
// AMBIGUOUS: every piece the move clears counts, its totems' clears too.
inline constexpr int kDemolitionPieces = 40;
// quick_hands: a level is won with at least this many seconds left.
inline constexpr double kQuickHandsSeconds = 150.0;
// frenzy: gems collected during one Score Frenzy.
// AMBIGUOUS: a gold-totem restart (TOT-12) continues the same Frenzy.
inline constexpr int kFrenzyTrophyGems = 12;

// Contract id string ("first_find", ...) for a trophy.
std::string_view trophy_name(TrophyId id) noexcept;
// Inverse of trophy_name; empty for an unknown id.
std::optional<TrophyId> trophy_from_name(std::string_view name) noexcept;

// What one level play brings to the profile.
struct LevelPlay {
    Outcome outcome = Outcome::Playing;  // Won or Lost to record it
    int score = 0;
    double time_left = 0.0;
    int star_sand = 0;
    std::array<bool, kTotemCount> totems_fired{};  // by TotemId
    std::vector<TrophyId> earned;  // in-level trophies, in TrophyId order
};

// Follows one LevelSession: feed it every report the session returns, then
// take play(). Deterministic; reads nothing but the reports.
class TrophyWatch {
public:
    void on_move(const MoveReport& report);
    void on_tick(const TickReport& report);
    // The play so far, with the session's outcome, score, time and sand.
    LevelPlay play(const LevelSession& session) const;

    const std::array<bool, kTrophyCount>& earned() const { return earned_; }
    int frenzy_gems() const { return frenzy_gems_; }

private:
    std::array<bool, kTrophyCount> earned_{};
    std::array<bool, kTotemCount> totems_fired_{};
    int frenzy_gems_ = 0;  // gems collected in the Frenzy running now
};

}  // namespace montezuma
