// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "match3_board.hpp"

namespace montezuma::godot_binding {

class BoardView3D;

// Free play on one board, with no level, timer or HUD: it owns a Match3Board,
// answers the view's swap requests with try_swap, and hands the result back
// to the view to animate. The board scene runs on its own with it; the game
// scene (Match3Game, WP-28) takes this job over for real levels.
class BoardPlaytest : public godot::Node3D {
    GDCLASS(BoardPlaytest, godot::Node3D)

public:
    void _ready() override;
    void _exit_tree() override;

    void set_board_view_path(const godot::NodePath& p) { board_view_path_ = p; }
    godot::NodePath get_board_view_path() const { return board_view_path_; }
    void set_seed(int64_t v) { seed_ = v; }
    int64_t get_seed() const { return seed_; }
    godot::Ref<Match3Board> get_board() const { return board_; }
    int get_score() const { return score_; }

protected:
    static void _bind_methods();

private:
    BoardView3D* view() const;
    void on_swap_requested(godot::Vector2i a, godot::Vector2i b);
    void on_animation_finished();

    godot::NodePath board_view_path_;
    int64_t seed_ = 0;
    int score_ = 0;
    bool redeal_ = false;
    godot::Ref<Match3Board> board_;
};

}  // namespace montezuma::godot_binding
