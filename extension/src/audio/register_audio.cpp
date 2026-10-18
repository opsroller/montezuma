// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_audio.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "game_audio.hpp"

namespace montezuma::godot_binding {

void register_audio() {
    GDREGISTER_CLASS(GameAudio);
}

}  // namespace montezuma::godot_binding
