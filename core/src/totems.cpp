// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/totems.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace montezuma {

namespace {

// In TotemId (kind) order.
constexpr std::array<std::string_view, kTotemCount> kNames{
    "fire", "time", "gold", "harvest", "ward", "gift", "paint",
};

bool row_major_less(const Cell& a, const Cell& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; }

void sort_row_major(std::vector<Cell>& cells) {
    std::sort(cells.begin(), cells.end(), row_major_less);
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
}

// Occupied cells in row-major order, optionally skipping one kind.
std::vector<Cell> occupied_cells(const Board& board, int skip_kind = Board::kEmpty) {
    std::vector<Cell> cells;
    for (int y = 0; y < board.height(); ++y)
        for (int x = 0; x < board.width(); ++x) {
            const int k = board.at(x, y);
            if (k != Board::kEmpty && k != skip_kind) cells.push_back({x, y});
        }
    return cells;
}

// Moves up to `n` random cells of `pool` into `out` (partial Fisher-Yates,
// one draw per cell taken). `used` counts the cells already taken from `pool`.
void take_random(std::vector<Cell>& pool, std::size_t& used, std::size_t n, Rng& rng, std::vector<Cell>& out) {
    const std::size_t take = std::min(n, pool.size() - used);
    for (std::size_t i = 0; i < take; ++i, ++used) {
        const auto pick = used + static_cast<std::size_t>(rng.below(static_cast<int>(pool.size() - used)));
        std::swap(pool[used], pool[pick]);
        out.push_back(pool[used]);
    }
}

void validate(const TotemDef& d, std::size_t i) {
    const std::string name(kNames[i]);
    auto fail = [&](const std::string& what) { throw std::invalid_argument("Totem " + name + " " + what); };
    if (static_cast<std::size_t>(d.id) != i) fail("table row has the wrong id");
    if (d.charges_to_fire < 1) fail("charges_to_fire must be at least 1");
    if (d.tier_count < 1 || d.tier_count > kMaxTotemTier) fail("tier_count must be 1..4");
    for (int t = 0; t < d.tier_count; ++t) {
        const TotemTierDef& td = d.tiers[static_cast<std::size_t>(t)];
        const std::string at = "tier " + std::to_string(t + 1) + " ";
        if (td.uses_per_level < 1 || td.uses_per_level > kMaxTotemUses) fail(at + "uses_per_level must be 1..2");
        for (const Fireballs& f : td.fireballs)
            if (f.count < 0 || f.size < 0) fail(at + "fireballs must not be negative");
        if (!std::isfinite(td.time_seconds) || td.time_seconds < 0.0)
            fail(at + "time_seconds must be finite and not negative");
        for (int p : td.points)
            if (p < 0) fail(at + "points must not be negative");
        if (td.powerups_to_place < 0) fail(at + "powerups_to_place must not be negative");
        if (td.gems_to_place < 0) fail(at + "gems_to_place must not be negative");
        if (td.repaint_count < 0) fail(at + "repaint_count must not be negative");
    }
}

}  // namespace

std::string_view totem_name(TotemId id) { return kNames[static_cast<std::size_t>(id)]; }

std::optional<TotemId> totem_from_name(std::string_view name) {
    for (std::size_t i = 0; i < kTotemCount; ++i)
        if (kNames[i] == name) return static_cast<TotemId>(i);
    return std::nullopt;
}

TotemSystem::TotemSystem(const std::vector<TotemUnlock>& unlocked, const TotemTable& defs) : defs_(defs) {
    for (std::size_t i = 0; i < kTotemCount; ++i) validate(defs_[i], i);
    for (const TotemUnlock& u : unlocked) unlock(u.id, u.tier);
}

TotemSystem TotemSystem::from_names(const std::vector<std::string_view>& unlocked, const TotemTable& defs) {
    std::vector<TotemUnlock> ids;
    for (std::string_view name : unlocked) {
        auto id = totem_from_name(name);
        if (!id) throw std::invalid_argument("Unknown totem id: " + std::string(name));
        ids.push_back({*id, 1});
    }
    return TotemSystem(ids, defs);
}

void TotemSystem::unlock(TotemId id, int tier) {
    const TotemDef& d = def(id);
    if (tier < 1 || tier > d.tier_count)
        throw std::invalid_argument("Totem " + std::string(totem_name(id)) + " has no tier " + std::to_string(tier));
    State& s = state_[index(id)];
    s.unlocked = true;
    s.tier = tier;
    s.uses_left = d.tiers[static_cast<std::size_t>(tier - 1)].uses_per_level;
    s.uses_spent = 0;
}

const TotemTierDef& TotemSystem::tier_def(TotemId id) const {
    return def(id).tiers[static_cast<std::size_t>(tier(id) - 1)];
}

void TotemSystem::start_level() {
    // rule: TOT-06 uses refill each level; charges start empty.
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        State& s = state_[i];
        s.charge = 0;
        s.uses_spent = 0;
        if (s.unlocked) s.uses_left = tier_def(static_cast<TotemId>(i)).uses_per_level;
    }
}

double TotemSystem::charge(TotemId id) const {
    const State& s = state_[index(id)];
    // rule: TOT-06 a spent totem shows empty.
    if (!s.unlocked || s.uses_left == 0) return 0.0;
    return std::clamp(static_cast<double>(s.charge) / def(id).charges_to_fire, 0.0, 1.0);
}

