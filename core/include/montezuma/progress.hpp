// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// Player progress for the classic campaign (WP-08; progression.md): which
// levels are open and won on each difficulty, best scores, bonus levels,
// golden stars and star sand, bought upgrades, trophies, and the save file.
// Engine independent and deterministic. See docs/reference/progress.md.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "montezuma/bonuses.hpp"
#include "montezuma/level_data.hpp"
#include "montezuma/level_session.hpp"
#include "montezuma/totems.hpp"
#include "montezuma/trophies.hpp"

namespace montezuma {

// rule: PROG-01 five episodes of 5, 6, 8, 10 and 12 levels.
inline constexpr std::array<int, 5> kEpisodeLevelCounts{5, 6, 8, 10, 12};
inline constexpr int kCampaignLevelCount = 41;
// rule: PROG-06 star sand per golden star.
inline constexpr int kStarSandPerStar = 50;
// rule: PROG-02, BG-01 golden stars for a first win or a first pass.
inline constexpr int kStarsPerFirstWin = 1;
// rule: PROG-04 every tier above the free ones costs this many stars.
inline constexpr int kStarsPerTier = 1;
// The star count saturates here instead of wrapping.
inline constexpr int kMaxStars = 2147483647;
// The save format's version.
inline constexpr int kSaveVersion = 1;

// Every campaign level id in play order ("e1-l01" .. "e5-l12").
std::span<const std::string> campaign_level_ids();
// The 0-based play-order index of a campaign level; empty for anything else.
std::optional<int> campaign_index(std::string_view level_id);

// A bonus level (rules-bonus-games.md) and the level it follows.
// AMBIGUOUS: bonus level ids are "<game>-<tier>", e.g. "bg_accuracy-1".
struct BonusLevel {
    std::string_view id;
    std::string_view game;  // bg_accuracy, bg_memory, bg_reaction, bg_attention
    int tier = 1;
    std::string_view after;  // the campaign level it follows
};
inline constexpr std::size_t kBonusLevelCount = 8;
std::span<const BonusLevel, kBonusLevelCount> bonus_levels();
std::optional<BonusLevel> bonus_level(std::string_view id);

enum class BonusResult { NotPlayed, Passed, Failed };

// One upgradable item: a board upgrade, or a totem or power-up by its id.
enum class UpgradeKind { Timer, Gems, Frenzy, Totem, PowerUp };
struct UpgradeItem {
    UpgradeKind kind = UpgradeKind::Timer;
    int index = 0;  // TotemId or PowerUpId value for Totem and PowerUp

    static UpgradeItem timer() { return {UpgradeKind::Timer, 0}; }
    static UpgradeItem gems() { return {UpgradeKind::Gems, 0}; }
    static UpgradeItem frenzy() { return {UpgradeKind::Frenzy, 0}; }
    static UpgradeItem totem(TotemId id) { return {UpgradeKind::Totem, static_cast<int>(id)}; }
    static UpgradeItem power_up(PowerUpId id) { return {UpgradeKind::PowerUp, static_cast<int>(id)}; }
    friend bool operator==(const UpgradeItem&, const UpgradeItem&) = default;
};

// Highest tier of an item (progression Upgrades and Unlocks tables).
int max_tier(UpgradeItem item);
// The level at which `tier` (1..max_tier) becomes available; empty for a
// tier the item does not have.
std::optional<std::string_view> unlock_level(UpgradeItem item, int tier);
// rule: PROG-03 tier 1 of a totem, a power-up and Score Frenzy is free;
// every timer and gems tier costs a star.
bool tier_is_free(UpgradeItem item, int tier);

// One campaign level on one difficulty.
struct LevelRecord {
    bool won = false;
    std::optional<int> best_score;  // AMBIGUOUS: best of any play, won or lost
    friend bool operator==(const LevelRecord&, const LevelRecord&) = default;
};

// What recording a play or a bonus level gave.
struct Reward {
    int stars = 0;                   // golden stars added (first win, sand)
    std::vector<TrophyId> trophies;  // newly earned, in TrophyId order
};

struct ProgressLoadResult;

class Progress {
public:
    // --- Campaign --------------------------------------------------------
    // rule: PROG-01 the next level opens on a win on the same difficulty
    // (AMBIGUOUS: per difficulty). DIFF-02 hard e1-l01 opens when e5-l12 is
    // won on normal; normal e1-l01 is always open.
    bool is_open(std::string_view level_id, Difficulty d) const;
    bool is_won(std::string_view level_id, Difficulty d) const;
    std::optional<int> best_score(std::string_view level_id, Difficulty d) const;
    // Record one finished play (Won or Lost). Throws std::invalid_argument
    // for an unknown or closed level, a play still in progress, a negative
    // score, negative star sand or a time left that is negative or NaN.
    Reward record_level(std::string_view level_id, Difficulty d, const LevelPlay& play);

