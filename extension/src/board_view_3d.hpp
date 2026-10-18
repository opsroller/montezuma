// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <vector>

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "match3_board.hpp"

namespace montezuma::godot_binding {

// 3D presentation of a Match3Board: one mesh per cell on a stone slab,
// click or tap to select and swap. Placeholder primitives stand in for the
// final carved-artifact models.
class BoardView3D : public godot::Node3D {
    GDCLASS(BoardView3D, godot::Node3D)

public:
    void _ready() override;
    void _unhandled_input(const godot::Ref<godot::InputEvent>& event) override;

    void set_board_width(int v) { board_width_ = v; }
    int get_board_width() const { return board_width_; }
    void set_board_height(int v) { board_height_ = v; }
    int get_board_height() const { return board_height_; }
    void set_seed(int64_t v) { seed_ = v; }
    int64_t get_seed() const { return seed_; }
    void set_cell_size(double v) { cell_size_ = v; }
    double get_cell_size() const { return cell_size_; }

    godot::Ref<Match3Board> get_board() const { return board_; }
    int get_score() const { return score_; }
    // Swaps two cells as if the player did it; returns false when illegal.
    bool request_swap(godot::Vector2i a, godot::Vector2i b);

protected:
    static void _bind_methods();

private:
    static constexpr int kKinds = 7;
    static constexpr double kSwapSeconds = 0.12;
    static constexpr double kSettleSeconds = 0.18;

    godot::Vector3 cell_position(int x, int y) const;
    bool pick_cell(godot::Vector2 screen_pos, godot::Vector2i& out) const;
    void handle_press(godot::Vector2 screen_pos);
    void build_meshes();
    void build_slab();
    void select(godot::Vector2i cell);
    void clear_selection();
    void sync_pieces();
    void finish_animation();
    godot::MeshInstance3D* piece_at(godot::Vector2i c) const;

    int board_width_ = 8;
    int board_height_ = 8;
    int64_t seed_ = 0;
    double cell_size_ = 1.0;
    int score_ = 0;
    bool animating_ = false;
    bool has_selection_ = false;
    godot::Vector2i selected_;
    godot::Dictionary pending_result_;

    godot::Ref<Match3Board> board_;
    std::vector<godot::MeshInstance3D*> pieces_;
    std::vector<int> shown_kinds_;
    godot::Ref<godot::Mesh> meshes_[kKinds];
    godot::Ref<godot::Material> materials_[kKinds];
};

}  // namespace montezuma::godot_binding
