// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_render.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "flicker_light_3d.hpp"
#include "scene_preloader.hpp"
#ifdef MONTEZUMA_RAY_TRACING
#include "rt/rt_reflections.hpp"
#endif

namespace montezuma::godot_binding {

void register_render() {
    GDREGISTER_CLASS(FlickerLight3D);
    GDREGISTER_CLASS(ScenePreloader);
#ifdef MONTEZUMA_RAY_TRACING
    GDREGISTER_CLASS(RtReflections);
#endif
}

}  // namespace montezuma::godot_binding
