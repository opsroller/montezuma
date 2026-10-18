// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "match3_board.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>

#include <utility>

using namespace godot;

namespace montezuma::godot_binding {

void Match3Board::setup(int width, int height, int kinds, int64_t seed) {
    // godot-cpp builds without exceptions, so use the non-throwing factory.
    auto board = Board::try_create(width, height, kinds, static_cast<std::uint64_t>(seed));
    ERR_FAIL_COND_MSG(!board, "Invalid board: needs at least 3x3 cells and 3 kinds, and must be playable.");
    board_ = std::move(*board);
}

int Match3Board::get_cell(int x, int y) const {
    ERR_FAIL_COND_V(!board_.in_bounds(x, y), Board::kEmpty);
    return board_.at(x, y);
}

PackedInt32Array Match3Board::get_cells() const {
    PackedInt32Array out;
    out.resize(static_cast<int64_t>(board_.cells().size()));
    for (std::size_t i = 0; i < board_.cells().size(); ++i) out.set(static_cast<int64_t>(i), board_.cells()[i]);
    return out;
}

bool Match3Board::is_legal_swap(Vector2i a, Vector2i b) { return board_.is_legal_swap({a.x, a.y}, {b.x, b.y}); }

bool Match3Board::has_legal_move() { return board_.has_legal_move(); }

Dictionary Match3Board::try_swap(Vector2i a, Vector2i b) {
    Dictionary out;
    auto result = board_.try_swap({a.x, a.y}, {b.x, b.y});
    if (!result) return out;
    Array steps;
    for (const auto& step : result->steps) {
        Dictionary d;
        for (const auto& [kind, count] : step) d[kind] = count;
        steps.push_back(d);
    }
    out["steps"] = steps;
    out["matched_kind"] = result->matched_kind;
    out["shuffled"] = result->shuffled;
    out["score"] = result->score();
    return out;
}

void Match3Board::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "width", "height", "kinds", "seed"), &Match3Board::setup);
    ClassDB::bind_method(D_METHOD("get_width"), &Match3Board::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &Match3Board::get_height);
    ClassDB::bind_method(D_METHOD("get_kinds"), &Match3Board::get_kinds);
    ClassDB::bind_method(D_METHOD("get_cell", "x", "y"), &Match3Board::get_cell);
    ClassDB::bind_method(D_METHOD("get_cells"), &Match3Board::get_cells);
    ClassDB::bind_method(D_METHOD("is_legal_swap", "a", "b"), &Match3Board::is_legal_swap);
    ClassDB::bind_method(D_METHOD("has_legal_move"), &Match3Board::has_legal_move);
    ClassDB::bind_method(D_METHOD("try_swap", "a", "b"), &Match3Board::try_swap);
}

}  // namespace montezuma::godot_binding
