// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "montezuma/rng.hpp"

namespace montezuma {

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

// A piece moving down inside its column during a collapse.
struct Fall {
    Cell from;
    Cell to;  // same x, larger y
    int kind = -1;
    bool operator==(const Fall&) const = default;
};

// A new piece entering from above the board. It starts `drop` rows above the
// top edge (at y = -drop, so drop >= 1) and falls to `to`.
struct Spawn {
    Cell to;
    int kind = -1;
    int drop = 1;
    bool operator==(const Spawn&) const = default;
};

// One straight horizontal or vertical line of 3+ pieces of the same kind.
// Cells run left to right (horizontal) or top to bottom (vertical). Runs may
// share a cell (L and T shapes show up as two crossing runs).
struct Run {
    std::vector<Cell> cells;
    int kind = -1;
    bool horizontal() const { return cells.size() >= 2 && cells[0].y == cells[1].y; }
    bool operator==(const Run&) const = default;
};

// What happened in one cascade step, in the order it happens:
// runs found -> cleared emptied -> falls move down -> spawns drop in.
struct CascadeStep {
    // Row-major order (y, then x). Each cell once, however many runs share it.
    std::vector<Cell> cleared;
    // Column by column left to right, bottom to top within a column.
    std::vector<Fall> falls;
    // Column by column left to right, bottom to top within a column.
    std::vector<Spawn> spawns;
    // Every horizontal run (rows top to bottom, left to right), then every
    // vertical run (columns left to right, top to bottom). Empty for the
    // effect-driven first step of Board::clear_cells.
    std::vector<Run> runs;
    bool operator==(const CascadeStep&) const = default;
};

// Outcome of one legal swap (or a clear_cells effect), ordered for the view to
// animate. See docs/reference/move-result.md.
struct MoveResult {
    // One entry per cascade step: artifact kind -> pieces cleared. Always the
    // same length as `cascades`, and step i counts cascades[i].cleared.
    std::vector<std::map<int, int>> steps;
    // Kind that made up most of the swap's own match; drives Power Totem
    // streaks. -1 for clear_cells, which is not a player match.
    int matched_kind = -1;
    bool shuffled = false;
    // No legal move was left and no playable arrangement could be found; the
    // caller should end or restart the level. Practically unreachable for
    // boards that passed construction, but never thrown.
    bool stuck = false;

    // The two cells the player swapped, as passed to try_swap. Empty for
    // clear_cells.
    std::optional<std::array<Cell, 2>> swap;
    // One per cascade step; step 0 is the swap's own match (or the cells
    // clear_cells removed).
    std::vector<CascadeStep> cascades;
    // When `shuffled` and not `redealt`: every piece that changed place in the
    // shuffle as {from, to}, ordered by `to` row-major. Pieces not listed
    // stayed where they were.
    std::vector<std::pair<Cell, Cell>> shuffle_map;
    // When `shuffled`: the existing pieces could not be rearranged, so a new
    // board was dealt. `shuffle_map` is empty; read Board::cells() for the new
    // layout.
    bool redealt = false;

    std::map<int, int> cleared_by_kind() const;
    // 10 points per piece, multiplied by the cascade step (1-based).
    int score() const;
    bool operator==(const MoveResult&) const = default;
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
    // Every run of 3+, in CascadeStep::runs order.
    std::vector<Run> find_runs() const;
    bool is_legal_swap(Cell a, Cell b);
    bool has_legal_move();
    // Applies a swap and resolves all cascades. Empty for an illegal swap.
    // Never throws for game reasons (see MoveResult::stuck).
    std::optional<MoveResult> try_swap(Cell a, Cell b);
    // Clears the given cells (for totem and bonus effects), then collapses,
    // refills, resolves cascades and shuffles exactly like a swap. Duplicate,
    // out-of-bounds and empty cells are ignored; empty when nothing valid is
    // left. Never throws for game reasons.
    std::optional<MoveResult> clear_cells(const std::vector<Cell>& cells);
    // Reorders existing pieces until no match shows and a legal move exists,
    // dealing a fresh board if reordering fails. Returns false, leaving the
    // board unchanged, if neither works.
    bool shuffle() noexcept;

private:
    static constexpr int kMaxFillAttempts = 1000;
    static constexpr int kMaxShuffleAttempts = 100;

    int index(int x, int y) const { return y * width_ + x; }
    int pick_non_matching(int x, int y);
    void swap_cells(Cell a, Cell b);
    Cell cell_of(int idx) const { return {idx % width_, idx / width_}; }
    int dominant_kind(const std::set<int>& matched) const;
    // Empties `matched`, collapses and refills, recording it all in `step`
    // and its per-kind counts in `result.steps`.
    void clear_step(const std::set<int>& matched, CascadeStep step, MoveResult& result);
    void collapse_and_refill(CascadeStep& step);
    // Resolves every remaining run, then shuffles if no legal move is left.
    void resolve_and_settle(MoveResult& result);
    bool shuffle_tracked(std::vector<std::pair<Cell, Cell>>& map, bool& redealt) noexcept;

    int width_;
    int height_;
    int kinds_;
    std::vector<int> cells_;
    Rng rng_;
};

}  // namespace montezuma
