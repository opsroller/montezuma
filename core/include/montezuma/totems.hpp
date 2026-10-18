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

// Power Totems (docs/reference/totems.md). One totem per piece colour.
//
// Colour mapping, defined here and nowhere else: a totem's enum value is the
// board kind index of its colour. Assumed until the rules spec confirms it.
// rule: <tbd>
enum class TotemId : int {
    Fire = 0,     // red
    Time = 1,     // orange
    Gold = 2,     // yellow
    Gift = 3,     // purple
    Harvest = 4,  // green
    Paint = 5,    // white
    Ward = 6,     // blue
};
inline constexpr std::size_t kTotemCount = 7;
inline constexpr int kMaxTotemTier = 4;

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
    // Times the totem can fire per level at this tier.
    int uses_per_level = 1;
    // fire: fireball groups, drawn in array order.
    std::array<Fireballs, 2> fireballs{};
    // time: seconds added to the level timer.
    double time_seconds = 0.0;
    // gold: points awarded, and whether Score Frenzy starts.
    int points = 0;
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
    // rule: <tbd>
    int charges_to_fire = 2;
    // Tiers in use, 1..kMaxTotemTier; tiers[tier_count..] are ignored.
    int tier_count = kMaxTotemTier;
    std::array<TotemTierDef, kMaxTotemTier> tiers{};
};

using TotemTable = std::array<TotemDef, kTotemCount>;

// Default tuning, one row per totem in TotemId (kind) order, from the game
// designer's sources (WP-03). Swap the table for a rule variant.
// rule: <tbd>
inline constexpr TotemTable kTotemDefs{{
    // rule: <tbd>
    {.id = TotemId::Fire,
     .tiers = {{
         {.uses_per_level = 1, .fireballs = {{{.count = 2, .size = 3}, {}}}},
         {.uses_per_level = 1, .fireballs = {{{.count = 4, .size = 3}, {}}}},
         {.uses_per_level = 2, .fireballs = {{{.count = 2, .size = 4}, {.count = 2, .size = 5}}}},
         {.uses_per_level = 2, .fireballs = {{{.count = 4, .size = 5}, {}}}},
     }}},
    // rule: <tbd>
    {.id = TotemId::Time,
     .tiers = {{
         {.uses_per_level = 1, .time_seconds = 60.0},
         {.uses_per_level = 1, .time_seconds = 120.0},
         {.uses_per_level = 2, .time_seconds = 180.0},
         {.uses_per_level = 2, .time_seconds = 240.0},
     }}},
    // rule: <tbd>
    // [verify] points: sources read 1500/2000/2000, 2500/2500, 3000.
    {.id = TotemId::Gold,
     .tiers = {{
         {.uses_per_level = 1, .points = 1500},
         {.uses_per_level = 1, .points = 2000, .start_frenzy = true},
         {.uses_per_level = 2, .points = 2500, .start_frenzy = true},
         {.uses_per_level = 2, .points = 3000, .start_frenzy = true},
     }}},
    // rule: <tbd>
    {.id = TotemId::Gift,
     .tiers = {{
         {.uses_per_level = 1, .powerups_to_place = 1},
         {.uses_per_level = 1, .powerups_to_place = 2},
         {.uses_per_level = 2, .powerups_to_place = 3},
         {.uses_per_level = 2, .powerups_to_place = 4},
     }}},
    // rule: <tbd>
    {.id = TotemId::Harvest,
     .tiers = {{
         {.uses_per_level = 1, .collect_all_gems = true},
         {.uses_per_level = 1, .collect_all_gems = true, .gems_to_place = 8},
         {.uses_per_level = 2, .collect_all_gems = true, .gems_to_place = 9, .remove_chains = true},
         {.uses_per_level = 2, .collect_all_gems = true, .gems_to_place = 10, .remove_chains = true},
     }}},
    // rule: <tbd>
    {.id = TotemId::Paint,
     .tiers = {{
         {.uses_per_level = 1, .repaint_count = 4},
         {.uses_per_level = 1, .repaint_count = 6},
         {.uses_per_level = 2, .repaint_count = 8},
         {.uses_per_level = 2, .repaint_count = 10},
     }}},
    // rule: <tbd>
    {.id = TotemId::Ward,
     .tier_count = 3,
     .tiers = {{
         {.uses_per_level = 1, .destroy_steel = true},
         {.uses_per_level = 1, .destroy_steel = true, .stop_steel = true},
         {.uses_per_level = 2, .destroy_steel = true, .stop_steel = true, .clear_around_steel = true},
         {},
     }}},
}};

