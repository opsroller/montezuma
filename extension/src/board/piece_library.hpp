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

// Visuals for the board, resolved from the asset manifest
// (game/assets/manifest.json, contract in docs/reference/contracts.md).
// A name missing from the manifest, or a path that fails to load, falls back
// to a placeholder primitive, so the board always draws.
//
// Piece visuals are pooled per kind: the board view acquires one when a
// piece appears and releases it when the piece leaves, so play does not
// allocate nodes once the pool is warm.
class PieceLibrary {
public:
    static constexpr int kMaxKinds = 7;
    static constexpr const char* kManifestPath = "res://assets/manifest.json";

    // Loads the manifest (if any) and builds placeholders sized for `cell`.
    void load(double cell);

    // A visual node for this kind, not in the tree. Kinds outside 0..6 get an
    // empty node.
    godot::Node3D* acquire(int kind);
    // Takes back a node from acquire(); it must already be out of the tree.
    void release(int kind, godot::Node3D* node);
    // Frees every pooled node. Call from the owner's exit/destructor path.
    void clear();

    // The board slab under the pieces: the manifest's `temple_board` if
    // present, otherwise a stone-coloured box sized to the board.
    godot::Node3D* make_board(int width, int height, double cell) const;

    bool uses_manifest(int kind) const { return kind >= 0 && kind < kMaxKinds && scenes_[kind].is_valid(); }

private:
    godot::Node3D* make_visual(int kind) const;
    godot::Ref<godot::Resource> load_entry(const godot::String& name) const;

    godot::Dictionary manifest_;
    float piece_size_ = 0.7f;
    std::array<godot::Ref<godot::PackedScene>, kMaxKinds> scenes_;
    std::array<godot::Ref<godot::Mesh>, kMaxKinds> meshes_;
    std::array<godot::Ref<godot::Material>, kMaxKinds> materials_;
    std::array<std::vector<godot::Node3D*>, kMaxKinds> pool_;
};

}  // namespace montezuma::godot_binding
