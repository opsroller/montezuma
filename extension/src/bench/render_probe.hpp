// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace montezuma::godot_binding::bench {

// What the frame is rendering with, read from the live viewport, world
// environment, camera and scene tree: the feature list the benchmark writes
// (keys in docs/reference/benchmark.md#features).
godot::Dictionary active_features(godot::Viewport* viewport);

// The renderer, GPU, OS and output size the run used.
godot::Dictionary device_info(godot::Viewport* viewport);

// Hides the UI layers the autoloads put over every scene (the menus), so
// the frame is the 3D board alone.
void hide_ui_overlays(godot::Node* root);

// Groups of effects the readability capture turns on and off
// (docs/reference/visual-target.md#readability-rules-every-tier).
enum EffectGroup : std::uint32_t {
    // Persistent effects rule 2 names: depth and volumetric fog, FogVolumes,
    // depth of field, and persistent particles (dust, smoke, flame).
    kAtmosphere = 1u << 0,
    // Bloom (rule 6). Switched off by glow intensity 0, not by disabling
    // glow: on Compatibility, enabling glow also moves the 3D render to an
    // HDR buffer, which changes specular highlights by itself.
    kGlow = 1u << 1,
    // Screen-space and real-time GI passes: SSR, SSAO, SSIL, SDFGI.
    kScreenSpace = 1u << 2,
    kAllEffects = kAtmosphere | kGlow | kScreenSpace,
};

// Remembers how the scene had its effects set, and switches groups on or off
// relative to that. A group the scene had off stays off.
class EffectSwitch {
public:
    // Records the current settings under `root` and `viewport`.
    void capture(godot::Node* root, godot::Viewport* viewport);
    // Leaves on only the groups in `on` (among those the scene had on).
    void apply(std::uint32_t on);
    // Back to how capture() found it.
    void restore() { apply(kAllEffects); }

private:
    struct Env {
        bool fog = false;
        bool volumetric_fog = false;
        double glow_intensity = 0.0;
        bool ssr = false;
        bool ssao = false;
        bool ssil = false;
        bool sdfgi = false;
    };
    struct Visible {
        godot::ObjectID node;
        bool visible = false;
    };
    godot::Ref<godot::Environment> env_;
    Env base_;
    godot::ObjectID blur_;  // the BackdropBlur drawing depth of field
    bool dof_ = false;
    std::vector<Visible> atmosphere_nodes_;
};

}  // namespace montezuma::godot_binding::bench