// What a fired totem does, as data. Nothing here has touched the board.
// Gems, steel and chains are not in Board yet, so those outcomes are flags
// and counts for the game layer. Unused fields keep these neutral values.
struct TotemEffect {
    TotemId totem = TotemId::Fire;
    int tier = 1;
    // fire: unique, in bounds, occupied, sorted row-major (y, then x).
    std::vector<Cell> cells_to_clear;
    // time
    double time_delta_seconds = 0.0;
    // gold
    int points = 0;
    bool start_frenzy = false;
    // gift: placement and power-up type are the game layer's choice.
    int powerups_to_place = 0;
    // harvest
    bool collect_all_gems = false;
    int gems_to_place = 0;
    bool remove_chains = false;
    // paint: unique, in bounds, occupied, not already repaint_kind, sorted
    // row-major. repaint_kind is -1 when nothing is repainted.
    std::vector<Cell> cells_to_repaint;
    int repaint_kind = -1;
    // ward
    bool destroy_steel = false;
    bool stop_steel = false;
    bool clear_around_steel = false;
};

// A totem to unlock, at a tier in 1..its def's tier_count.
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
    // From LevelData's `totems_unlocked` strings, each at tier 1. Throws
    // std::invalid_argument naming an unknown id.
    static TotemSystem from_names(const std::vector<std::string_view>& unlocked, const TotemTable& defs = kTotemDefs);

    bool unlocked(TotemId id) const { return state_[index(id)].unlocked; }
    // Tier of the totem (1 if never unlocked).
    int tier(TotemId id) const { return state_[index(id)].tier; }
    // Unlocks (or re-tiers) a totem and refills its uses for that tier. Its
    // charge is kept. Throws std::invalid_argument for a tier out of range.
    void unlock(TotemId id, int tier = 1);
    const TotemDef& def(TotemId id) const { return defs_[index(id)]; }
    const TotemTierDef& tier_def(TotemId id) const;

    // Refills every unlocked totem's uses and empties every charge.
    void start_level();

    // Consecutive qualifying moves counted so far; below charges_to_fire.
    int charge_count(TotemId id) const { return state_[index(id)].charge; }
    // charge_count / charges_to_fire, in [0, 1); 0 for a locked or spent
    // totem. Feeds the HUD's totem_charge_changed signal.
    double charge(TotemId id) const;
    // Fires left this level; 0 for a locked totem.
    int uses_left(TotemId id) const { return state_[index(id)].uses_left; }

    // Records one move. A result without a swap (steps empty or matched_kind
    // -1) changes nothing. Otherwise, for each unlocked totem with uses left,
    // its charge grows by 1 if its colour is a key of steps[0], else drops
    // to 0; at charges_to_fire it fires, drops to 0 and spends a use. Returns
    // the fired totems in kind order.
    std::vector<TotemId> record_move(const MoveResult& result);

    // Builds a fired totem's effect at its current tier against the board as
    // it is after the move resolved. Random choices draw from `rng` in a
    // fixed order (docs/reference/totems.md).
    TotemEffect make_effect(TotemId id, const Board& board, Rng& rng) const;

    // record_move, then make_effect for each fired totem in kind order, all
    // against the same board.
    std::vector<TotemEffect> apply_move(const MoveResult& result, const Board& board, Rng& rng);

private:
    struct State {
        bool unlocked = false;
        int tier = 1;
        int charge = 0;
        int uses_left = 0;
    };

    static std::size_t index(TotemId id) { return static_cast<std::size_t>(id); }

    TotemTable defs_;
    std::array<State, kTotemCount> state_{};
};

}  // namespace montezuma
