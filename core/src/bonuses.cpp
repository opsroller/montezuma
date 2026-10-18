// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/bonuses.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace montezuma {

namespace {

// Tuning table, one BonusDef per id in BonusId order. Every number here is a
// [verify] placeholder until WP-03 (game designer) settles the original's
// bonuses; change them only with a rules-spec change.
//
// Level params: {max_stock, extent, both_axes, time_seconds, freeze_seconds, reshuffle}
constexpr std::array<BonusDef, kBonusCount> kBonusDefs{{
    // [verify] placeholder until WP-03: hammer clears one piece; upgrades
    // widen it to a plus (radius 1) and a diamond (radius 2).
    {BonusId::Hammer, BonusShape::Diamond, 4, 30,
     {{{3, 0, false, 0.0, 0.0, false}, {4, 1, false, 0.0, 0.0, false}, {5, 2, false, 0.0, 0.0, false}}}},
    // [verify] placeholder until WP-03: lightning clears the chosen row or
    // column; level 2 clears both through the target, level 3 a 3-wide cross.
    {BonusId::Lightning, BonusShape::Line, 4, 25,
     {{{2, 0, false, 0.0, 0.0, false}, {3, 0, true, 0.0, 0.0, false}, {3, 1, true, 0.0, 0.0, false}}}},
    // [verify] placeholder until WP-03: seconds added to the level timer.
    {BonusId::ExtraTime, BonusShape::None, 4, 20,
     {{{2, 0, false, 10.0, 0.0, false}, {3, 0, false, 15.0, 0.0, false}, {3, 0, false, 20.0, 0.0, false}}}},
    // [verify] placeholder until WP-03: reshuffles the board; upgrades only
    // raise how many the player may hold.
    {BonusId::Shuffle, BonusShape::None, 4, 15,
     {{{1, 0, false, 0.0, 0.0, true}, {2, 0, false, 0.0, 0.0, true}, {3, 0, false, 0.0, 0.0, true}}}},
    // [verify] placeholder until WP-03: dynamite clears a square around the
    // target, 3x3 then 5x5 then 7x7. Needs a match of 5.
    {BonusId::Dynamite, BonusShape::Square, 5, 6,
     {{{1, 1, false, 0.0, 0.0, false}, {2, 2, false, 0.0, 0.0, false}, {2, 3, false, 0.0, 0.0, false}}}},
    // [verify] placeholder until WP-03: stops the level timer. Needs a match of 5.
    {BonusId::Freeze, BonusShape::None, 5, 4,
     {{{1, 0, false, 0.0, 5.0, false}, {2, 0, false, 0.0, 8.0, false}, {2, 0, false, 0.0, 12.0, false}}}},
}};

constexpr std::array<std::string_view, kBonusCount> kBonusNames{
    "hammer", "lightning", "extra_time", "shuffle", "dynamite", "freeze"};

constexpr bool table_is_consistent() {
    for (std::size_t i = 0; i < kBonusCount; ++i) {
        const BonusDef& d = kBonusDefs[i];
        if (static_cast<std::size_t>(d.id) != i) return false;
        if (d.earn_min_cleared < kBonusEarnThreshold || d.earn_weight <= 0) return false;
        int previous_stock = 0;
        for (const BonusLevelParams& p : d.levels) {
            if (p.max_stock < previous_stock || p.max_stock < 1 || p.extent < 0) return false;
            previous_stock = p.max_stock;
        }
    }
    return true;
}
static_assert(table_is_consistent(), "bonus table: ids in order, caps >= 1 and non-decreasing");

int clamp_level(int level) { return std::clamp(level, kMinBonusLevel, kMaxBonusLevel); }

}  // namespace

std::string_view bonus_name(BonusId id) { return kBonusNames[BonusInventory::index(id)]; }

std::optional<BonusId> bonus_from_name(std::string_view name) {
    for (std::size_t i = 0; i < kBonusCount; ++i)
        if (kBonusNames[i] == name) return static_cast<BonusId>(i);
    return std::nullopt;
}

const BonusLevelParams& BonusDef::at_level(int level) const {
    return levels[static_cast<std::size_t>(clamp_level(level) - kMinBonusLevel)];
}

std::span<const BonusDef, kBonusCount> bonus_defs() { return kBonusDefs; }

const BonusDef& bonus_def(BonusId id) { return kBonusDefs[BonusInventory::index(id)]; }

