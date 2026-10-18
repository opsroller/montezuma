// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/board.hpp"

#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace montezuma {

std::map<int, int> MoveResult::cleared_by_kind() const {
    std::map<int, int> total;
    for (const auto& step : steps)
        for (const auto& [kind, count] : step) total[kind] += count;
    return total;
}

int MoveResult::score() const {
    int points = 0;
    for (std::size_t i = 0; i < steps.size(); ++i)
        for (const auto& [kind, count] : steps[i]) points += count * 10 * static_cast<int>(i + 1);
    return points;
}

Board::Board(int width, int height, int kinds, std::uint64_t seed)
    : width_(width), height_(height), kinds_(kinds), rng_(seed) {
    if (width < kMinSize || height < kMinSize) throw std::invalid_argument("Board must be at least 3x3");
    if (kinds < kMinKinds) throw std::invalid_argument("Board needs at least 3 artifact kinds");
    cells_.assign(static_cast<std::size_t>(width) * height, kEmpty);
    fill_fresh();
}

std::optional<Board> Board::try_create(int width, int height, int kinds, std::uint64_t seed) noexcept {
    try {
        return Board(width, height, kinds, seed);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void Board::fill_fresh() {
    for (int attempt = 0; attempt < kMaxFillAttempts; ++attempt) {
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x) set(x, y, pick_non_matching(x, y));
        if (has_legal_move()) return;
    }
    throw std::runtime_error("Could not generate a board with a legal move; use fewer kinds or a larger board");
}

int Board::pick_non_matching(int x, int y) {
    int kind = rng_.below(kinds_);
    for (int i = 0; i < kinds_; ++i) {
        bool row = x >= 2 && at(x - 1, y) == kind && at(x - 2, y) == kind;
        bool col = y >= 2 && at(x, y - 1) == kind && at(x, y - 2) == kind;
        if (!row && !col) return kind;
        kind = (kind + 1) % kinds_;
    }
    return kind;
}

std::set<int> Board::find_matches() const {
    std::set<int> matched;
    for (int y = 0; y < height_; ++y) {
        int x = 0;
        while (x < width_) {
            int kind = at(x, y);
            int end = x + 1;
            while (end < width_ && kind != kEmpty && at(end, y) == kind) ++end;
            if (kind != kEmpty && end - x >= 3)
                for (int i = x; i < end; ++i) matched.insert(index(i, y));
            x = end;
        }
    }
    for (int x = 0; x < width_; ++x) {
        int y = 0;
        while (y < height_) {
            int kind = at(x, y);
            int end = y + 1;
            while (end < height_ && kind != kEmpty && at(x, end) == kind) ++end;
            if (kind != kEmpty && end - y >= 3)
                for (int i = y; i < end; ++i) matched.insert(index(x, i));
            y = end;
        }
    }
    return matched;
}

void Board::swap_cells(Cell a, Cell b) { std::swap(cells_[index(a.x, a.y)], cells_[index(b.x, b.y)]); }

bool Board::is_legal_swap(Cell a, Cell b) {
    if (!in_bounds(a.x, a.y) || !in_bounds(b.x, b.y)) return false;
    if (std::abs(a.x - b.x) + std::abs(a.y - b.y) != 1) return false;
    swap_cells(a, b);
    bool ok = !find_matches().empty();
    swap_cells(a, b);
    return ok;
}

bool Board::has_legal_move() {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (x + 1 < width_ && is_legal_swap({x, y}, {x + 1, y})) return true;
            if (y + 1 < height_ && is_legal_swap({x, y}, {x, y + 1})) return true;
        }
    }
    return false;
}

std::optional<MoveResult> Board::try_swap(Cell a, Cell b) {
    if (!is_legal_swap(a, b)) return std::nullopt;
    swap_cells(a, b);
    MoveResult result;
    result.matched_kind = dominant_kind(find_matches());
    for (auto matched = find_matches(); !matched.empty(); matched = find_matches()) {
        std::map<int, int> step;
        for (int idx : matched) {
            ++step[cells_[idx]];
            cells_[idx] = kEmpty;
        }
        result.steps.push_back(std::move(step));
        collapse_and_refill();
    }
    if (!has_legal_move()) {
        shuffle();
        result.shuffled = true;
    }
    return result;
}

int Board::dominant_kind(const std::set<int>& matched) const {
    std::map<int, int> counts;
    int best = kEmpty;
    for (int idx : matched) {
        int kind = cells_[idx];
        int count = ++counts[kind];
        if (best == kEmpty || count > counts[best]) best = kind;
    }
    return best;
}

void Board::collapse_and_refill() {
    for (int x = 0; x < width_; ++x) {
        int write = height_ - 1;
        for (int y = height_ - 1; y >= 0; --y) {
            int kind = at(x, y);
            if (kind != kEmpty) set(x, write--, kind);
        }
        for (int y = write; y >= 0; --y) set(x, y, rng_.below(kinds_));
    }
}

void Board::shuffle() {
    for (int attempt = 0; attempt < kMaxShuffleAttempts; ++attempt) {
        for (int i = static_cast<int>(cells_.size()) - 1; i > 0; --i)
            std::swap(cells_[i], cells_[rng_.below(i + 1)]);
        if (find_matches().empty() && has_legal_move()) return;
    }
    // The current pieces can't be arranged into a playable board; deal a new one.
    fill_fresh();
}

}  // namespace montezuma
