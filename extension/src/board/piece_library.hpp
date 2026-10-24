// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <vector>

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace montezuma::godot_binding {

// Visuals for the board, resolved from one theme of the asset manifest
// (game/assets/manifest.json, contract in docs/reference/contracts.md, names
// in docs/reference/board-themes.md). A name missing from the theme, or a
// path that fails to load, falls back to a placeholder, so the board always
// draws.
//
// Piece visuals are pooled per kind: the board view acquires one when a
// piece appears and releases it when the piece leaves, so play does not
// allocate nodes once the pool is warm.
class PieceLibrary {
public:
    static constexpr int kMaxKinds = 7;
    static constexpr const char* kManifestPath = "res://assets/manifest.json";

    // Loads `theme` from the manifest (empty means its `default_theme`) and
    // builds placeholders sized for `cell`.
    void load(double cell, const godot::String& theme);

    // A visual node for this kind, not in the tree. Kinds outside 0..6 get an
    // empty node.
    godot::Node3D* acquire(int kind);
    // Takes back a node from acquire(); it must already be out of the tree.
    void release(int kind, godot::Node3D* node);
    // Frees every pooled node. Call from the owner's exit/destructor path.
    void clear();

    // The board under the pieces: the theme's `board` if present, otherwise a
    // stone-coloured slab sized to the board.
    godot::Node3D* make_board(int width, int height, double cell) const;
    // Sky, lights and backdrop: the theme's `environment` if present,
    // otherwise a plain sky with a key and a fill light.
    godot::Node3D* make_environment() const;
    // The marker at the selected cell (`cell_highlight`); nullptr when the
    // theme has none, and the lifted piece alone shows the selection.
    godot::Node3D* make_highlight() const;
    // A one-shot burst for a cleared cell (`fx_match`) that frees itself when
    // done; nullptr when the theme has none.
    godot::Node3D* make_match_fx() const;

    // The theme's scene for `name` (effects and markers look theirs up here);
    // null when the theme has none.
    godot::Ref<godot::PackedScene> entry(const godot::String& name) const { return load_entry(name); }

    bool uses_manifest(int kind) const { return kind >= 0 && kind < kMaxKinds && scenes_[kind].is_valid(); }

private:
    godot::Node3D* make_visual(int kind) const;
    godot::Ref<godot::Resource> load_entry(const godot::String& name) const;
    // `<name>.<feature>` variants for the features this build has replace the plain entries.
    static godot::Dictionary resolve_features(const godot::Dictionary& entries);
    godot::Node3D* instantiate_entry(const godot::String& name) const;

    // The chosen theme's name -> path entries.
    godot::Dictionary entries_;
    godot::Ref<godot::PackedScene> match_fx_;
    float piece_size_ = 0.7f;
    std::array<godot::Ref<godot::PackedScene>, kMaxKinds> scenes_;
    std::array<godot::Ref<godot::Mesh>, kMaxKinds> meshes_;
    std::array<godot::Ref<godot::Material>, kMaxKinds> materials_;
    std::array<std::vector<godot::Node3D*>, kMaxKinds> pool_;
};

}  // namespace montezuma::godot_binding
