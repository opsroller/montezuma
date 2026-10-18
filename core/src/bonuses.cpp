// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/bonuses.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace montezuma {

namespace {

// Builders keep the table readable: each sets only the part that applies.
constexpr PowerUpTierParams area(int chebyshev, int manhattan) {
    PowerUpTierParams p;
    p.area_chebyshev = chebyshev;
    p.area_manhattan = manhattan;
    return p;
}
constexpr PowerUpTierParams rays(unsigned mask) {
    PowerUpTierParams p;
    p.rays = mask;
    return p;
}
constexpr PowerUpTierParams colour(bool steel, bool adjacent) {
    PowerUpTierParams p;
    p.colour = true;
    p.colour_steel = steel;
    p.colour_adjacent = adjacent;
    return p;
}
constexpr PowerUpTierParams time_bonus(double seconds) {
    PowerUpTierParams p;
    p.time_seconds = seconds;
    return p;
}
constexpr PowerUpTierParams multiply(int factor) {
    PowerUpTierParams p;
    p.score_multiplier = factor;
    return p;
}
constexpr PowerUpTierParams hint(double seconds) {
    PowerUpTierParams p;
    p.hint_seconds = seconds;
    return p;
}

// The defs table, one row per id in PowerUpId order. Numbers come from the
// game designer's rules (WP-03); change them only with a rules-spec change.
constexpr std::array<PowerUpDef, kPowerUpCount> kPowerUpDefs{{
    // rule: <tbd>
    // L1 3x3 (9 cells), L2 diamond of radius 2 (13), L3 5x5 minus the four
    // corners (21).
    {PowerUpId::Tnt, "tnt", {{area(1, 2), area(2, 2), area(2, 3)}}},
    // rule: <tbd>
    // L1 every piece of its colour, L2 also that colour's steel pieces, L3
    // also every piece next to those cells.
    {PowerUpId::ColorClear, "color_clear", {{colour(false, false), colour(true, false), colour(true, true)}}},
    // rule: <tbd>
    // L1 the column above, L2 up, left and right, L3 the full row and column.
    {PowerUpId::Lightning, "lightning", {{rays(kRayUp), rays(kRayUp | kRayLeft | kRayRight), rays(kRayAll)}}},
    // rule: <tbd>
    {PowerUpId::Clock, "clock", {{time_bonus(15.0), time_bonus(20.0), time_bonus(25.0)}}},
    // rule: <tbd>
    {PowerUpId::Multiplier, "multiplier", {{multiply(5), multiply(7), multiply(10)}}},
    // rule: <tbd>
    {PowerUpId::Hint, "hint", {{hint(20.0), hint(25.0), hint(30.0)}}},
}};

// nullptr when `defs` is well formed, otherwise what is wrong.
constexpr const char* defs_error(std::span<const PowerUpDef> defs) {
    if (defs.size() != kPowerUpCount) return "power-up defs: wrong number of rows";
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const PowerUpDef& d = defs[i];
        if (power_up_index(d.id) != i) return "power-up defs: ids out of order";
        if (d.name.empty()) return "power-up defs: empty name";
        for (std::size_t j = 0; j < i; ++j)
            if (defs[j].name == d.name) return "power-up defs: duplicate name";
        for (const PowerUpTierParams& p : d.tiers) {
            if (p.area_chebyshev < -1 || (p.area_chebyshev >= 0 && p.area_manhattan < 0))
                return "power-up defs: bad area";
            if ((p.rays & ~static_cast<unsigned>(kRayAll)) != 0) return "power-up defs: bad ray mask";
            if ((p.colour_steel || p.colour_adjacent) && !p.colour)
                return "power-up defs: steel or adjacency without colour";
            if (p.score_multiplier < 1) return "power-up defs: multiplier below 1";
            if (p.time_seconds < 0.0 || p.hint_seconds < 0.0) return "power-up defs: negative time";
        }
    }
    return nullptr;
}
static_assert(defs_error(kPowerUpDefs) == nullptr, "power-up defs table is malformed");

constexpr int clamp_tier(int tier) { return std::clamp(tier, kMinPowerUpTier, kMaxPowerUpTier); }

// How many power-ups the runs of `result` earn: one per run of exactly 4,
// two per run of 5+. rule: <tbd>
// [verify] whether cascade steps after the swap's own match count; today
// every step does.
int earned_by_runs(const MoveResult& result) {
    int earned = 0;
    for (const CascadeStep& step : result.cascades) {
        for (const Run& run : step.runs) {
            const std::size_t len = run.cells.size();
            if (len == 4) earned += 1;
            else if (len >= 5) earned += 2;
        }
    }
    return earned;
}

}  // namespace

std::string_view power_up_name(PowerUpId id) noexcept { return kPowerUpDefs[power_up_index(id)].name; }

