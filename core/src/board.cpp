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

std::vector<Run> Board::find_runs() const { return find_runs(nullptr); }

int Board::match_kind(int x, int y, const StepHooks* hooks) const {
    const int kind = at(x, y);
    if (kind == kEmpty || !hooks || !hooks->matchable) return kind;
    return hooks->matchable(*this, Cell{x, y}) ? kind : kEmpty;
}

std::vector<Run> Board::find_runs(const StepHooks* hooks) const {
    std::vector<Run> runs;
    for (int y = 0; y < height_; ++y) {
        int x = 0;
        while (x < width_) {
            int kind = match_kind(x, y, hooks);
            int end = x + 1;
            while (end < width_ && kind != kEmpty && match_kind(end, y, hooks) == kind) ++end;
            if (kind != kEmpty && end - x >= 3) {
                Run run{{}, kind};
                for (int i = x; i < end; ++i) run.cells.push_back({i, y});
                runs.push_back(std::move(run));
            }
            x = end;
        }
    }
    for (int x = 0; x < width_; ++x) {
        int y = 0;
        while (y < height_) {
            int kind = match_kind(x, y, hooks);
            int end = y + 1;
            while (end < height_ && kind != kEmpty && match_kind(x, end, hooks) == kind) ++end;
            if (kind != kEmpty && end - y >= 3) {
                Run run{{}, kind};
                for (int i = y; i < end; ++i) run.cells.push_back({x, i});
                runs.push_back(std::move(run));
            }
            y = end;
        }
    }
    return runs;
}

bool Board::is_legal_swap(Cell a, Cell b, const StepHooks* hooks) {
    if (!hooks || (!hooks->matchable && !hooks->movable)) return is_legal_swap(a, b);
    if (!in_bounds(a.x, a.y) || !in_bounds(b.x, b.y)) return false;
    if (std::abs(a.x - b.x) + std::abs(a.y - b.y) != 1) return false;
    if (hooks->movable && (!hooks->movable(*this, a) || !hooks->movable(*this, b))) return false;
    swap_cells(a, b);
    bool ok = !find_runs(hooks).empty();
    swap_cells(a, b);
    return ok;
}

bool Board::has_legal_move(const StepHooks* hooks) {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (x + 1 < width_ && is_legal_swap({x, y}, {x + 1, y}, hooks)) return true;
            if (y + 1 < height_ && is_legal_swap({x, y}, {x, y + 1}, hooks)) return true;
        }
    }
    return false;
}

std::optional<MoveResult> Board::try_swap(Cell a, Cell b, const StepHooks* hooks) {
    if (!is_legal_swap(a, b, hooks)) return std::nullopt;
    swap_cells(a, b);
    MoveResult result;
    result.swap = std::array<Cell, 2>{a, b};
    std::set<int> matched;
    for (const Run& run : find_runs(hooks))
        for (Cell c : run.cells) matched.insert(index(c.x, c.y));
    result.matched_kind = dominant_kind(matched);
    resolve_and_settle(result, hooks);
    return result;
}

std::optional<MoveResult> Board::clear_cells(const std::vector<Cell>& cells, const StepHooks* hooks) {
    std::set<int> targets;
    for (Cell c : cells)
        if (in_bounds(c.x, c.y) && at(c.x, c.y) != kEmpty) targets.insert(index(c.x, c.y));
    if (targets.empty()) return std::nullopt;
    MoveResult result;
    clear_step(std::move(targets), CascadeStep{}, result, hooks);
    resolve_and_settle(result, hooks);
    return result;
}

void Board::resolve_and_settle(MoveResult& result, const StepHooks* hooks) {
    for (auto runs = find_runs(hooks); !runs.empty(); runs = find_runs(hooks)) {
        if (hooks && hooks->max_steps > 0 && static_cast<int>(result.cascades.size()) >= hooks->max_steps) {
            result.stuck = true;
            return;
        }
        std::set<int> matched;
        for (const Run& run : runs)
            for (Cell c : run.cells) matched.insert(index(c.x, c.y));
        CascadeStep step;
        step.runs = std::move(runs);
        clear_step(std::move(matched), std::move(step), result, hooks);
    }
    if (hooks && !hooks->settle_shuffle) return;
    if (!has_legal_move(hooks)) {
        result.shuffled = shuffle(hooks, result.shuffle_map, result.redealt);
        result.stuck = !result.shuffled;
    }
}

