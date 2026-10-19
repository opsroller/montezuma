// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace montezuma::godot_binding {

// Draws the meshes under one subtree (the board's pieces and their markers)
// as one multimesh per mesh, material and shadow setting, so a full board is
// a few draws instead of one per piece. The pieces stay ordinary nodes that
// the view moves, scales and hides; once a frame, after every tween, sync()
// copies where they are into the multimeshes and hides the meshes it drew.
// Turned off, it shows the meshes again and draws nothing itself.
// Reference: docs/reference/board-view.md, "Draw calls".
class PieceBatcher {
public:
    // Batches go under `parent`, which must share the subtree's parent space.
    void attach(godot::Node3D* parent);
    // Mirrors every visible MeshInstance3D under `source` when `enabled`;
    // otherwise gives back every mesh it hid.
    void sync(godot::Node* source, bool enabled);
    // Frees the batches (the parent keeps no reference to them).
    void clear();
    // Draws used now: batches with at least one instance.
    int batches_drawn() const;
    // Meshes drawn through a batch in the last sync.
    int meshes_batched() const { return batched_; }

private:
    using Key = std::tuple<int64_t, int64_t, int, uint32_t>;  // mesh, material, shadow casting, layers
    struct Batch {
        godot::MultiMeshInstance3D* node = nullptr;
        godot::Ref<godot::MultiMesh> multimesh;
        std::vector<godot::Transform3D> transforms;
    };

    void gather(godot::Node* node, bool enabled, const godot::Transform3D& to_local);
    Batch& batch_for(const Key& key, godot::MeshInstance3D* mesh, const godot::Ref<godot::Material>& material);

    godot::Node3D* parent_ = nullptr;
    std::map<Key, Batch> batches_;
    int batched_ = 0;
};

}  // namespace montezuma::godot_binding
