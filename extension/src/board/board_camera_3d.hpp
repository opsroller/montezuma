// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include "feel.hpp"

namespace montezuma::godot_binding {

class BoardView3D;

// Fixed, near top-down camera that keeps the whole board in frame at any
// window size or aspect ratio: it fits by width on tall phones and by height
// on wide screens, and never crops the board. It refits when the viewport
// resizes or the board changes size, and does nothing else (no motion during
// play, so input picking always matches the frame the player saw).
class BoardCamera3D : public godot::Camera3D {
    GDCLASS(BoardCamera3D, godot::Camera3D)

public:
    void _ready() override;
    void _exit_tree() override;

    void set_board_view_path(const godot::NodePath& p);
    godot::NodePath get_board_view_path() const { return board_view_path_; }
    void set_tilt_degrees(double v);
    double get_tilt_degrees() const { return tilt_degrees_; }
    void set_margin(double v);
    double get_margin() const { return margin_; }

    // Re-frames the board now.
    void refit();

protected:
    static void _bind_methods();

private:
    BoardView3D* board_view() const;

    godot::NodePath board_view_path_;
    double tilt_degrees_ = feel::kCameraTiltDegrees;
    double margin_ = feel::kCameraMargin;
};

}  // namespace montezuma::godot_binding