    // --- Bonus levels ----------------------------------------------------
    // Open once the level it follows is won on any difficulty.
    bool bonus_open(std::string_view bonus_id) const;
    BonusResult bonus_result(std::string_view bonus_id) const;
    // rule: BG-01, BG-02 only the first play can give the star.
    // Throws std::invalid_argument for an unknown or closed bonus level.
    Reward record_bonus(std::string_view bonus_id, bool passed);

    // --- Stars -------------------------------------------------------------
    int stars() const { return stars_; }
    int star_sand() const { return star_sand_; }
    // rule: PROG-06 every 50 sand turns into a star at once. Returns the
    // stars made. Throws std::invalid_argument for a negative amount.
    int add_star_sand(int sand);

    // --- Upgrades ----------------------------------------------------------
    // The tier the player owns (0 = none), free tiers included (PROG-03).
    int tier(UpgradeItem item) const;
    // True when the next tier exists, its level is open on either
    // difficulty, every tier below it is owned and a star is there to pay.
    bool can_buy(UpgradeItem item) const;
    // rule: PROG-04 buy the next tier for one star; false, and nothing
    // changes, when can_buy is false.
    bool buy(UpgradeItem item);
    // rule: PROG-05 the tiers to start `level_id` with (either difficulty).
    // Throws std::invalid_argument for an unknown level.
    Upgrades upgrades_for(std::string_view level_id) const;

    // --- Trophies ------------------------------------------------------------
    // Throw std::out_of_range for a value outside the enum.
    bool has_trophy(TrophyId id) const { return trophies_.at(static_cast<std::size_t>(id)); }
    bool totem_fired(TotemId id) const { return totems_fired_.at(static_cast<std::size_t>(id)); }

    // --- Save file ---------------------------------------------------------
    // Deterministic JSON: the same progress gives the same text.
    std::string to_json() const;
    // Never throws. An empty or blank text gives a fresh profile.
    static ProgressLoadResult try_from_json(std::string_view json) noexcept;

    friend bool operator==(const Progress&, const Progress&) = default;

private:
    // Earned trophies not yet held, marked held, in TrophyId order.
    void award(TrophyId id, Reward& reward);
    // Adds up to n stars, saturating at kMaxStars; returns the stars added.
    int add_stars(int n);
    void check_profile_trophies(Reward& reward);
    int bought(UpgradeItem item) const;
    int& bought_ref(UpgradeItem item);
    bool reached(std::string_view level_id) const;

    int stars_ = 0;
    int star_sand_ = 0;
    std::array<std::map<std::string, LevelRecord, std::less<>>, 2> levels_;  // by Difficulty
    std::map<std::string, BonusResult, std::less<>> bonus_;
    int timer_bought_ = 0;
    int gems_bought_ = 0;
    int frenzy_bought_ = 0;
    std::array<int, kTotemCount> totem_bought_{};
    std::array<int, kPowerUpCount> power_up_bought_{};
    std::array<bool, kTotemCount> totems_fired_{};
    std::array<bool, kTrophyCount> trophies_{};
};

struct ProgressLoadResult {
    std::optional<Progress> progress;  // empty on error
    std::string error;                 // empty on success
};

}  // namespace montezuma
