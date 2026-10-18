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

// Power Totems (docs/reference/totems.md; rules in rules-totems.md). One
// totem per piece colour.
//
// Colour mapping, defined here and nowhere else: a totem's enum value is the
// board kind index of its colour, in the contract kind order red, orange,
// yellow, green, blue, purple, white.
// rule: TOT ids table (rules-totems.md#ids)
enum class TotemId : int {
    Fire = 0,     // red
    Time = 1,     // orange
    Gold = 2,     // yellow
    Harvest = 3,  // green
    Ward = 4,     // blue
    Gift = 5,     // purple
    Paint = 6,    // white
};
inline constexpr std::size_t kTotemCount = 7;
inline constexpr int kMaxTotemTier = 4;
// Uses per level are 1 or 2 (TOT-06), so per-use values have two slots.
inline constexpr int kMaxTotemUses = 2;
// Power-ups allowed on the board at once when gift places them (TOT-15).
inline constexpr int kGiftPowerupCap = 8;

// Board kind (colour) that charges a totem.
constexpr int totem_kind(TotemId id) { return static_cast<int>(id); }
// The totem charged by a board kind; empty for a kind with no totem.
constexpr std::optional<TotemId> totem_for_kind(int kind) {
    if (kind < 0 || kind >= static_cast<int>(kTotemCount)) return std::nullopt;
    return static_cast<TotemId>(kind);
}

// Contract id string ("fire", ...) for a totem.
std::string_view totem_name(TotemId id);
// Inverse of totem_name; empty for an unknown id.
std::optional<TotemId> totem_from_name(std::string_view name);

// `count` fireballs that each clear `size` random pieces.
struct Fireballs {
    int count = 0;
    int size = 0;
};

// Tuning for one tier of one totem. Only the fields its totem's effect reads
// matter; the rest keep their neutral defaults.
struct TotemTierDef {
    // Times the totem can fire per level at this tier, 1..kMaxTotemUses.
    int uses_per_level = 1;
    // fire: fireball groups, drawn in array order.
    std::array<Fireballs, 2> fireballs{};
    // time: seconds added to the level timer.
    double time_seconds = 0.0;
    // gold: points on the level's first and second use, and whether Score
    // Frenzy starts.
    std::array<int, kMaxTotemUses> points{};
    bool start_frenzy = false;
    // gift: power-ups the game layer places.
    int powerups_to_place = 0;
    // harvest: collect every gem, then place new gems, and remove chains.
    bool collect_all_gems = false;
    int gems_to_place = 0;
    bool remove_chains = false;
    // paint: pieces repainted to one colour present on the board.
    int repaint_count = 0;
    // ward: destroy steel, stop it spawning, clear pieces next to it.
    bool destroy_steel = false;
    bool stop_steel = false;
    bool clear_around_steel = false;
};

// Tuning for one totem.
struct TotemDef {
    TotemId id;
    // Consecutive moves matching the totem's colour needed to fire.
    // rule: TOT-03
    int charges_to_fire = 2;
    // Tiers in use, 1..kMaxTotemTier; tiers[tier_count..] are ignored.
    int tier_count = kMaxTotemTier;
    std::array<TotemTierDef, kMaxTotemTier> tiers{};
};

using TotemTable = std::array<TotemDef, kTotemCount>;

