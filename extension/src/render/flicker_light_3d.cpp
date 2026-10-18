// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "flicker_light_3d.hpp"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "flicker.hpp"

using namespace godot;

namespace montezuma::godot_binding {

void FlickerLight3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_flicker", "value"), &FlickerLight3D::set_flicker);
    ClassDB::bind_method(D_METHOD("get_flicker"), &FlickerLight3D::get_flicker);
    ClassDB::bind_method(D_METHOD("set_speed", "value"), &FlickerLight3D::set_speed);
    ClassDB::bind_method(D_METHOD("get_speed"), &FlickerLight3D::get_speed);
    ClassDB::bind_method(D_METHOD("set_sway", "value"), &FlickerLight3D::set_sway);
    ClassDB::bind_method(D_METHOD("get_sway"), &FlickerLight3D::get_sway);
    ClassDB::bind_method(D_METHOD("set_seed", "value"), &FlickerLight3D::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &FlickerLight3D::get_seed);
    ADD_GROUP("Flicker", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flicker", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_flicker", "get_flicker");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0.5,20,0.1,suffix:Hz"), "set_speed",
                 "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sway", PROPERTY_HINT_RANGE, "0,0.2,0.005,suffix:m"), "set_sway",
                 "get_sway");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
}

void FlickerLight3D::set_flicker(float value) { flicker_ = std::clamp(value, 0.0f, 1.0f); }
void FlickerLight3D::set_speed(float value) { speed_ = std::clamp(value, 0.5f, 20.0f); }
void FlickerLight3D::set_sway(float value) { sway_ = std::clamp(value, 0.0f, 0.2f); }

void FlickerLight3D::_ready() {
    // In the editor the light stays as authored, so saving never bakes a
    // flickered energy into the scene.
    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }
    base_energy_ = static_cast<float>(get_param(PARAM_ENERGY));
    rest_ = get_position();
    const Vector3 at = get_global_position();
    noise_seed_ = seed_ != 0 ? static_cast<std::uint32_t>(seed_)
                             : flicker::seed_from_position(static_cast<float>(at.x), static_cast<float>(at.y),
                                                           static_cast<float>(at.z));
    set_process(true);
}

void FlickerLight3D::_process(double delta) {
    // Wrapped hourly so the float phase keeps its precision in long
    // sessions; the one jump an hour is a single flicker step.
    time_ = std::fmod(time_ + delta, 3600.0);
    const auto t = static_cast<float>(time_);
    const float n = flicker::fire(t, speed_, noise_seed_);
    set_param(PARAM_ENERGY, std::max(0.0f, base_energy_ * (1.0f + flicker_ * n)));
    if (sway_ > 0.0f) {
        // Slower than the flicker: the flame leans, it does not jitter.
        const float s = speed_ * 0.35f;
        const Vector3 drift(flicker::fire(t, s, noise_seed_ + 1U), 0.5f * flicker::fire(t, s, noise_seed_ + 2U),
                            flicker::fire(t, s, noise_seed_ + 3U));
        set_position(rest_ + drift * sway_);
    }
}

}  // namespace montezuma::godot_binding
