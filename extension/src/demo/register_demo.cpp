// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_demo.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "intro_level.hpp"

namespace montezuma::godot_binding {

void register_demo() { GDREGISTER_CLASS(IntroLevel); }

}  // namespace montezuma::godot_binding
