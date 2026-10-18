// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_board.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "board_camera_3d.hpp"
#include "board_playtest.hpp"
#include "board_view_3d.hpp"

namespace montezuma::godot_binding {

void register_board() {
    GDREGISTER_CLASS(BoardView3D);
    GDREGISTER_CLASS(BoardCamera3D);
    GDREGISTER_CLASS(BoardPlaytest);
}

}  // namespace montezuma::godot_binding
