// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "match3_board.hpp"
#include "match3_level.hpp"

namespace montezuma::godot_binding {

class BoardView3D;

// The web demo's scene root: plays one level file (the intro level, e1-l01)
// on a BoardView3D and, once, when the level is won, emits
// level_completed(level_id, score). On the web build it also dispatches the
// page events from the web demo hand-off contract: montezuma:ready on the
// first frame and montezuma:level-completed on the win. Off the web it
// dispatches nothing.
//
// The win comes from Match3Level, so it needs a level with quotas. Gem goals
// (the intro level's goal) are not tracked by the core yet; until they are,
// such a level plays without a win. The user argument --complete-level
// (after "--" on the command line, or in the page's Engine args) wins the
// level on the first frame, for the browser smoke test.
class IntroLevel : public godot::Node3D {
    GDCLASS(IntroLevel, godot::Node3D)

public:
    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void set_board_view_path(const godot::NodePath& p) { board_view_path_ = p; }
    godot::NodePath get_board_view_path() const { return board_view_path_; }
    void set_level_path(const godot::String& p) { level_path_ = p; }
    godot::String get_level_path() const { return level_path_; }

    godot::String get_level_id() const { return level_id_; }
    int get_score() const { return score_; }
    bool is_completed() const { return completed_; }
    // Wins the level: emits level_completed and tells the page, once.
    void complete();

protected:
    static void _bind_methods();

private:
    BoardView3D* view() const;
    bool load_level();
    void on_swap_requested(godot::Vector2i a, godot::Vector2i b);
    void on_animation_finished();
    void on_level_finished(int outcome, int score);

    godot::NodePath board_view_path_;
    godot::String level_path_ = "res://data/levels/e1/l01.json";
    godot::String level_id_;
    int width_ = 0;
    int height_ = 0;
    int kinds_ = 0;
    int64_t seed_ = 0;
    int score_ = 0;
    bool redeal_ = false;
    bool first_frame_ = true;
    bool complete_on_start_ = false;
    bool completed_ = false;
    godot::Ref<Match3Board> board_;
    godot::Ref<Match3Level> level_;
};

}  // namespace montezuma::godot_binding
