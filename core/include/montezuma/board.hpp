// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "montezuma/rng.hpp"

namespace montezuma {

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

// Outcome of one legal swap, ordered for the view to animate.
struct MoveResult {
    // One entry per cascade step: artifact kind -> pieces cleared.
    std::vector<std::map<int, int>> steps;
    // Kind that made up most of the swap's own match; drives Power Totem streaks.
    int matched_kind = -1;
    bool shuffled = false;

    std::map<int, int> cleared_by_kind() const;
    // 10 points per piece, multiplied by the cascade step (1-based).
    int score() const;
};

// Pure match-3 board model. No rendering; the 3D layer observes results.
class Board {
public:
    static constexpr int kEmpty = -1;
    static constexpr int kMinSize = 3;
    static constexpr int kMinKinds = 3;

    // Throws std::invalid_argument for a board smaller than 3x3 or fewer than
    // 3 kinds, and std::runtime_error if no playable board can be dealt.
    Board(int width = 8, int height = 8, int kinds = 7, std::uint64_t seed = 0);
    // Non-throwing variant for callers built without exceptions (the Godot
    // extension). Empty when the constructor would throw.
    static std::optional<Board> try_create(int width, int height, int kinds, std::uint64_t seed) noexcept;

    int width() const { return width_; }
    int height() const { return height_; }
    int kinds() const { return kinds_; }
    const std::vector<int>& cells() const { return cells_; }

    int at(int x, int y) const {
        assert(in_bounds(x, y));
        return cells_[index(x, y)];
    }
    void set(int x, int y, int kind) {
        assert(in_bounds(x, y));
        cells_[index(x, y)] = kind;
    }
    bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

    // Fills the board with no pre-existing matches and at least one legal move.
    void fill_fresh();
    // Indices of every cell in a horizontal or vertical run of 3+.
    std::set<int> find_matches() const;
    bool is_legal_swap(Cell a, Cell b);
    bool has_legal_move();
    // Applies a swap and resolves all cascades. Empty for an illegal swap.
    std::optional<MoveResult> try_swap(Cell a, Cell b);
    // Reorders existing pieces until no match shows and a legal move exists.
    void shuffle();

private:
    static constexpr int kMaxFillAttempts = 1000;
    static constexpr int kMaxShuffleAttempts = 100;

    int index(int x, int y) const { return y * width_ + x; }
    int pick_non_matching(int x, int y);
    void swap_cells(Cell a, Cell b);
    int dominant_kind(const std::set<int>& matched) const;
    void collapse_and_refill();

    int width_;
    int height_;
    int kinds_;
    std::vector<int> cells_;
    Rng rng_;
};

}  // namespace montezuma