std::vector<TotemId> TotemSystem::record_move(const MoveResult& result) {
    std::vector<TotemId> fired;
    // rule: TOT-02, TOT-07 only a player swap charges; effects (clear_cells
    // results), cascades, shuffles and hints never do.
    if (!result.swap || result.cascades.empty()) return fired;
    // rule: TOT-02, TOT-04 the kinds of the runs in step 0, every one of them.
    std::set<int> kinds;
    for (const Run& run : result.cascades.front().runs) kinds.insert(run.kind);
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        State& s = state_[i];
        // rule: TOT-01 only owned totems with uses left take part.
        if (!s.unlocked || s.uses_left == 0) continue;
        s.charge = kinds.count(static_cast<int>(i)) != 0 ? s.charge + 1 : 0;
        // rule: TOT-03 fires at the charge threshold: charge 0, one use spent.
        if (s.charge >= defs_[i].charges_to_fire) {
            s.charge = 0;
            --s.uses_left;
            ++s.uses_spent;
            fired.push_back(static_cast<TotemId>(i));
        }
    }
    // rule: TOT-05 kind order.
    return fired;
}

TotemEffect TotemSystem::make_effect(TotemId id, const Board& board, Rng& rng) const {
    const TotemTierDef& td = tier_def(id);
    TotemEffect effect;
    effect.totem = id;
    effect.tier = tier(id);
    effect.use = std::clamp(state_[index(id)].uses_spent, 1, td.uses_per_level);

    switch (id) {
    case TotemId::Fire: {
        // rule: TOT-10, TOT-17 One shared pool of every piece (steel
        // included once it exists): fireballs in table order each take
        // `size` distinct pieces, so no target repeats, until the board runs
        // out.
        std::vector<Cell> pool = occupied_cells(board);
        std::size_t used = 0;
        for (const Fireballs& group : td.fireballs)
            for (int f = 0; f < group.count; ++f)
                take_random(pool, used, static_cast<std::size_t>(group.size), rng, effect.cells_to_clear);
        sort_row_major(effect.cells_to_clear);
        break;
    }
    case TotemId::Time:
        // rule: TOT-11
        effect.time_delta_seconds = td.time_seconds;
        break;
    case TotemId::Gold:
        // rule: TOT-12 points depend on the use of the level.
        effect.points = td.points[static_cast<std::size_t>(effect.use - 1)];
        effect.start_frenzy = td.start_frenzy;
        break;
    case TotemId::Harvest:
        // rule: TOT-13
        effect.collect_all_gems = td.collect_all_gems;
        effect.gems_to_place = td.gems_to_place;
        effect.remove_chains = td.remove_chains;
        break;
    case TotemId::Ward:
        // rule: TOT-14
        effect.destroy_steel = td.destroy_steel;
        effect.stop_steel = td.stop_steel;
        effect.clear_around_steel = td.clear_around_steel;
        break;
    case TotemId::Gift:
        // rule: TOT-15
        effect.powerups_to_place = td.powerups_to_place;
        effect.powerup_board_cap = kGiftPowerupCap;
        break;
    case TotemId::Paint: {
        // rule: TOT-16, TOT-17 Target kind first, uniform among the kinds
        // present (ascending), then the cells, from pieces of other kinds.
        // Fewer than two kinds: nothing to repaint, no draws.
        std::vector<int> present;
        for (int k : board.cells())
            if (k != Board::kEmpty) present.push_back(k);
        std::sort(present.begin(), present.end());
        present.erase(std::unique(present.begin(), present.end()), present.end());
        if (present.size() < 2 || td.repaint_count == 0) break;
        effect.repaint_kind = present[static_cast<std::size_t>(rng.below(static_cast<int>(present.size())))];
        // Once steel exists in Board, steel pieces leave this pool too.
        std::vector<Cell> pool = occupied_cells(board, effect.repaint_kind);
        std::size_t used = 0;
        take_random(pool, used, static_cast<std::size_t>(td.repaint_count), rng, effect.cells_to_repaint);
        sort_row_major(effect.cells_to_repaint);
        break;
    }
    }
    return effect;
}

std::vector<Cell> prepare_board_effect(const TotemEffect& effect, Board& board) {
    // rule: TOT-16 repaint; gems and power-ups (game layer) stay put.
    for (Cell c : effect.cells_to_repaint)
        if (board.in_bounds(c.x, c.y) && board.at(c.x, c.y) != Board::kEmpty) board.set(c.x, c.y, effect.repaint_kind);
    // rule: TOT-05, RES-07 the effect's clears, plus any runs the repaint
    // made, resolve fully (fall, refill, cascades) before the next totem.
    std::vector<Cell> cells = effect.cells_to_clear;
    for (const Run& run : board.find_runs()) cells.insert(cells.end(), run.cells.begin(), run.cells.end());
    sort_row_major(cells);
    return cells;
}

std::optional<MoveResult> apply_board_effect(const TotemEffect& effect, Board& board, const StepHooks* hooks) {
    const std::vector<Cell> cells = prepare_board_effect(effect, board);
    if (cells.empty()) return std::nullopt;
    return board.clear_cells(cells, hooks);
}

}  // namespace montezuma