std::optional<PowerUpId> power_up_from_name(std::string_view name) noexcept {
    for (const PowerUpDef& d : kPowerUpDefs)
        if (d.name == name) return d.id;
    return std::nullopt;
}

const PowerUpTierParams& PowerUpDef::at_tier(int tier) const noexcept {
    return tiers[static_cast<std::size_t>(clamp_tier(tier) - kMinPowerUpTier)];
}

std::span<const PowerUpDef, kPowerUpCount> power_up_defs() noexcept { return kPowerUpDefs; }

const PowerUpDef& power_up_def(PowerUpId id) noexcept { return kPowerUpDefs[power_up_index(id)]; }

void validate_power_up_defs(std::span<const PowerUpDef> defs) {
    if (const char* error = defs_error(defs)) throw std::invalid_argument(error);
}

void validate_power_up_tiers(const PowerUpTiers& tiers) {
    for (std::size_t i = 0; i < kPowerUpCount; ++i)
        if (tiers[i] < kMinPowerUpTier || tiers[i] > kMaxPowerUpTier)
            throw std::invalid_argument("power-up tiers: tier " + std::to_string(tiers[i]) + " out of range for '" +
                                        std::string(kPowerUpDefs[i].name) + "'");
}

PowerUpEffect power_up_effect(PowerUpId id, int tier, const Board& board, Cell at, int colour) noexcept {
    const PowerUpTierParams& p = power_up_def(id).at_tier(tier);

    PowerUpEffect effect;
    effect.id = id;
    effect.tier = clamp_tier(tier);
    effect.time_delta_seconds = p.time_seconds;
    effect.score_multiplier = p.score_multiplier;
    effect.hint_seconds = p.hint_seconds;
    effect.clear_colour_steel = p.colour && p.colour_steel;
    if (!board.in_bounds(at.x, at.y)) return effect;

    const int w = board.width();
    const int h = board.height();
    std::vector<char> mark(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    const auto put = [&](int x, int y) {
        if (board.in_bounds(x, y)) mark[static_cast<std::size_t>(y * w + x)] = 1;
    };

    if (p.area_chebyshev >= 0) {
        for (int dy = -p.area_chebyshev; dy <= p.area_chebyshev; ++dy)
            for (int dx = -p.area_chebyshev; dx <= p.area_chebyshev; ++dx)
                if (std::abs(dx) + std::abs(dy) <= p.area_manhattan) put(at.x + dx, at.y + dy);
    }
    if (p.rays != kRayNone) {
        // [verify] each ray includes the power-up's own cell, so a lightning
        // at the top edge still clears that cell. rule: <tbd>
        put(at.x, at.y);
        if (p.rays & kRayUp)
            for (int y = at.y - 1; y >= 0; --y) put(at.x, y);
        if (p.rays & kRayDown)
            for (int y = at.y + 1; y < h; ++y) put(at.x, y);
        if (p.rays & kRayLeft)
            for (int x = at.x - 1; x >= 0; --x) put(x, at.y);
        if (p.rays & kRayRight)
            for (int x = at.x + 1; x < w; ++x) put(x, at.y);
    }
    if (p.colour) {
        const int kind = colour >= 0 ? colour : board.at(at.x, at.y);
        if (kind != Board::kEmpty) {
            std::vector<char> same(mark.size(), 0);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (board.at(x, y) == kind) same[static_cast<std::size_t>(y * w + x)] = 1;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (!same[static_cast<std::size_t>(y * w + x)]) continue;
                    put(x, y);
                    if (p.colour_adjacent) {
                        // [verify] "adjacent" read as the four orthogonal
                        // neighbours. rule: <tbd>
                        put(x - 1, y);
                        put(x + 1, y);
                        put(x, y - 1);
                        put(x, y + 1);
                    }
                }
            }
        }
    }

    // Scanning in row-major order sorts and de-duplicates; skip empty cells.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (mark[static_cast<std::size_t>(y * w + x)] && board.at(x, y) != Board::kEmpty)
                effect.cells_to_clear.push_back({x, y});
    return effect;
}

PowerUpEffect power_up_effect(const TriggeredPowerUp& triggered, const Board& board) noexcept {
    return power_up_effect(triggered.power_up.id, triggered.power_up.tier, board, triggered.cell,
                           triggered.power_up.kind);
}

PowerUpLayer::PowerUpLayer(int w, int h) : width(w), height(h) {
    if (w < 1 || h < 1) throw std::invalid_argument("power-up layer: width and height must be at least 1");
    cells.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), std::nullopt);
}

std::optional<PowerUp> PowerUpLayer::at(Cell c) const noexcept {
    if (!in_bounds(c) || index(c) >= cells.size()) return std::nullopt;
    return cells[index(c)];
}

void PowerUpLayer::set(Cell c, PowerUp p) noexcept {
    if (in_bounds(c) && index(c) < cells.size()) cells[index(c)] = p;
}

