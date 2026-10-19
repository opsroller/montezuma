// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

namespace montezuma::godot_binding {

// Since Godot 4.8, RenderingServer binds the methods below only in engine
// builds that have a RenderingDevice; the web templates do not. godot-cpp
// release builds call a missing method through a null pointer, so check
// that the method is bound before calling it.
inline bool rendering_server_has(const char* method) {
    return godot::ClassDBSingleton::get_singleton()->class_has_method("RenderingServer", method);
}

// The main RenderingDevice, or nullptr where there is none (Compatibility
// renderer, web).
inline godot::RenderingDevice* main_rendering_device() {
    static const bool bound = rendering_server_has("get_rendering_device");
    return bound ? godot::RenderingServer::get_singleton()->get_rendering_device() : nullptr;
}

}  // namespace montezuma::godot_binding
