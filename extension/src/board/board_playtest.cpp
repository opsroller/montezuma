// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "board_playtest.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "board_view_3d.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {
constexpr int kWidth = 8;
constexpr int kHeight = 8;
constexpr int kKinds = 7;
}  // namespace

BoardView3D* BoardPlaytest::view() const {
    if (board_view_path_.is_empty() || !is_inside_tree()) return nullptr;
    return Object::cast_to<BoardView3D>(get_node_or_null(board_view_path_));
}

void BoardPlaytest::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    BoardView3D* v = view();
    ERR_FAIL_NULL_MSG(v, "BoardPlaytest needs board_view_path to point at a BoardView3D.");
    board_.instantiate();
    board_->setup(kWidth, kHeight, kKinds, seed_);
    v->connect("swap_requested", callable_mp(this, &BoardPlaytest::on_swap_requested));
    v->connect("animation_finished", callable_mp(this, &BoardPlaytest::on_animation_finished));
    v->show_board(board_);
}

void BoardPlaytest::_exit_tree() {
    BoardView3D* v = view();
    if (!v) return;
    const Callable swap = callable_mp(this, &BoardPlaytest::on_swap_requested);
    const Callable done = callable_mp(this, &BoardPlaytest::on_animation_finished);
    if (v->is_connected("swap_requested", swap)) v->disconnect("swap_requested", swap);
    if (v->is_connected("animation_finished", done)) v->disconnect("animation_finished", done);
}

void BoardPlaytest::on_swap_requested(Vector2i a, Vector2i b) {
    BoardView3D* v = view();
    if (!v || v->is_busy()) return;
    const Dictionary result = board_->try_swap(a, b);
    if (result.is_empty()) {
        v->play_illegal(a, b);
        return;
    }
    score_ += static_cast<int>(result["score"]);
    // No playable arrangement is left: deal a new board once this move is shown.
    redeal_ = static_cast<bool>(result["stuck"]);
    v->play(result);
}

void BoardPlaytest::on_animation_finished() {
    if (!redeal_) return;
    redeal_ = false;
    board_->setup(kWidth, kHeight, kKinds, ++seed_);
    if (BoardView3D* v = view()) v->show_board(board_);
}

void BoardPlaytest::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_board_view_path", "path"), &BoardPlaytest::set_board_view_path);
    ClassDB::bind_method(D_METHOD("get_board_view_path"), &BoardPlaytest::get_board_view_path);
    ClassDB::bind_method(D_METHOD("set_seed", "seed"), &BoardPlaytest::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &BoardPlaytest::get_seed);
    ClassDB::bind_method(D_METHOD("get_board"), &BoardPlaytest::get_board);
    ClassDB::bind_method(D_METHOD("get_score"), &BoardPlaytest::get_score);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "board_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BoardView3D"),
                 "set_board_view_path", "get_board_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
}

}  // namespace montezuma::godot_binding