// Default tuning for the `classic` rule set, one row per totem in TotemId
// (kind) order: the Numbers table of rules-totems.md. Uses per level are 1,
// 1, 2, 2 by tier (TOT-06). Swap the table for a rule variant.
inline constexpr TotemTable kTotemDefs{{
    // rule: TOT-10 fireballs x distinct pieces, steel included
    {.id = TotemId::Fire,
     .tiers = {{
         {.uses_per_level = 1, .fireballs = {{{.count = 2, .size = 3}, {}}}},
         {.uses_per_level = 1, .fireballs = {{{.count = 4, .size = 3}, {}}}},
         {.uses_per_level = 2, .fireballs = {{{.count = 2, .size = 4}, {.count = 2, .size = 5}}}},
         {.uses_per_level = 2, .fireballs = {{{.count = 4, .size = 5}, {}}}},
     }}},
    // rule: TOT-11 seconds added to time_left (GOAL-05: no cap)
    {.id = TotemId::Time,
     .tiers = {{
         {.uses_per_level = 1, .time_seconds = 60.0},
         {.uses_per_level = 1, .time_seconds = 120.0},
         {.uses_per_level = 2, .time_seconds = 180.0},
         {.uses_per_level = 2, .time_seconds = 240.0},
     }}},
    // rule: TOT-12 points by use of the level; tiers 2-4 start Frenzy (FRZ-03)
    {.id = TotemId::Gold,
     .tiers = {{
         {.uses_per_level = 1, .points = {1500, 1500}},
         {.uses_per_level = 1, .points = {2000, 2000}, .start_frenzy = true},
         {.uses_per_level = 2, .points = {2000, 2500}, .start_frenzy = true},
         {.uses_per_level = 2, .points = {2500, 3000}, .start_frenzy = true},
     }}},
    // rule: TOT-13 collect every gem; new gems from tier 2; chains from tier 3
    {.id = TotemId::Harvest,
     .tiers = {{
         {.uses_per_level = 1, .collect_all_gems = true},
         {.uses_per_level = 1, .collect_all_gems = true, .gems_to_place = 8},
         {.uses_per_level = 2, .collect_all_gems = true, .gems_to_place = 9, .remove_chains = true},
         {.uses_per_level = 2, .collect_all_gems = true, .gems_to_place = 10, .remove_chains = true},
     }}},
    // rule: TOT-14 three tiers only
    {.id = TotemId::Ward,
     .tier_count = 3,
     .tiers = {{
         {.uses_per_level = 1, .destroy_steel = true},
         {.uses_per_level = 1, .destroy_steel = true, .stop_steel = true},
         {.uses_per_level = 2, .destroy_steel = true, .stop_steel = true, .clear_around_steel = true},
         {},
     }}},
    // rule: TOT-15 power-ups placed, up to kGiftPowerupCap on the board
    {.id = TotemId::Gift,
     .tiers = {{
         {.uses_per_level = 1, .powerups_to_place = 1},
         {.uses_per_level = 1, .powerups_to_place = 2},
         {.uses_per_level = 2, .powerups_to_place = 3},
         {.uses_per_level = 2, .powerups_to_place = 4},
     }}},
    // rule: TOT-16 pieces repainted
    {.id = TotemId::Paint,
     .tiers = {{
         {.uses_per_level = 1, .repaint_count = 4},
         {.uses_per_level = 1, .repaint_count = 6},
         {.uses_per_level = 2, .repaint_count = 8},
         {.uses_per_level = 2, .repaint_count = 10},
     }}},
}};

// What a fired totem does, as data. Nothing here has touched the board.
// Gems, steel, chains and power-ups are not in Board yet, so those outcomes
// are flags and counts for the game layer. Unused fields keep these neutral
// values.
struct TotemEffect {
    TotemId totem = TotemId::Fire;
    int tier = 1;
    // Which use of the level this is, 1..uses_per_level (TOT-12).
    int use = 1;
    // fire: unique, in bounds, occupied, sorted row-major (y, then x).
    std::vector<Cell> cells_to_clear;
    // time
    double time_delta_seconds = 0.0;
    // gold: points for this use; start_frenzy starts Frenzy, or restarts its
    // duration during one (FRZ-03).
    int points = 0;
    bool start_frenzy = false;
    // gift: the game layer draws each power-up uniformly from the unlocked
    // ones and puts it on a random plain piece, stopping at
    // powerup_board_cap power-ups on the board (TOT-15). 0 for other totems.
    int powerups_to_place = 0;
    int powerup_board_cap = 0;
    // harvest, applied in this order: collect every gem (the pieces stay),
    // place gems_to_place gems on random plain pieces ignoring gems_max,
    // remove every chain (TOT-13).
    bool collect_all_gems = false;
    int gems_to_place = 0;
    bool remove_chains = false;
    // paint: unique, in bounds, occupied, not already repaint_kind, sorted
    // row-major. repaint_kind is -1 when nothing is repainted (fewer than two
    // kinds on the board). Gems and power-ups stay on their pieces (TOT-16).
    std::vector<Cell> cells_to_repaint;
    int repaint_kind = -1;
    // ward: destroy every steel piece; no steel spawns for the rest of the
    // level; clear every piece orthogonally next to a destroyed steel piece
    // (TOT-14).
    bool destroy_steel = false;
    bool stop_steel = false;
    bool clear_around_steel = false;
};

