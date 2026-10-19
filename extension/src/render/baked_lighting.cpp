// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "baked_lighting.hpp"

#include <godot_cpp/classes/lightmap_gi.hpp>
#include <godot_cpp/classes/lightmap_gi_data.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/voxel_gi.hpp>
#include <godot_cpp/classes/voxel_gi_data.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "quality_tiers.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr const char* kDataPath = "light_data_path";

void attach_lightmap(LightmapGI* lightmap) {
    Ref<LightmapGIData> data = lightmap->get_light_data();
    if (data.is_null()) {
        if (!lightmap->has_meta(kDataPath)) return;
        const String path = lightmap->get_meta(kDataPath);
        if (!ResourceLoader::get_singleton()->exists(path)) {
            // The game still runs, without baked light; say why it looks flat.
            UtilityFunctions::push_warning("No lightmap at ", path,
                                           ": run cmake -P tools/assets/bake_lightmaps.cmake (docs/how-to/build-and-test.md)");
            return;
        }
        data = ResourceLoader::get_singleton()->load(path);
        if (data.is_null()) return;
    }
    // Setting the data again assigns it to the meshes now at its paths.
    lightmap->set_light_data(Ref<LightmapGIData>());
    lightmap->set_light_data(data);
}

bool voxel_gi_wanted() {
    const QualityTiers* tiers = QualityTiers::get_singleton();
    if (!tiers || !tiers->has_feature("voxel_gi")) return false;
    // VoxelGI runs on Forward+ only; the tier table already says so, this
    // keeps a forced tier from baking where the renderer cannot.
    const String method = ProjectSettings::get_singleton()->get_setting_with_override("rendering/renderer/rendering_method");
    return method == "forward_plus";
}

}  // namespace

void attach_baked_lighting(Node* lighting, Node* scene) {
    if (!lighting || !scene) return;
    const TypedArray<Node> lightmaps = lighting->find_children("*", "LightmapGI", true, false);
    for (int64_t i = 0; i < lightmaps.size(); ++i) {
        if (auto* lightmap = Object::cast_to<LightmapGI>(lightmaps[i])) attach_lightmap(lightmap);
    }
    const TypedArray<Node> voxels = lighting->find_children("*", "VoxelGI", true, false);
    if (voxels.is_empty() || !voxel_gi_wanted()) return;
    for (int64_t i = 0; i < voxels.size(); ++i) {
        auto* voxel = Object::cast_to<VoxelGI>(voxels[i]);
        if (voxel && voxel->is_inside_tree() && voxel->get_probe_data().is_null()) voxel->bake(scene, false);
    }
}

}  // namespace montezuma::godot_binding
