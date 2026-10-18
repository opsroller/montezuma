// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <vector>

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include "match3_board.hpp"
#include "animator.hpp"
#include "board_layout.hpp"
#include "effects/board_effects.hpp"
#include "effects/effect_plan.hpp"
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
    // view matches the board it was shown. Optional keys add effects (WP-29):
    // per cascade `lead`, `repaints` and `fx`, and `marks` for the markers
    // after the move (docs/reference/board-view.md, Effects).
    void play(const godot::Dictionary& result);
    // Markers on every cell, row-major (fx::marks: gem, steel, chain bits and
    // a power-up badge); drawn now, or when the playing animation ends.
    void show_marks(const godot::PackedInt32Array& marks);
    godot::PackedInt32Array get_marks() const;
    // Highlights each [a, b] swap in `pairs` until the next move, or for
    // `seconds` when above 0.
    void show_hints(const godot::Array& pairs, double seconds);
    void clear_hints();
    bool has_hints() const { return !hints_.empty(); }
    // Plays one totem or power-up effect on its own, outside a move:
    // `label` is "totem:<id>" or "powerup:<id>", `cells` what it touches.
    // For the benchmark's bench_event (WP-46) and look-dev.
    void play_effect(const godot::String& label, const godot::Array& cells);
    // Effect beats scheduled since the board was built, by name (tests).
    godot::Dictionary get_effects_played() const;
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
    // Theme in the asset manifest; empty means the manifest's default_theme.
    void set_theme(const godot::String& v);
    godot::String get_theme() const { return theme_; }
    // How far the drawn board (frame included) reaches past the outer cells,
    // in cells, so the camera can fit all of it.
    double get_board_padding() const { return board_padding_; }

    godot::Vector3 cell_position(godot::Vector2i cell) const;
    // Cell under a screen position, using the active camera; (-1, -1) when none.
    godot::Vector2i cell_at_screen(godot::Vector2 screen_pos) const;

protected:
    static void _bind_methods();

private:
    struct Slot {
        godot::Node3D* node = nullptr;
        int kind = -1;
        int marks = fx::marks::kNone;
    };
    struct PlanHooks;

    godot::Vector3 cell_position(int x, int y) const;
    void play_events(const anim::Events& events);
    void emit_cascade_step(int step);
    void spawn_match_fx(godot::Vector3 position);
    void redeal_in();
    // Returns a piece to the pool without its markers.
    void release_piece(int kind, godot::Node3D* node);
    // Replaces a piece's markers; `pop` grows the new ones in.
    void set_piece_marks(godot::Object* piece, int marks, bool pop);
    // Makes every piece's markers match `marks_target_`.
    void sync_marks();
    void clear_effects();
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
    // Stops all animation at once, before a rebuild.
    void stop_animation();
    // Ends the redeal grow at once, every piece at full size.
    void finish_grow();

    input::Output press(int pointer, godot::Vector2 screen_pos);
    void apply_input(const input::Output& out);
    double swipe_threshold_pixels() const;
    void select(godot::Vector2i cell);
    void clear_selection();

    layout::Grid grid_;
    PieceLibrary library_;
    godot::Ref<Match3Board> board_;
    godot::String theme_;
    godot::Node3D* environment_ = nullptr;
    godot::Node3D* slab_ = nullptr;
    godot::Node3D* highlight_ = nullptr;
    godot::real_t highlight_lift_ = 0.0f;
    double board_padding_ = 0.0;
    godot::Node3D* pieces_root_ = nullptr;
    godot::Node3D* fx_root_ = nullptr;
    // Worst seconds a move's effects ran past its plan, since load (0: none).
    double effects_overrun_ = 0.0;
    std::vector<godot::Node3D*> pieces_;
    std::vector<int> kinds_;
    // Markers each cell should show once the playing animation ends.
    std::vector<int> marks_;
    std::vector<int> marks_target_;
    bool has_marks_target_ = false;
    BoardEffects effects_;
    // The playing move's effects, parallel to its steps.
    std::vector<fx::StepFx> fx_steps_;
    godot::Node3D* hints_root_ = nullptr;
    std::vector<godot::Node3D*> hints_;
    godot::Ref<godot::Tween> hint_pulse_;
    godot::Ref<godot::Tween> hint_timer_;
    godot::Ref<godot::Tween> tween_;
    // The new deal growing in after a redeal; runs past tween_.
    godot::Ref<godot::Tween> grow_tween_;
    std::vector<Slot> dying_;
    double animation_speed_ = 1.0;
    bool redealing_ = false;
    bool busy_ = false;
    bool has_selection_ = false;
    godot::Vector2i selected_;
    godot::Vector2i last_request_[2] = {godot::Vector2i(-1, -1), godot::Vector2i(-1, -1)};
    input::SwapGesture gesture_;
};

}  // namespace montezuma::godot_binding
