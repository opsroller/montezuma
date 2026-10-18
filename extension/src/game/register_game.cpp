// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_game.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "match3_game.hpp"

namespace montezuma::godot_binding {

void register_game() { GDREGISTER_CLASS(Match3Game); }

}  // namespace montezuma::godot_binding
