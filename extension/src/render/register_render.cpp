// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_render.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "backdrop_blur.hpp"
#include "flicker_light_3d.hpp"
#include "quality_tiers.hpp"

namespace montezuma::godot_binding {

void register_render() {
    GDREGISTER_CLASS(QualityTiers);
    GDREGISTER_CLASS(FlickerLight3D);
    GDREGISTER_CLASS(BackdropBlur);
}

}  // namespace montezuma::godot_binding
