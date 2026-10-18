// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace montezuma::godot_binding {

class Match3Game;

// The web demo's scene root: a thin wrapper over the game scene. Its
// Match3Game child plays the intro level (level_id, e1-l01) with
// the HUD, the core level session and every rule; IntroLevel holds none of
// its own. Once, when that level is won, it emits level_completed(level_id,
// score). On the web build it also dispatches the page events from the web
// demo hand-off contract: montezuma:ready on the first frame and
// montezuma:level-completed on the win. Off the web it dispatches nothing.
//
// The demo plays the intro level only: a request for any other level (the
// results, map or pause screens) is sent back to it. The user argument
// --complete-level wins on the first frame, for the browser smoke test;
// --intro-selftest plays the level to a win and checks the above.
class IntroLevel : public godot::Node3D {
    GDCLASS(IntroLevel, godot::Node3D)

public:
    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void set_game_path(const godot::NodePath& p) { game_path_ = p; }
    godot::NodePath get_game_path() const { return game_path_; }

    // The intro level's id, e1-l01 by default.
    void set_level_id(const godot::String& v) { level_id_ = v; }
    godot::String get_level_id() const { return level_id_; }
    // The game's score, or the winning score once completed.
    int get_score() const;
    bool is_completed() const { return completed_; }
    // Wins the level: emits level_completed and tells the page, once.
    void complete();

protected:
    static void _bind_methods();

private:
    Match3Game* game() const;
    void connect_game(Match3Game* g);
    void on_level_started(const godot::Variant& level, const godot::String& difficulty);
    void on_level_finished(int outcome, int score);
    void back_to_intro();
    void finish(int score);
    void run_self_test();
    void finish_self_test();

    godot::NodePath game_path_;
    godot::String level_id_ = "e1-l01";
    int score_ = 0;
    bool first_frame_ = true;
    bool complete_on_start_ = false;
    bool completed_ = false;

    // Self-test (`-- --intro-selftest`).
    godot::String started_difficulty_;
    int completions_ = 0;
    int self_test_moves_ = 0;
    int self_test_outcome_ = 0;
    int self_test_failures_ = 0;
};

}  // namespace montezuma::godot_binding