void Board::clear_step(std::set<int> matched, CascadeStep step, MoveResult& result, const StepHooks* hooks) {
    if (hooks && hooks->extend_clear) {
        std::vector<Cell> so_far;
        so_far.reserve(matched.size());
        for (int idx : matched) so_far.push_back(cell_of(idx));
        const int index_of_step = static_cast<int>(result.cascades.size());
        for (Cell c : hooks->extend_clear(*this, so_far, index_of_step))
            if (in_bounds(c.x, c.y) && at(c.x, c.y) != kEmpty) matched.insert(index(c.x, c.y));
    }
    std::map<int, int> counts;
    // std::set iterates indices in ascending order, which is row-major.
    for (int idx : matched) {
        ++counts[cells_[idx]];
        cells_[idx] = kEmpty;
        step.cleared.push_back(cell_of(idx));
    }
    collapse_and_refill(step);
    result.steps.push_back(std::move(counts));
    result.cascades.push_back(std::move(step));
    if (hooks && hooks->after_refill)
        hooks->after_refill(*this, result.cascades.back(), static_cast<int>(result.cascades.size()) - 1);
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

void Board::collapse_and_refill(CascadeStep& step) {
    for (int x = 0; x < width_; ++x) {
        int write = height_ - 1;
        for (int y = height_ - 1; y >= 0; --y) {
            int kind = at(x, y);
            if (kind == kEmpty) continue;
            if (write != y) step.falls.push_back({{x, y}, {x, write}, kind});
            set(x, write--, kind);
        }
        // Rows 0..write are now free; the piece for row y starts above the
        // board at y - (write + 1), so the lowest new piece starts one row up.
        const int fresh = write + 1;
        for (int y = write; y >= 0; --y) {
            int kind = rng_.below(kinds_);
            set(x, y, kind);
            step.spawns.push_back({{x, y}, kind, fresh - y});
        }
    }
}

bool Board::shuffle() noexcept {
    std::vector<std::pair<Cell, Cell>> map;
    bool redealt = false;
    return shuffle_tracked(map, redealt);
}

bool Board::shuffle(const StepHooks* hooks, std::vector<std::pair<Cell, Cell>>& map, bool& redealt) noexcept {
    if (!hooks || (!hooks->matchable && !hooks->movable)) return shuffle_tracked(map, redealt);
    return shuffle_movable(*hooks, map, redealt);
}

bool Board::shuffle_movable(const StepHooks& hooks, std::vector<std::pair<Cell, Cell>>& map, bool& redealt) noexcept {
    map.clear();
    redealt = false;
    const std::vector<int> before = cells_;
    std::vector<int> movable;
    for (int i = 0; i < static_cast<int>(cells_.size()); ++i)
        if (cells_[i] != kEmpty && (!hooks.movable || hooks.movable(*this, cell_of(i)))) movable.push_back(i);
    std::vector<int> origin(cells_.size());
    for (std::size_t i = 0; i < origin.size(); ++i) origin[i] = static_cast<int>(i);
    const auto playable = [&] { return find_runs(&hooks).empty() && has_legal_move(&hooks); };
    for (int attempt = 0; attempt < kMaxShuffleAttempts && movable.size() > 1; ++attempt) {
        for (int i = static_cast<int>(movable.size()) - 1; i > 0; --i) {
            int j = rng_.below(i + 1);
            std::swap(cells_[movable[i]], cells_[movable[j]]);
            std::swap(origin[movable[i]], origin[movable[j]]);
        }
        if (playable()) {
            for (int i = 0; i < static_cast<int>(origin.size()); ++i)
                if (origin[i] != i) map.emplace_back(cell_of(origin[i]), cell_of(i));
            return true;
        }
    }
    // Deal the movable cells afresh; the fixed pieces stay.
    for (int attempt = 0; attempt < kMaxFillAttempts && !movable.empty(); ++attempt) {
        for (int idx : movable) {
            Cell c = cell_of(idx);
            cells_[idx] = pick_non_matching(c.x, c.y);
        }
        if (playable()) {
            redealt = true;
            return true;
        }
    }
    cells_ = before;
    return false;
}

bool Board::shuffle_tracked(std::vector<std::pair<Cell, Cell>>& map, bool& redealt) noexcept {
    map.clear();
    redealt = false;
    const std::vector<int> before = cells_;
    // origin[i] = where the piece now at i stood before the shuffle.
    std::vector<int> origin(cells_.size());
    for (std::size_t i = 0; i < origin.size(); ++i) origin[i] = static_cast<int>(i);
    for (int attempt = 0; attempt < kMaxShuffleAttempts; ++attempt) {
        for (int i = static_cast<int>(cells_.size()) - 1; i > 0; --i) {
            int j = rng_.below(i + 1);
            std::swap(cells_[i], cells_[j]);
            std::swap(origin[i], origin[j]);
        }
        if (find_matches().empty() && has_legal_move()) {
            for (int i = 0; i < static_cast<int>(origin.size()); ++i)
                if (origin[i] != i) map.emplace_back(cell_of(origin[i]), cell_of(i));
            return true;
        }
    }
    // The current pieces can't be arranged into a playable board; deal a new one.
    try {
        fill_fresh();
        redealt = true;
        return true;
    } catch (const std::exception&) {
        cells_ = before;
        return false;
    }
}

}  // namespace montezuma
