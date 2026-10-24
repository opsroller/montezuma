// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>

#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace montezuma::godot_binding {

// An omni light that flickers and sways like the flame it stands in for:
// its energy follows flicker::fire() around the energy the scene gave it,
// and it drifts a few centimetres so the shadows it casts move. Lights in
// different places never flicker in step (the seed comes from the
// position unless one is set). Shadows and fog energy stay with the light
// as authored (docs/reference/fire.md).
class FlickerLight3D : public godot::OmniLight3D {
    GDCLASS(FlickerLight3D, godot::OmniLight3D)

public:
    void _ready() override;
    void _process(double delta) override;

    // Energy swing as a fraction of the authored energy, 0..1.
    void set_flicker(float value);
    float get_flicker() const { return flicker_; }
    // Typical flicker rate in Hz.
    void set_speed(float value);
    float get_speed() const { return speed_; }
    // Largest drift from the rest position, in metres.
    void set_sway(float value);
    float get_sway() const { return sway_; }
    // 0 picks a seed from the light's position.
    void set_seed(int value) { seed_ = value; }
    int get_seed() const { return seed_; }

protected:
    static void _bind_methods();

private:
    float flicker_ = 0.25f;
    float speed_ = 7.0f;
    float sway_ = 0.04f;
    int seed_ = 0;

    float base_energy_ = 1.0f;
    godot::Vector3 rest_;
    std::uint32_t noise_seed_ = 0;
    double time_ = 0.0;
};

}  // namespace montezuma::godot_binding
