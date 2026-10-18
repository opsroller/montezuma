// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "../match3_board.hpp"
#include "montezuma/level_session.hpp"

namespace montezuma::godot_binding {

class BoardView3D;

// The game scene's controller (contract: "Game signals" in
// docs/reference/contracts.md). It loads a level file, plays it with a core
// LevelSession, answers the board view's swap requests, mirrors the
// session's board into a Match3Board for the view, counts time down, emits
// the HUD signals and plays the audio cues. It holds no rules: every
// decision is the session's. Core exceptions stop at this class.
class Match3Game : public godot::Node3D {
    GDCLASS(Match3Game, godot::Node3D)

public:
    enum Outcome { PLAYING = 0, WON = 1, LOST = 2 };  // LevelState.Outcome

    Match3Game();
    ~Match3Game() override;

    void _ready() override;
    void _exit_tree() override;
    void _process(double delta) override;

    // Loads levels_dir/e<N>/l<NN>.json for "e<N>-l<NN>" and starts it at
    // "normal" or "hard". False, with an engine error and nothing started,
    // for a bad id or difficulty, a missing or invalid file, or a level the
    // session cannot deal.
    bool start_level(const godot::String& level_id, const godot::String& difficulty);
    // Ends the level without a result (left from the pause menu).
    void stop_level();
    // HUD commands (contract): a manual shuffle and a bought hint.
    void shuffle();
    void buy_hint();
    // The board view's swap request; true when the swap was legal.
    bool request_swap(godot::Vector2i a, godot::Vector2i b);

    bool is_playing() const;
    godot::String get_level_id() const;
    int get_outcome() const;
    int get_score() const;
    double get_time_left() const;
    int get_gems_collected() const;
    int get_gem_goal() const;
    int get_moves() const;
    godot::Ref<Match3Board> get_board() const { return board_; }
    // Every legal swap as [a, b] (Vector2i), for bots and tests.
    godot::Array get_legal_moves() const;

    void set_board_view_path(const godot::NodePath& p) { board_view_path_ = p; }
    godot::NodePath get_board_view_path() const { return board_view_path_; }
    void set_levels_dir(const godot::String& v) { levels_dir_ = v; }
    godot::String get_levels_dir() const { return levels_dir_; }
    void set_autostart_level(const godot::String& v) { autostart_level_ = v; }
    godot::String get_autostart_level() const { return autostart_level_; }
    void set_autostart_difficulty(const godot::String& v) { autostart_difficulty_ = v; }
    godot::String get_autostart_difficulty() const { return autostart_difficulty_; }
    void set_warning_seconds(double v) { warning_seconds_ = v; }
    double get_warning_seconds() const { return warning_seconds_; }

protected:
    static void _bind_methods();

private:
    // What the HUD was last told, so each signal fires only on a change.
    struct Shown {
        int score = -1;
        int gems = -1;
        int gem_goal = -1;
        std::map<int, int> quotas;
        std::array<double, kTotemCount> totem_charge{};
        std::array<int, kTotemCount> totem_uses{};
        double frenzy_bar = -1.0;
        double frenzy_left = -1.0;
        int shuffles = -1;
        int hint_cost = -1;
        bool hint_affordable = false;
    };

    BoardView3D* view() const;
    void connect_ui();
    void disconnect_ui();
    void on_play_requested(const godot::String& level_id, const godot::String& difficulty);
    void on_level_quit_requested();
    void on_swap_requested(godot::Vector2i a, godot::Vector2i b);
    void on_cascade_step(int step);
    void on_animation_finished();

    // Emits every HUD signal whose value changed; all of them when `all`.
    void publish(bool all);
    void play_board(const godot::Dictionary& result);
    void after_command();
    void finish_if_over();
    void emit_finished();
    void cue(const godot::String& name);
    template <typename... Args>
    void send(const char* signal, const Args&... args) {
        if (self_test_) ++signal_counts_[signal];
        emit_signal(signal, args...);
    }
    void run_self_test();
    godot::Dictionary level_dictionary() const;

    std::unique_ptr<LevelSession> session_;
    godot::Ref<Match3Board> board_;
    godot::String level_id_;
    godot::String difficulty_;
    Shown shown_;
    // Totem charges to show: a move's report shows a totem that fired at 1.
    std::array<double, kTotemCount> totem_charge_{};
    // Cue names per display step of the playing animation (cascade_step).
    std::vector<std::vector<godot::String>> step_cues_;
    bool finish_pending_ = false;
    bool finished_ = false;
    bool warned_ = false;

    godot::NodePath board_view_path_;
    godot::String levels_dir_ = "res://data/levels";
    godot::String autostart_level_;
    godot::String autostart_difficulty_ = "normal";
    double warning_seconds_ = 10.0;

    // Self-test (`-- --game-selftest`): cues are recorded, not played.
    bool self_test_ = false;
    std::vector<godot::String> cues_played_;
    std::map<std::string, int> signal_counts_;
    int step_mismatches_ = 0;
};

}  // namespace montezuma::godot_binding

VARIANT_ENUM_CAST(montezuma::godot_binding::Match3Game::Outcome);