// A totem to unlock, at a tier in 1..its def's tier_count. The tier is a
// runtime input from player progress (WP-08), not from LevelData.
struct TotemUnlock {
    TotemId id;
    int tier = 1;
};

// Per-level totem state. Pure: fed each move's result, it charges unlocked
// totems whose colour was in the swap's own match and reports which fired.
class TotemSystem {
public:
    // Throws std::invalid_argument if `defs` is out of order or a value is out
    // of range, or if an unlock names a tier the totem does not have.
    explicit TotemSystem(const std::vector<TotemUnlock>& unlocked = {}, const TotemTable& defs = kTotemDefs);
    // From LevelData's `totems_unlocked` strings (which totems are available),
    // each at tier 1: the default for tests, the sim and the web demo. The
    // game re-tiers each from player progress with unlock(id, tier). Throws
    // std::invalid_argument naming an unknown id.
    static TotemSystem from_names(const std::vector<std::string_view>& unlocked, const TotemTable& defs = kTotemDefs);

    bool unlocked(TotemId id) const { return state_[index(id)].unlocked; }
    // Tier of the totem (1 if never unlocked).
    int tier(TotemId id) const { return state_[index(id)].tier; }
    // Unlocks (or re-tiers) a totem and refills its uses for that tier, as at
    // the start of a level. Its charge is kept. Throws std::invalid_argument
    // for a tier out of range.
    void unlock(TotemId id, int tier = 1);
    const TotemDef& def(TotemId id) const { return defs_[index(id)]; }
    const TotemTierDef& tier_def(TotemId id) const;

    // Refills every unlocked totem's uses and empties every charge (TOT-06).
    void start_level();

    // Consecutive qualifying moves counted so far; below charges_to_fire.
    int charge_count(TotemId id) const { return state_[index(id)].charge; }
    // charge_count / charges_to_fire, in [0, 1); 0 for a locked or spent
    // totem. Feeds the HUD's totem_charge_changed signal; a totem that just
    // fired is shown at 1 first (docs/reference/totems.md).
    double charge(TotemId id) const;
    // Fires left this level; 0 for a locked totem.
    int uses_left(TotemId id) const { return state_[index(id)].uses_left; }
    // Fires spent this level.
    int uses_spent(TotemId id) const { return state_[index(id)].uses_spent; }

    // Records one move and returns the totems it fired, in kind order. Only a
    // player swap counts: a result with no `swap` or no `cascades` (such as a
    // clear_cells effect) changes nothing (TOT-02, TOT-07). For each unlocked
    // totem with uses left (TOT-01), the charge grows by 1 if its colour is
    // the kind of a run in step 0 (cascades[0].runs), else drops to 0
    // (TOT-02, TOT-04). At charges_to_fire it fires: charge 0, one use spent
    // (TOT-03).
    std::vector<TotemId> record_move(const MoveResult& result);

    // Builds a totem's effect at its current tier against `board` as it is
    // now. Random choices draw from `rng` (the level's generator) in a fixed
    // order (TOT-17, docs/reference/totems.md). The use is the one just spent
    // (uses_spent, at least 1).
    TotemEffect make_effect(TotemId id, const Board& board, Rng& rng) const;

    // record_move under the name the firing loop reads with. Returns the fired
    // totems in kind order; the caller then builds and resolves them one at a
    // time, each on the board the previous one left (TOT-05):
    //   for (TotemId id : totems.apply_move(result))
    //       apply_board_effect(totems.make_effect(id, board, rng), board);
    std::vector<TotemId> apply_move(const MoveResult& result) { return record_move(result); }

private:
    struct State {
        bool unlocked = false;
        int tier = 1;
        int charge = 0;
        int uses_left = 0;
        int uses_spent = 0;
    };

    static std::size_t index(TotemId id) { return static_cast<std::size_t>(id); }

    TotemTable defs_;
    std::array<State, kTotemCount> state_{};
};

// Applies the parts of an effect that Board models today and resolves them:
// repaints `cells_to_repaint` to `repaint_kind` (TOT-16), then clears
// `cells_to_clear` plus every run now on the board with Board::clear_cells,
// which falls, refills and cascades (TOT-05, RES-07). Empty when nothing was
// cleared. Time, points, Frenzy, gems, chains, steel and power-ups are the
// game layer's.
std::optional<MoveResult> apply_board_effect(const TotemEffect& effect, Board& board);

}  // namespace montezuma
