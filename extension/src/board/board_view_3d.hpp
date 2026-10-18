// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <vector>

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "match3_board.hpp"
#include "animator.hpp"
#include "board_layout.hpp"
#include "input.hpp"
#include "piece_library.hpp"

namespace montezuma::godot_binding {

// 3D presentation of a Match3Board (contract: "Game signals" in
// docs/reference/contracts.md). It draws the board, asks for swaps through
// `swap_requested`, and plays back results it is given. It never applies a
// rule: whoever owns the board calls try_swap and hands the result to play().
class BoardView3D : public godot::Node3D {
    GDCLASS(BoardView3D, godot::Node3D)

public:
    ~BoardView3D() override;

    void _unhandled_input(const godot::Ref<godot::InputEvent>& event) override;
    void _notification(int what);

    // Draws `board` as it is now, without animation.
    void show_board(const godot::Ref<Match3Board>& board);
    // Plays a try_swap result, then emits `animation_finished`. Afterwards the
    // view matches the board it was shown.
    void play(const godot::Dictionary& result);
    // Bounces two pieces toward each other and back, then emits `animation_finished`.
    void play_illegal(godot::Vector2i a, godot::Vector2i b);
    bool is_busy() const { return busy_; }
    // Jumps to the end of the playing animation; the view then matches the board.
    void skip_animation();
    // Playback speed for animations (1 = feel targets); outcomes never change.
    void set_animation_speed(double v);
    double get_animation_speed() const { return animation_speed_; }

    int get_board_width() const { return grid_.width; }
    int get_board_height() const { return grid_.height; }
    void set_cell_size(double v);
    double get_cell_size() const { return grid_.cell; }

    godot::Vector3 cell_position(godot::Vector2i cell) const;
    // Cell under a screen position, using the active camera; (-1, -1) when none.
    godot::Vector2i cell_at_screen(godot::Vector2 screen_pos) const;

protected:
    static void _bind_methods();

private:
    struct Slot {
        godot::Node3D* node = nullptr;
        int kind = -1;
    };
    struct PlanHooks;

    godot::Vector3 cell_position(int x, int y) const;
    void play_events(const anim::Events& events);
    void emit_cascade_step(int step);
    void redeal_in();
    void release_dying();
    // Releases cleared pieces and makes every cell show the board's kind.
    void reconcile();
    godot::Node3D* piece_at(godot::Vector2i c) const;
    int index(godot::Vector2i c) const { return c.y * grid_.width + c.x; }
    bool in_bounds(godot::Vector2i c) const;

    void rebuild();
    void clear_pieces();
    // Makes every cell show the board's kind; changed cells drop in when `animate`.
    void sync_to_board(bool animate);
    godot::Ref<godot::Tween> begin_animation();
    void settle();
    void finish_animation();

    input::Output press(int pointer, godot::Vector2 screen_pos);
    void apply_input(const input::Output& out);
    double swipe_threshold_pixels() const;
    void select(godot::Vector2i cell);
    void clear_selection();

    layout::Grid grid_;
    PieceLibrary library_;
    godot::Ref<Match3Board> board_;
    godot::Node3D* slab_ = nullptr;
    godot::Node3D* pieces_root_ = nullptr;
    std::vector<godot::Node3D*> pieces_;
    std::vector<int> kinds_;
    godot::Ref<godot::Tween> tween_;
    std::vector<Slot> dying_;
    double animation_speed_ = 1.0;
    bool redealing_ = false;
    bool busy_ = false;
    bool has_selection_ = false;
    godot::Vector2i selected_;
    godot::Vector2i last_request_[2];
    input::SwapGesture gesture_;
};

}  // namespace montezuma::godot_binding
