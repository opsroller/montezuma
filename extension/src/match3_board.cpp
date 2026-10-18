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

namespace {

Vector2i to_vec(Cell c) { return Vector2i(c.x, c.y); }

Array to_array(const std::vector<Cell>& cells) {
    Array out;
    for (Cell c : cells) out.push_back(to_vec(c));
    return out;
}

}  // namespace

Dictionary Match3Board::to_dictionary(const MoveResult& result) {
    Dictionary out;
    Array steps;
    for (const auto& step : result.steps) {
        Dictionary d;
        for (const auto& [kind, count] : step) d[kind] = count;
        steps.push_back(d);
    }
    out["steps"] = steps;
    out["matched_kind"] = result.matched_kind;
    out["shuffled"] = result.shuffled;
    out["stuck"] = result.stuck;
    out["score"] = result.score();
    if (result.swap) {
        Array swap;
        swap.push_back(to_vec((*result.swap)[0]));
        swap.push_back(to_vec((*result.swap)[1]));
        out["swap"] = swap;
    }
    Array cascades;
    for (const CascadeStep& step : result.cascades) {
        Array falls;
        for (const Fall& f : step.falls) {
            Dictionary d;
            d["from"] = to_vec(f.from);
            d["to"] = to_vec(f.to);
            d["kind"] = f.kind;
            falls.push_back(d);
        }
        Array spawns;
        for (const Spawn& sp : step.spawns) {
            Dictionary d;
            d["to"] = to_vec(sp.to);
            d["kind"] = sp.kind;
            d["drop"] = sp.drop;
            spawns.push_back(d);
        }
        Array runs;
        for (const Run& run : step.runs) {
            Dictionary d;
            d["cells"] = to_array(run.cells);
            d["kind"] = run.kind;
            runs.push_back(d);
        }
        Dictionary d;
        d["cleared"] = to_array(step.cleared);
        d["falls"] = falls;
        d["spawns"] = spawns;
        d["runs"] = runs;
        cascades.push_back(d);
    }
    out["cascades"] = cascades;
    Array shuffle_map;
    for (const auto& [from, to] : result.shuffle_map) {
        Array pair;
        pair.push_back(to_vec(from));
        pair.push_back(to_vec(to));
        shuffle_map.push_back(pair);
    }
    out["shuffle_map"] = shuffle_map;
    out["redealt"] = result.redealt;
    return out;
}

Dictionary Match3Board::try_swap(Vector2i a, Vector2i b) {
    auto result = board_.try_swap({a.x, a.y}, {b.x, b.y});
    return result ? to_dictionary(*result) : Dictionary();
}

Dictionary Match3Board::clear_cells(const Array& cells) {
    std::vector<Cell> targets;
    for (int64_t i = 0; i < cells.size(); ++i) {
        ERR_CONTINUE_MSG(cells[i].get_type() != Variant::VECTOR2I, "clear_cells expects an Array of Vector2i.");
        const Vector2i v = cells[i];
        targets.push_back({v.x, v.y});
    }
    auto result = board_.clear_cells(targets);
    return result ? to_dictionary(*result) : Dictionary();
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
    ClassDB::bind_method(D_METHOD("clear_cells", "cells"), &Match3Board::clear_cells);
}

}  // namespace montezuma::godot_binding