void PowerUpLayer::erase(Cell c) noexcept {
    if (in_bounds(c) && index(c) < cells.size()) cells[index(c)].reset();
}

void PowerUpLayer::clear() noexcept { std::fill(cells.begin(), cells.end(), std::nullopt); }

int PowerUpLayer::count() const noexcept {
    return static_cast<int>(std::count_if(cells.begin(), cells.end(), [](const auto& c) { return c.has_value(); }));
}

std::vector<PowerUpPlacement> PowerUpLayer::entries() const {
    std::vector<PowerUpPlacement> out;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (const auto p = at({x, y})) out.push_back({{x, y}, *p});
    return out;
}

void PowerUpLayer::walk(PowerUpLayer& layer, const MoveResult& result, std::vector<TriggeredPowerUp>* fired) noexcept {
    // rule: <tbd> power-ups move with their pieces and trigger wherever the
    // piece is when it is cleared, by a match, a totem or another power-up.
    if (result.swap) {
        const Cell a = (*result.swap)[0];
        const Cell b = (*result.swap)[1];
        if (layer.in_bounds(a) && layer.in_bounds(b)) std::swap(layer.cells[layer.index(a)], layer.cells[layer.index(b)]);
    }
    std::vector<std::pair<Cell, PowerUp>> moving;
    for (std::size_t s = 0; s < result.cascades.size(); ++s) {
        const CascadeStep& step = result.cascades[s];
        for (const Cell c : step.cleared) {
            const auto p = layer.at(c);
            if (!p) continue;
            if (fired) fired->push_back({c, *p, static_cast<int>(s)});
            layer.erase(c);
        }
        // Falls happen at once: lift every moving power-up, then drop them.
        moving.clear();
        for (const Fall& f : step.falls) {
            if (const auto p = layer.at(f.from)) {
                moving.emplace_back(f.to, *p);
                layer.erase(f.from);
            }
        }
        for (const auto& [to, p] : moving) layer.set(to, p);
        // Spawns are new plain pieces: nothing to do.
    }
    if (result.shuffled) {
        if (result.redealt) {
            // [verify] a redeal replaces every piece, so the power-ups on
            // them are lost. rule: <tbd>
            layer.clear();
        } else {
            const std::vector<std::optional<PowerUp>> before = layer.cells;
            for (const auto& [from, to] : result.shuffle_map) {
                if (!layer.in_bounds(from) || !layer.in_bounds(to)) continue;
                layer.cells[layer.index(to)] = before[layer.index(from)];
            }
        }
    }
}

std::vector<TriggeredPowerUp> PowerUpLayer::triggered_by(const MoveResult& result) const noexcept {
    std::vector<TriggeredPowerUp> fired;
    PowerUpLayer scratch = *this;
    walk(scratch, result, &fired);
    return fired;
}

void PowerUpLayer::remap(const MoveResult& result) noexcept { walk(*this, result, nullptr); }

int PowerUpLayer::earned_by(const MoveResult& result) noexcept {
    // rule: <tbd> only a legal swap earns power-ups; an effect's clear_cells
    // result does not, even when its cascades make runs. [verify]
    if (!result.swap) return 0;
    return earned_by_runs(result);
}

std::vector<PowerUpPlacement> PowerUpLayer::place_from_move(const MoveResult& result, const Board& board, Rng& rng,
                                                            const PowerUpSet& unlocked, const PowerUpTiers& tiers) {
    validate_power_up_tiers(tiers);
    if (board.width() != width || board.height() != height ||
        cells.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        throw std::invalid_argument("power-up layer: size differs from the board");

    std::vector<PowerUpPlacement> placed;
    const int earned = earned_by(result);
    if (earned == 0) return placed;

    std::vector<PowerUpId> pool;
    for (const PowerUpDef& d : kPowerUpDefs)
        if (unlocked[power_up_index(d.id)]) pool.push_back(d.id);
    if (pool.empty()) return placed;

    std::vector<Cell> plain;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (board.at(x, y) != Board::kEmpty && !has({x, y})) plain.push_back({x, y});

    for (int i = 0; i < earned && !plain.empty(); ++i) {
        // rule: <tbd> [verify] the id is a uniform pick among the unlocked
        // power-ups. Draw order: id first, then cell.
        const PowerUpId id = pool[static_cast<std::size_t>(rng.below(static_cast<int>(pool.size())))];
        // rule: <tbd> placed on a random plain piece.
        const std::size_t pick = static_cast<std::size_t>(rng.below(static_cast<int>(plain.size())));
        const Cell c = plain[pick];
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(pick));
        const PowerUp p{id, tiers[power_up_index(id)], board.at(c.x, c.y)};
        set(c, p);
        placed.push_back({c, p});
    }
    return placed;
}

}  // namespace montezuma
