// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/totems.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace montezuma {

namespace {

constexpr std::array<std::string_view, kTotemCount> kNames{
    "row_clear", "random_blast", "time", "lightning", "color_bomb", "multiplier",
};

void sort_row_major(std::vector<Cell>& cells) {
    std::sort(cells.begin(), cells.end(),
              [](const Cell& a, const Cell& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
}

std::vector<Cell> occupied_cells(const Board& board) {
    std::vector<Cell> cells;
    for (int y = 0; y < board.height(); ++y)
        for (int x = 0; x < board.width(); ++x)
            if (board.at(x, y) != Board::kEmpty) cells.push_back({x, y});
    return cells;
}

void add_row(std::vector<Cell>& cells, const Board& board, int y) {
    for (int x = 0; x < board.width(); ++x)
        if (board.at(x, y) != Board::kEmpty) cells.push_back({x, y});
}

void add_column(std::vector<Cell>& cells, const Board& board, int x) {
    for (int y = 0; y < board.height(); ++y)
        if (board.at(x, y) != Board::kEmpty) cells.push_back({x, y});
}

}  // namespace

std::string_view totem_name(TotemId id) { return kNames[static_cast<std::size_t>(id)]; }

std::optional<TotemId> totem_from_name(std::string_view name) {
    for (std::size_t i = 0; i < kTotemCount; ++i)
        if (kNames[i] == name) return static_cast<TotemId>(i);
    return std::nullopt;
}

TotemSystem::TotemSystem(const std::vector<TotemId>& unlocked, const TotemTable& defs) : defs_(defs) {
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        const TotemDef& d = defs_[i];
        std::string name(kNames[i]);
        if (index(d.id) != i) throw std::invalid_argument("Totem table row " + name + " has the wrong id");
        if (d.threshold < 1) throw std::invalid_argument("Totem " + name + " threshold must be at least 1");
        if (d.blast_count < 0) throw std::invalid_argument("Totem " + name + " blast_count must not be negative");
        if (d.score_multiplier < 1)
            throw std::invalid_argument("Totem " + name + " score_multiplier must be at least 1");
        if (d.multiplier_moves < 0)
            throw std::invalid_argument("Totem " + name + " multiplier_moves must not be negative");
        if (!std::isfinite(d.time_delta_seconds))
            throw std::invalid_argument("Totem " + name + " time_delta_seconds must be finite");
    }
    for (TotemId id : unlocked) unlock(id);
}

TotemSystem TotemSystem::from_names(const std::vector<std::string_view>& unlocked, const TotemTable& defs) {
    std::vector<TotemId> ids;
    for (std::string_view name : unlocked) {
        auto id = totem_from_name(name);
        if (!id) throw std::invalid_argument("Unknown totem id: " + std::string(name));
        ids.push_back(*id);
    }
    return TotemSystem(ids, defs);
}

double TotemSystem::charge(TotemId id) const {
    if (!unlocked(id)) return 0.0;
    double fraction = static_cast<double>(charge_count(id)) / def(id).threshold;
    return std::clamp(fraction, 0.0, 1.0);
}

std::vector<TotemId> TotemSystem::record_match(int kind) {
    std::vector<TotemId> fired;
    if (kind < 0) {
        streak_kind_ = -1;
        streak_ = 0;
        charge_.fill(0);
        return fired;
    }
    const bool extends = kind == streak_kind_;
    if (extends) {
        ++streak_;
    } else {
        streak_kind_ = kind;
        streak_ = 1;
    }
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        if (!unlocked_[i]) continue;
        charge_[i] = extends ? charge_[i] + 1 : 1;
        if (charge_[i] >= defs_[i].threshold) {
            charge_[i] = 0;
            fired.push_back(static_cast<TotemId>(i));
        }
    }
    return fired;
}

TotemEffect TotemSystem::make_effect(TotemId id, const Board& board, Rng& rng, int target_kind) const {
    const TotemDef& d = def(id);
    TotemEffect effect;
    effect.totem = id;
    auto& cells = effect.cells_to_clear;

    switch (id) {
    case TotemId::RowClear:
        add_row(cells, board, rng.below(board.height()));
        break;
    case TotemId::RandomBlast: {
        // Partial Fisher-Yates over occupied cells in row-major order.
        std::vector<Cell> pool = occupied_cells(board);
        const std::size_t n = std::min(pool.size(), static_cast<std::size_t>(d.blast_count));
        for (std::size_t i = 0; i < n; ++i) {
            const auto pick = i + static_cast<std::size_t>(rng.below(static_cast<int>(pool.size() - i)));
            std::swap(pool[i], pool[pick]);
            cells.push_back(pool[i]);
        }
        break;
    }
    case TotemId::Time:
        effect.time_delta_seconds = d.time_delta_seconds;
        break;
    case TotemId::Lightning:
        if (d.lightning_shape == LightningShape::Column) {
            add_column(cells, board, rng.below(board.width()));
        } else {
            const int x = rng.below(board.width());
            const int y = rng.below(board.height());
            add_column(cells, board, x);
            add_row(cells, board, y);
        }
        break;
    case TotemId::ColorBomb: {
        // Aim at the streak's kind; if none of it is on the board, pick a
        // kind that is, from those present in ascending order.
        std::vector<int> present;
        for (int k : board.cells())
            if (k != Board::kEmpty) present.push_back(k);
        std::sort(present.begin(), present.end());
        present.erase(std::unique(present.begin(), present.end()), present.end());
        if (present.empty()) break;
        int kind = target_kind;
        if (!std::binary_search(present.begin(), present.end(), kind))
            kind = present[static_cast<std::size_t>(rng.below(static_cast<int>(present.size())))];
        for (int y = 0; y < board.height(); ++y)
            for (int x = 0; x < board.width(); ++x)
                if (board.at(x, y) == kind) cells.push_back({x, y});
        break;
    }
    case TotemId::Multiplier:
        effect.score_multiplier = d.score_multiplier;
        effect.multiplier_moves = d.multiplier_moves;
        break;
    }
    sort_row_major(cells);
    return effect;
}

std::vector<TotemEffect> TotemSystem::apply_move(const MoveResult& result, const Board& board, Rng& rng) {
    std::vector<TotemEffect> effects;
    for (TotemId id : record_match(result.matched_kind))
        effects.push_back(make_effect(id, board, rng, result.matched_kind));
    return effects;
}

}  // namespace montezuma
