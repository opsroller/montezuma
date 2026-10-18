// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <functional>
#include <map>

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "effect_plan.hpp"

namespace montezuma::godot_binding {

class PieceLibrary;

// Visuals for piece markers and effect beats (WP-29). Each resolves from
// the theme in the asset manifest (bonus_<id>, marker_*, fx_*) and falls
// back to a placeholder built here, so every effect shows on every theme.
// Placeholders are cheap meshes, Label3D text and small CPUParticles3D
// bursts that run on every renderer; the realistic GPU-particle versions
// come from the technical-artist as fx_* scenes (WP-53).
class BoardEffects {
public:
    // Kinds of beat counted by beats_played(), for tests and the self-test.
    static constexpr int kCountSlots = fx::kTotemCount + fx::kPowerUpCount + 4;

    // Loads the placeholders for `cell` and the theme's overrides.
    void load(double cell, const PieceLibrary& library);

    // The markers for `marks` (fx::marks), as one node to parent under a
    // piece; nullptr for no marks.
    godot::Node3D* make_marks(int marks) const;
    // A ring under one cell of a hinted swap, pulsing by its own animation.
    godot::Node3D* make_hint_ring() const;

    // Schedules one beat on `tween` (a parallel tween), with its nodes under
    // `root`, freed when the beat ends. `at` maps a cell to its centre.
    void play(const fx::Beat& beat, const godot::Ref<godot::Tween>& tween, godot::Node3D* root,
              const std::function<godot::Vector3(fx::Cell)>& at, godot::Vector3 board_centre,
              double board_span) const;

    // Beats scheduled since load, by slot: totems first, then power-ups
    // (power_up_slot), then gem collect, steel break, chain break, mark placed.
    int beats_played(int slot) const { return slot >= 0 && slot < kCountSlots ? counts_[slot] : 0; }
    static constexpr int power_up_slot(int id) { return fx::kTotemCount + id; }

    // When the last node or fade play() scheduled since reset_end() ends, in
    // the tween's seconds. The view checks it against the plan's length.
    double scheduled_end() const { return end_; }
    void reset_end() const { end_ = 0.0; }

private:
    godot::Node3D* override_for(const godot::String& name) const;
    godot::Ref<godot::Material> glow(godot::Color c, float energy = 2.0f, bool transparent = true) const;
    godot::Ref<godot::Material> solid(godot::Color c, float metallic, float roughness) const;
    godot::Node3D* mesh_node(const godot::Ref<godot::Mesh>& mesh, const godot::Ref<godot::Material>& mat) const;
    godot::Node3D* label(const godot::String& text, godot::Color c) const;
    godot::Node3D* burst(godot::Color c, int amount, double lifetime, float speed, const godot::Ref<godot::Mesh>& mesh) const;
    godot::Node3D* power_up_badge(int id) const;

    // Shows `node` at `start`, frees it at `start + seconds`.
    void schedule(godot::Node3D* node, godot::Node3D* root, const godot::Ref<godot::Tween>& tween, double start,
                  double seconds) const;
    void fade(godot::Node3D* node, const godot::Ref<godot::Tween>& tween, double start, double seconds) const;

    const PieceLibrary* library_ = nullptr;
    float cell_ = 1.0f;
    float particle_scale_ = 1.0f;
    godot::Ref<godot::Mesh> sphere_;
    godot::Ref<godot::Mesh> octahedron_;
    godot::Ref<godot::Mesh> ring_;
    godot::Ref<godot::Mesh> disc_;
    godot::Ref<godot::Mesh> box_;
    godot::Ref<godot::Mesh> barrel_;
    godot::Ref<godot::Mesh> cone_;
    godot::Ref<godot::Mesh> shard_;
    mutable std::array<int, kCountSlots> counts_{};
    mutable double end_ = 0.0;
    // Materials by colour and settings, shared by every node that uses them.
    mutable std::map<std::array<float, 7>, godot::Ref<godot::Material>> materials_;
};

}  // namespace montezuma::godot_binding