Effect bonus_effect(BonusId id, int level, const Board& board, BonusTarget target) {
    const BonusDef& def = bonus_def(id);
    const BonusLevelParams& p = def.at_level(level);

    Effect effect;
    effect.bonus = id;
    effect.time_delta_seconds = p.time_seconds;
    effect.freeze_seconds = p.freeze_seconds;
    effect.reshuffle = p.reshuffle;
    if (!def.targeted()) return effect;

    const int tx = target.cell.x;
    const int ty = target.cell.y;
    const auto covers = [&](int x, int y) {
        const int dx = std::abs(x - tx);
        const int dy = std::abs(y - ty);
        switch (def.shape) {
            case BonusShape::Diamond:
                return dx + dy <= p.extent;
            case BonusShape::Square:
                return dx <= p.extent && dy <= p.extent;
            case BonusShape::Line: {
                const bool on_row = dy <= p.extent;
                const bool on_column = dx <= p.extent;
                if (p.both_axes) return on_row || on_column;
                return target.axis == Axis::Row ? on_row : on_column;
            }
            case BonusShape::None:
                break;
        }
        return false;
    };
    // Scanning only the board clips the shape to it and yields row-major order.
    for (int y = 0; y < board.height(); ++y)
        for (int x = 0; x < board.width(); ++x)
            if (covers(x, y)) effect.cells_to_clear.push_back({x, y});
    return effect;
}

int BonusInventory::cap(BonusId id) const { return bonus_def(id).at_level(level_of(id)).max_stock; }

bool BonusInventory::add(BonusId id) {
    if (count(id) >= cap(id)) return false;
    ++stock[index(id)];
    return true;
}

std::vector<BonusId> BonusInventory::earn_from(const MoveResult& result, Rng& rng) {
    // Today MoveResult carries only per-step kind counts, so "a match of 4+"
    // is read as "a step cleared 4+ of one kind". Two separate runs of 3 in
    // one step therefore also qualify. Switch to CascadeStep::runs when the
    // cell-level MoveResult lands (see docs/reference/bonuses.md).
    std::vector<BonusId> awarded;
    for (const auto& step : result.steps) {
        for (const auto& [kind, cleared] : step) {  // std::map: ascending kind order
            if (cleared < kBonusEarnThreshold) continue;
            int total_weight = 0;
            for (const BonusDef& d : kBonusDefs)
                if (cleared >= d.earn_min_cleared && count(d.id) < cap(d.id)) total_weight += d.earn_weight;
            if (total_weight == 0) continue;  // every eligible bonus is full: pickup lost
            int roll = rng.below(total_weight);
            for (const BonusDef& d : kBonusDefs) {
                if (cleared < d.earn_min_cleared || count(d.id) >= cap(d.id)) continue;
                if (roll < d.earn_weight) {
                    add(d.id);
                    awarded.push_back(d.id);
                    break;
                }
                roll -= d.earn_weight;
            }
        }
    }
    return awarded;
}

UseResult BonusInventory::use(BonusId id, const Board& board, BonusTarget target) {
    if (count(id) <= 0) return {std::nullopt, UseError::NoStock};
    if (bonus_def(id).targeted() && !board.in_bounds(target.cell.x, target.cell.y))
        return {std::nullopt, UseError::TargetOutOfBounds};
    --stock[index(id)];
    return {bonus_effect(id, level_of(id), board, target), UseError::None};
}

bool BonusInventory::upgrade(BonusId id) {
    if (level_of(id) >= kMaxBonusLevel) return false;
    ++level[index(id)];
    return true;
}

std::string BonusInventory::to_text() const {
    std::string out;
    for (std::size_t i = 0; i < kBonusCount; ++i) {
        out += kBonusNames[i];
        out += ' ';
        out += std::to_string(stock[i]);
        out += ' ';
        out += std::to_string(level[i]);
        out += '\n';
    }
    return out;
}

BonusInventory BonusInventory::from_text(std::string_view text) {
    BonusInventory inv;
    std::array<bool, kBonusCount> seen{};
    std::istringstream lines{std::string(text)};
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        std::istringstream fields(line);
        std::string name;
        int stock_value = 0;
        int level_value = 0;
        std::string extra;
        if (!(fields >> name >> stock_value >> level_value) || (fields >> extra))
            throw std::invalid_argument("bonus inventory: malformed line '" + line + "'");
        const auto id = bonus_from_name(name);
        if (!id) throw std::invalid_argument("bonus inventory: unknown bonus '" + name + "'");
        const std::size_t i = index(*id);
        if (seen[i]) throw std::invalid_argument("bonus inventory: duplicate bonus '" + name + "'");
        seen[i] = true;
        if (level_value < kMinBonusLevel || level_value > kMaxBonusLevel)
            throw std::invalid_argument("bonus inventory: level out of range for '" + name + "'");
        inv.level[i] = level_value;
        if (stock_value < 0 || stock_value > inv.cap(*id))
            throw std::invalid_argument("bonus inventory: stock out of range for '" + name + "'");
        inv.stock[i] = stock_value;
    }
    for (std::size_t i = 0; i < kBonusCount; ++i)
        if (!seen[i])
            throw std::invalid_argument("bonus inventory: missing bonus '" + std::string(kBonusNames[i]) + "'");
    return inv;
}

std::optional<BonusInventory> BonusInventory::try_from_text(std::string_view text) noexcept {
    try {
        return from_text(text);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace montezuma
