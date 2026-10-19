// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "piece_batcher.hpp"

#include <algorithm>

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Where a batched mesh keeps its own render layers while it is drawn by a
// batch (its layers are 0 then, so neither the camera nor a shadow map
// draws it twice).
const char* const kLayersMeta = "_batched_layers";

// The one material the batch draws a mesh with: null to use the mesh's own
// surface materials, or the material every surface shows. False when the
// surfaces show different materials that are not the mesh's own, which a
// multimesh cannot express.
bool batch_material(MeshInstance3D* mi, const Ref<Mesh>& mesh, Ref<Material>& out) {
    const int surfaces = static_cast<int>(mesh->get_surface_count());
    if (surfaces == 0) return false;
    const Ref<Material> override = mi->get_material_override();
    if (override.is_valid()) {
        out = override;
        return true;
    }
    bool own = true;
    bool shared = true;
    const Ref<Material> first = mi->get_active_material(0);
    for (int s = 0; s < surfaces; ++s) {
        const Ref<Material> active = mi->get_active_material(s);
        own = own && active == mesh->surface_get_material(s);
        shared = shared && active == first;
    }
    if (own) {
        out.unref();
        return true;
    }
    if (shared && first.is_valid()) {
        out = first;
        return true;
    }
    return false;
}

void restore(MeshInstance3D* mi) {
    if (!mi->has_meta(kLayersMeta)) return;
    mi->set_layer_mask(static_cast<uint32_t>(int64_t(mi->get_meta(kLayersMeta))));
    mi->remove_meta(kLayersMeta);
}

}  // namespace

void PieceBatcher::attach(Node3D* parent) { parent_ = parent; }

void PieceBatcher::clear() {
    for (auto& [key, batch] : batches_) {
        if (batch.node) batch.node->queue_free();
    }
    batches_.clear();
    batched_ = 0;
}

int PieceBatcher::batches_drawn() const {
    int n = 0;
    for (const auto& [key, batch] : batches_) n += batch.transforms.empty() ? 0 : 1;
    return n;
}

PieceBatcher::Batch& PieceBatcher::batch_for(const Key& key, MeshInstance3D* mi, const Ref<Material>& material) {
    Batch& batch = batches_[key];
    if (!batch.node) {
        batch.multimesh.instantiate();
        batch.multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
        batch.multimesh->set_mesh(mi->get_mesh());
        batch.node = memnew(MultiMeshInstance3D);
        batch.node->set_name("Batch");
        batch.node->set_multimesh(batch.multimesh);
        batch.node->set_material_override(material);
        batch.node->set_cast_shadows_setting(mi->get_cast_shadows_setting());
        batch.node->set_layer_mask(std::get<3>(key));
        parent_->add_child(batch.node);
    }
    return batch;
}

void PieceBatcher::gather(Node* node, bool enabled, const Transform3D& to_local) {
    if (auto* mi = Object::cast_to<MeshInstance3D>(node)) {
        const Ref<Mesh> mesh = mi->get_mesh();
        Ref<Material> material;
        const bool drawn = mi->is_visible_in_tree() && mesh.is_valid();
        // Overlays, transparency fades and skins are per instance: those
        // meshes keep drawing themselves.
        const bool batchable = enabled && drawn && mi->get_material_overlay().is_null() &&
                               mi->get_transparency() == 0.0f && mi->get_skeleton_path().is_empty() &&
                               batch_material(mi, mesh, material);
        if (!batchable) {
            restore(mi);
        } else {
            if (!mi->has_meta(kLayersMeta)) {
                mi->set_meta(kLayersMeta, int64_t(mi->get_layer_mask()));
                mi->set_layer_mask(0);
            }
            const auto layers = static_cast<uint32_t>(int64_t(mi->get_meta(kLayersMeta)));
            const Key key{int64_t(mesh->get_rid().get_id()),
                          material.is_valid() ? int64_t(material->get_rid().get_id()) : 0,
                          static_cast<int>(mi->get_cast_shadows_setting()), layers};
            batch_for(key, mi, material).transforms.push_back(to_local * mi->get_global_transform());
            ++batched_;
        }
    }
    const TypedArray<Node> children = node->get_children(true);
    for (int64_t i = 0; i < children.size(); ++i)
        gather(Object::cast_to<Node>(children[i].operator Object*()), enabled, to_local);
}

void PieceBatcher::sync(Node* source, bool enabled) {
    if (!parent_ || !source || !parent_->is_inside_tree()) return;
    for (auto& [key, batch] : batches_) batch.transforms.clear();
    batched_ = 0;
    gather(source, enabled, parent_->get_global_transform().affine_inverse());
    for (auto& [key, batch] : batches_) {
        const auto n = static_cast<int32_t>(batch.transforms.size());
        batch.node->set_visible(n > 0);
        if (n == 0) continue;
        // Grows only; extra instances are hidden, so a busy frame does not
        // reallocate every frame after it.
        if (batch.multimesh->get_instance_count() < n)
            batch.multimesh->set_instance_count(std::max(n, batch.multimesh->get_instance_count() * 2));
        batch.multimesh->set_visible_instance_count(n);
        for (int32_t i = 0; i < n; ++i) batch.multimesh->set_instance_transform(i, batch.transforms[i]);
    }
}

}  // namespace montezuma::godot_binding
