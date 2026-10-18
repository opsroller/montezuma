// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "quality_tiers.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/camera_attributes.hpp>
#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/tube_trail_mesh.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace q = quality;

namespace {

const char* const kSection = "quality";
const char* const kKey = "tier";
const char* const kAuto = "auto";
const char* const kGroupPrefix = "quality:";
const char* const kFallbackPrefix = "quality_fallback:";
const char* const kShadowGroup = "quality_shadow_light";
const char* const kFogLightGroup = "quality_fog_light";
// Where the authored (theme) value of a switched property is kept, so a
// lower tier can turn an effect off and a higher one can restore it.
const char* const kAuthoredMeta = "_quality_authored";
// Material slots QualityTiers filled with a variant of a mesh's own material:
// surface index -> material, and kOverrideKey for material_override.
const char* const kVariantMeta = "_quality_variant";
const char* const kOverrideKey = "override";

String to_godot(std::string_view s) { return String::utf8(s.data(), static_cast<int64_t>(s.size())); }
std::string to_std(const String& s) { return std::string(s.utf8().get_data()); }

q::AdapterType adapter_type(int64_t device_type) {
    switch (device_type) {
        case RenderingDevice::DEVICE_TYPE_INTEGRATED_GPU: return q::AdapterType::Integrated;
        case RenderingDevice::DEVICE_TYPE_DISCRETE_GPU: return q::AdapterType::Discrete;
        case RenderingDevice::DEVICE_TYPE_VIRTUAL_GPU: return q::AdapterType::Virtual;
        case RenderingDevice::DEVICE_TYPE_CPU: return q::AdapterType::Cpu;
        default: return q::AdapterType::Other;
    }
}

RenderingServer::ShadowQuality shadow_quality(q::SoftShadows s) {
    switch (s) {
        case q::SoftShadows::Hard: return RenderingServer::SHADOW_QUALITY_HARD;
        case q::SoftShadows::VeryLow: return RenderingServer::SHADOW_QUALITY_SOFT_VERY_LOW;
        case q::SoftShadows::Low: return RenderingServer::SHADOW_QUALITY_SOFT_LOW;
        case q::SoftShadows::Medium: return RenderingServer::SHADOW_QUALITY_SOFT_MEDIUM;
        case q::SoftShadows::High: return RenderingServer::SHADOW_QUALITY_SOFT_HIGH;
    }
    return RenderingServer::SHADOW_QUALITY_SOFT_LOW;
}

RenderingServer::SubSurfaceScatteringQuality sss_quality(q::SubsurfaceQuality s) {
    switch (s) {
        case q::SubsurfaceQuality::Disabled: return RenderingServer::SUB_SURFACE_SCATTERING_QUALITY_DISABLED;
        case q::SubsurfaceQuality::Low: return RenderingServer::SUB_SURFACE_SCATTERING_QUALITY_LOW;
        case q::SubsurfaceQuality::Medium: return RenderingServer::SUB_SURFACE_SCATTERING_QUALITY_MEDIUM;
        case q::SubsurfaceQuality::High: return RenderingServer::SUB_SURFACE_SCATTERING_QUALITY_HIGH;
    }
    return RenderingServer::SUB_SURFACE_SCATTERING_QUALITY_DISABLED;
}

Viewport::MSAA msaa_mode(int samples) {
    if (samples >= 8) return Viewport::MSAA_8X;
    if (samples >= 4) return Viewport::MSAA_4X;
    if (samples >= 2) return Viewport::MSAA_2X;
    return Viewport::MSAA_DISABLED;
}

Viewport::AnisotropicFiltering anisotropy_level(int level) {
    if (level >= 16) return Viewport::ANISOTROPY_16X;
    if (level >= 8) return Viewport::ANISOTROPY_8X;
    if (level >= 4) return Viewport::ANISOTROPY_4X;
    if (level >= 2) return Viewport::ANISOTROPY_2X;
    return Viewport::ANISOTROPY_DISABLED;
}

// The authored value of a switched bool, recorded the first time a tier
// touches it; `current` is the value the theme set.
bool authored(Object* owner, const char* key, bool current) {
    Dictionary memo = owner->get_meta(kAuthoredMeta, Dictionary());
    if (!memo.has(key)) {
        memo[key] = current;
        owner->set_meta(kAuthoredMeta, memo);
    }
    return memo[key];
}

double authored_number(Object* owner, const char* key, double current) {
    Dictionary memo = owner->get_meta(kAuthoredMeta, Dictionary());
    if (!memo.has(key)) {
        memo[key] = current;
        owner->set_meta(kAuthoredMeta, memo);
    }
    return memo[key];
}

}  // namespace

QualityTiers* QualityTiers::singleton_ = nullptr;

QualityTiers::QualityTiers() = default;
QualityTiers::~QualityTiers() = default;

void QualityTiers::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_choice"), &QualityTiers::get_choice);
    ClassDB::bind_method(D_METHOD("set_choice", "choice"), &QualityTiers::set_choice);
    ClassDB::bind_method(D_METHOD("get_tier"), &QualityTiers::get_tier);
    ClassDB::bind_method(D_METHOD("get_detected_tier"), &QualityTiers::get_detected_tier);
    ClassDB::bind_method(D_METHOD("get_available_tiers"), &QualityTiers::get_available_tiers);
    ClassDB::bind_method(D_METHOD("has_feature", "feature"), &QualityTiers::has_feature);
    ClassDB::bind_method(D_METHOD("get_active_features"), &QualityTiers::get_active_features);
    ClassDB::bind_method(D_METHOD("resolve_variant", "path"), &QualityTiers::resolve_variant);
    ClassDB::bind_method(D_METHOD("apply_to", "root"), &QualityTiers::apply_to);
    ClassDB::bind_method(D_METHOD("preload_variants", "scene"), &QualityTiers::preload_variants);
    ClassDB::bind_method(D_METHOD("apply_shadow_budget"), &QualityTiers::apply_shadow_budget);
    ClassDB::bind_method(D_METHOD("run_self_test"), &QualityTiers::run_self_test);
    ADD_SIGNAL(MethodInfo("tier_changed", PropertyInfo(Variant::STRING, "tier")));
}

void QualityTiers::_enter_tree() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (!singleton_) singleton_ = this;
}

void QualityTiers::_exit_tree() {
    if (singleton_ == this) singleton_ = nullptr;
    SceneTree* tree = get_tree();
    if (tree && tree->is_connected("node_added", callable_mp(this, &QualityTiers::on_node_added)))
        tree->disconnect("node_added", callable_mp(this, &QualityTiers::on_node_added));
    Window* root = tree ? tree->get_root() : nullptr;
    if (root && root->is_connected("size_changed", callable_mp(this, &QualityTiers::on_viewport_resized)))
        root->disconnect("size_changed", callable_mp(this, &QualityTiers::on_viewport_resized));
}

void QualityTiers::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    RenderingServer* rs = RenderingServer::get_singleton();
    adapter_.rendering_method = to_std(rs->get_current_rendering_method());
    adapter_.type = adapter_type(rs->get_video_adapter_type());
    adapter_.name = to_std(rs->get_video_adapter_name());
    adapter_.vendor = to_std(rs->get_video_adapter_vendor());

    load_choice();
    // `-- --quality=<tier>` forces a tier for this run without saving it
    // (the benchmark uses it to capture every tier).
    for (const String& arg : OS::get_singleton()->get_cmdline_user_args()) {
        if (arg.begins_with("--quality=")) {
            choice_ = arg.substr(10);
        }
    }
    tier_ = q::resolve_tier(to_std(choice_), adapter_);

    SceneTree* tree = get_tree();
    tree->connect("node_added", callable_mp(this, &QualityTiers::on_node_added));
    tree->get_root()->connect("size_changed", callable_mp(this, &QualityTiers::on_viewport_resized));
    apply_all();
    UtilityFunctions::print("QualityTiers: ", get_tier(), " (", to_godot(adapter_.rendering_method), ", ",
                            to_godot(adapter_.name), ")");
    if (OS::get_singleton()->get_cmdline_user_args().has("--quality-selftest")) call_deferred("run_self_test");
}

void QualityTiers::load_choice() {
    Ref<ConfigFile> cfg;
    cfg.instantiate();
    if (cfg->load(settings_path_) != OK) return;  // first run: auto
    choice_ = cfg->get_value(kSection, kKey, kAuto);
}

void QualityTiers::save_choice() const {
    Ref<ConfigFile> cfg;
    cfg.instantiate();
    cfg->set_value(kSection, kKey, choice_);
    if (cfg->save(settings_path_) != OK)
        UtilityFunctions::push_warning("QualityTiers: could not save the quality tier to ", settings_path_);
}

void QualityTiers::set_choice(const String& choice) { switch_to(choice, true); }

void QualityTiers::switch_to(const String& choice, bool save) {
    const std::string wanted = to_std(choice);
    if (wanted != kAuto) {
        const std::optional<q::Tier> tier = q::tier_from_id(wanted);
        if (!tier || !q::tier_runs_on(*tier, adapter_.rendering_method)) {
            UtilityFunctions::push_warning("QualityTiers: tier '", choice, "' does not run on ",
                                           to_godot(adapter_.rendering_method), "; keeping ", choice_);
            return;
        }
    }
    choice_ = choice;
    if (save) save_choice();
    const q::Tier next = q::resolve_tier(wanted, adapter_);
    if (next == tier_) return;
    tier_ = next;
    variant_cache_.clear();  // resolved for the old tier's variant folder
    apply_all();
    emit_signal("tier_changed", get_tier());
}

String QualityTiers::get_tier() const { return to_godot(q::tier_id(tier_)); }

String QualityTiers::get_detected_tier() const { return to_godot(q::tier_id(q::detect_tier(adapter_))); }

PackedStringArray QualityTiers::get_available_tiers() const {
    PackedStringArray out;
    for (q::Tier t : q::kTiers)
        if (q::tier_runs_on(t, adapter_.rendering_method)) out.push_back(to_godot(q::tier_id(t)));
    return out;
}

bool QualityTiers::has_feature(const String& feature) const {
    const std::optional<q::Feature> f = q::feature_from_id(to_std(feature));
    return f && get_profile().has(*f);
}

PackedStringArray QualityTiers::get_active_features() const {
    PackedStringArray out;
    for (std::size_t i = 0; i < q::kFeatureCount; ++i) {
        const auto f = static_cast<q::Feature>(i);
        if (get_profile().has(f)) out.push_back(to_godot(q::feature_id(f)));
    }
    return out;
}

String QualityTiers::resolve_variant(const String& path) const {
    const std::string_view variant = get_profile().texture_variant;
    if (variant.empty()) return path;
    const String candidate = to_godot(q::variant_path(to_std(path), variant));
    return ResourceLoader::get_singleton()->exists(candidate) ? candidate : path;
}

void QualityTiers::apply_all() {
    apply_engine_settings();
    apply_viewport();
    if (SceneTree* tree = get_tree()) apply_to(tree->get_root());
    apply_shadow_budget();
}

void QualityTiers::apply_engine_settings() {
    const q::Profile& p = get_profile();
    RenderingServer* rs = RenderingServer::get_singleton();
    rs->directional_shadow_atlas_set_size(p.sun_shadow_size, true);
    rs->directional_soft_shadow_filter_set_quality(shadow_quality(p.soft_shadows));
    rs->positional_soft_shadow_filter_set_quality(shadow_quality(p.soft_shadows));
    rs->sub_surface_scattering_set_quality(sss_quality(p.subsurface));
    rs->environment_set_ssr_half_size(p.ssr_half_size);
    rs->environment_set_volumetric_fog_volume_size(p.volumetric_fog_size, p.volumetric_fog_depth);
}

void QualityTiers::apply_viewport() {
    SceneTree* tree = get_tree();
    if (!tree) return;
    Window* root = tree->get_root();
    const q::Profile& p = get_profile();
    const q::ViewportPlan plan = q::viewport_plan(p, root->get_size().y);
    Viewport::Scaling3DMode mode = Viewport::SCALING_3D_MODE_BILINEAR;
    if (plan.scaling == q::Scaling::SpatialUpscale) mode = Viewport::SCALING_3D_MODE_FSR;
    if (plan.scaling == q::Scaling::TemporalUpscale) {
        const bool metal = RenderingServer::get_singleton()->get_current_rendering_driver_name() == "metal";
        mode = metal ? Viewport::SCALING_3D_MODE_METALFX_TEMPORAL : Viewport::SCALING_3D_MODE_FSR2;
    }
    root->set_scaling_3d_mode(mode);
    root->set_scaling_3d_scale(plan.render_scale);
    root->set_use_taa(plan.taa);
    root->set_msaa_3d(msaa_mode(p.msaa));
    root->set_screen_space_aa(p.smaa ? Viewport::SCREEN_SPACE_AA_SMAA : Viewport::SCREEN_SPACE_AA_DISABLED);
    root->set_anisotropic_filtering_level(anisotropy_level(p.anisotropy));
    root->set_mesh_lod_threshold(p.mesh_lod_threshold);
    root->set_positional_shadow_atlas_size(p.positional_shadow_atlas);
}

void QualityTiers::on_viewport_resized() { apply_viewport(); }

void QualityTiers::apply_to(Node* root) {
    if (!root) return;
    apply_node(root);
    const TypedArray<Node> children = root->get_children(true);
    for (int64_t i = 0; i < children.size(); ++i) apply_to(Object::cast_to<Node>(children[i].operator Object*()));
}

void QualityTiers::on_node_added(Node* node) {
    apply_node(node);
    if (node->is_in_group(kShadowGroup)) request_shadow_budget();
}

void QualityTiers::apply_node(Node* node) {
    const q::Profile& p = get_profile();
    if (auto* geometry = Object::cast_to<GeometryInstance3D>(node)) apply_materials(geometry);
    if (auto* world = Object::cast_to<WorldEnvironment>(node)) {
        apply_environment(world->get_environment());
        apply_camera_attributes(world->get_camera_attributes());
    } else if (auto* camera = Object::cast_to<Camera3D>(node)) {
        apply_environment(camera->get_environment());
        apply_camera_attributes(camera->get_attributes());
    }
    if (auto* light = Object::cast_to<Light3D>(node); light && light->is_in_group(kFogLightGroup)) {
        const double energy = authored_number(light, "volumetric_fog_energy",
                                              light->get_param(Light3D::PARAM_VOLUMETRIC_FOG_ENERGY));
        light->set_param(Light3D::PARAM_VOLUMETRIC_FOG_ENERGY,
                         p.has(q::Feature::FogFireLight) ? static_cast<float>(energy) : 0.0f);
    }

    // Trails are authored off, with the trail mesh in metadata/trail_mesh,
    // and switched on here: the Compatibility renderer warns as soon as a
    // particle system has trails or a trail mesh, even a hidden one.
    if (auto* particles = Object::cast_to<GPUParticles3D>(node);
        particles && particles->is_in_group("quality:particle_trails")) {
        const bool trails = p.has(q::Feature::ParticleTrails);
        if (trails && particles->has_meta("trail_mesh")) {
            const Ref<Mesh> mesh = particles->get_meta("trail_mesh");
            if (mesh.is_valid()) particles->set_draw_pass_mesh(0, mesh);
        }
        particles->set_trail_enabled(trails);
    }

    const TypedArray<StringName> groups = node->get_groups();
    for (int64_t i = 0; i < groups.size(); ++i) {
        const String group = groups[i];
        bool fallback = false;
        String id;
        if (group.begins_with(kGroupPrefix)) {
            id = group.substr(String(kGroupPrefix).length());
        } else if (group.begins_with(kFallbackPrefix)) {
            id = group.substr(String(kFallbackPrefix).length());
            fallback = true;
        } else {
            continue;
        }
        const std::optional<q::Feature> f = q::feature_from_id(to_std(id));
        if (!f) {
            UtilityFunctions::push_warning("QualityTiers: node '", node->get_name(), "' is in group '", group,
                                           "', which names no quality feature.");
            continue;
        }
        node->set("visible", p.has(*f) != fallback);
    }
}

Ref<Material> QualityTiers::variant_of(const Ref<Material>& material) {
    if (material.is_null()) return material;
    const String path = material->get_path();
    // Built in code, or embedded in a scene ("res://x.tscn::id"): no file to vary.
    if (path.is_empty() || path.contains("::")) return material;
    if (const Ref<Material>* hit = variant_cache_.getptr(path)) return *hit;
    const String wanted = resolve_variant(to_godot(q::base_path(to_std(path))));
    Ref<Material> out = material;
    if (wanted != path) {
        const Ref<Material> loaded = ResourceLoader::get_singleton()->load(wanted);
        if (loaded.is_valid()) {
            out = loaded;
        } else {
            UtilityFunctions::push_warning("QualityTiers: material variant ", wanted, " does not load; keeping ", path);
        }
    }
    variant_cache_.insert(path, out);
    return out;
}

void QualityTiers::apply_materials(GeometryInstance3D* geometry) {
    // Slots this class filled from a mesh's own material, so they are told
    // apart from content's overrides and follow a new mesh on the next pass.
    Dictionary owned = geometry->get_meta(kVariantMeta, Dictionary());
    bool changed = false;
    auto release = [&](const Variant& key) {
        if (owned.has(key)) {
            owned.erase(key);
            changed = true;
        }
    };
    auto claim = [&](const Variant& key, const Ref<Material>& material) {
        if (!owned.has(key) || Ref<Material>(owned[key]) != material) {
            owned[key] = material;
            changed = true;
        }
    };
    auto is_owned = [&](const Variant& key, const Ref<Material>& material) {
        return material.is_valid() && owned.has(key) && Ref<Material>(owned[key]) == material;
    };

    // Particles and multimeshes draw shared meshes and have no per-surface
    // slots; when every surface they draw has one material, its variant goes
    // in material_override.
    const Ref<Material> override = geometry->get_material_override();
    if (override.is_valid() && !is_owned(kOverrideKey, override)) {
        release(kOverrideKey);
        if (const Ref<Material> v = variant_of(override); v != override) geometry->set_material_override(v);
    } else {
        const Ref<Material> source = shared_mesh_material(geometry);
        const Ref<Material> wanted = variant_of(source);
        if (source.is_valid() && wanted != source) {
            if (override != wanted) geometry->set_material_override(wanted);
            claim(kOverrideKey, wanted);
        } else {
            if (is_owned(kOverrideKey, override)) geometry->set_material_override(Ref<Material>());
            release(kOverrideKey);
        }
    }
    if (const Ref<Material> m = geometry->get_material_overlay(); m.is_valid()) {
        if (const Ref<Material> v = variant_of(m); v != m) geometry->set_material_overlay(v);
    }
    // CSG shapes keep their material on the node.
    if (geometry->is_class("CSGPrimitive3D")) {
        const Ref<Material> m = geometry->get("material");
        if (const Ref<Material> v = variant_of(m); v != m) geometry->set("material", v);
    }

    if (auto* mesh_instance = Object::cast_to<MeshInstance3D>(geometry)) {
        const Ref<Mesh> mesh = mesh_instance->get_mesh();
        const int32_t surfaces = mesh_instance->get_surface_override_material_count();
        for (int32_t i = 0; i < surfaces; ++i) {
            const Ref<Material> current = mesh_instance->get_surface_override_material(i);
            if (current.is_valid() && !is_owned(i, current)) {
                // Content's own override: show its variant.
                release(i);
                if (const Ref<Material> v = variant_of(current); v != current) {
                    mesh_instance->set_surface_override_material(i, v);
                }
                continue;
            }
            // No override, or one this class set: the mesh's own material
            // decides, so a new mesh is followed.
            const Ref<Material> source = mesh.is_valid() ? mesh->surface_get_material(i) : Ref<Material>();
            const Ref<Material> wanted = variant_of(source);
            if (source.is_valid() && wanted != source) {
                if (current != wanted) mesh_instance->set_surface_override_material(i, wanted);
                claim(i, wanted);
            } else {
                if (current.is_valid()) mesh_instance->set_surface_override_material(i, Ref<Material>());
                release(i);
            }
        }
        // Surfaces the old mesh had and the new one lacks.
        const Array keys = owned.keys();
        for (int64_t k = 0; k < keys.size(); ++k) {
            if (keys[k].get_type() == Variant::INT && int64_t(keys[k]) >= surfaces) release(keys[k]);
        }
    }
    if (!changed) return;
    if (owned.is_empty()) {
        geometry->remove_meta(kVariantMeta);
    } else {
        geometry->set_meta(kVariantMeta, owned);
    }
}

Ref<Material> QualityTiers::shared_mesh_material(GeometryInstance3D* geometry) {
    std::vector<Ref<Mesh>> meshes;
    if (auto* cpu = Object::cast_to<CPUParticles3D>(geometry)) {
        meshes.push_back(cpu->get_mesh());
    } else if (auto* gpu = Object::cast_to<GPUParticles3D>(geometry)) {
        for (int32_t i = 0; i < gpu->get_draw_passes(); ++i) meshes.push_back(gpu->get_draw_pass_mesh(i));
    } else if (auto* multi = Object::cast_to<MultiMeshInstance3D>(geometry)) {
        if (const Ref<MultiMesh> mm = multi->get_multimesh(); mm.is_valid()) meshes.push_back(mm->get_mesh());
    }
    Ref<Material> shared;
    for (const Ref<Mesh>& mesh : meshes) {
        if (mesh.is_null()) continue;
        for (int32_t i = 0; i < mesh->get_surface_count(); ++i) {
            const Ref<Material> m = mesh->surface_get_material(i);
            if (m.is_null() || (shared.is_valid() && m != shared)) return {};
            shared = m;
        }
    }
    return shared;
}

void QualityTiers::preload_variants(const Ref<PackedScene>& scene) {
    if (scene.is_null()) return;
    Node* copy = scene->instantiate();
    if (!copy) return;
    // Fills the variant cache out of the tree, so the first copy that enters
    // it later costs a lookup, not a load.
    apply_materials_to(copy);
    memdelete(copy);
}

void QualityTiers::apply_materials_to(Node* root) {
    if (auto* geometry = Object::cast_to<GeometryInstance3D>(root)) apply_materials(geometry);
    const TypedArray<Node> children = root->get_children(true);
    for (int64_t i = 0; i < children.size(); ++i) {
        apply_materials_to(Object::cast_to<Node>(children[i].operator Object*()));
    }
}

void QualityTiers::apply_environment(const Ref<Environment>& env) {
    if (env.is_null()) return;
    const q::Profile& p = get_profile();
    Environment* e = env.ptr();
    e->set_ssao_enabled(p.has(q::Feature::Ssao) && authored(e, "ssao", e->is_ssao_enabled()));
    e->set_ssil_enabled(p.has(q::Feature::Ssil) && authored(e, "ssil", e->is_ssil_enabled()));
    e->set_ssr_enabled(p.has(q::Feature::Ssr) && authored(e, "ssr", e->is_ssr_enabled()));
    e->set_sdfgi_enabled(p.has(q::Feature::Sdfgi) && authored(e, "sdfgi", e->is_sdfgi_enabled()));
    e->set_volumetric_fog_enabled(p.has(q::Feature::VolumetricFog) &&
                                  authored(e, "volumetric_fog", e->is_volumetric_fog_enabled()));
    e->set_glow_enabled(p.has(q::Feature::Glow) && authored(e, "glow", e->is_glow_enabled()));
}

void QualityTiers::apply_camera_attributes(const Ref<CameraAttributes>& attributes) {
    if (attributes.is_null()) return;
    const q::Profile& p = get_profile();
    CameraAttributes* a = attributes.ptr();
    a->set_auto_exposure_enabled(p.has(q::Feature::AutoExposure) &&
                                 authored(a, "auto_exposure", a->is_auto_exposure_enabled()));
    if (auto* practical = Object::cast_to<CameraAttributesPractical>(a)) {
        const bool dof = p.has(q::Feature::DepthOfField);
        practical->set_dof_blur_far_enabled(dof && authored(a, "dof_far", practical->is_dof_blur_far_enabled()));
        practical->set_dof_blur_near_enabled(dof &&
                                             authored(a, "dof_near", practical->is_dof_blur_near_enabled()));
    }
}

void QualityTiers::request_shadow_budget() {
    if (shadow_budget_pending_) return;
    shadow_budget_pending_ = true;
    call_deferred("apply_shadow_budget");
}

void QualityTiers::apply_shadow_budget() {
    shadow_budget_pending_ = false;
    SceneTree* tree = get_tree();
    if (!tree) return;
    const TypedArray<Node> nodes = tree->get_nodes_in_group(kShadowGroup);
    Camera3D* camera = tree->get_root()->get_camera_3d();
    const Vector3 eye = camera ? camera->get_global_position() : Vector3();
    std::vector<std::pair<real_t, Light3D*>> lights;
    for (int64_t i = 0; i < nodes.size(); ++i) {
        auto* light = Object::cast_to<Light3D>(nodes[i].operator Object*());
        if (!light || !light->is_inside_tree()) continue;
        lights.emplace_back(light->get_global_position().distance_squared_to(eye), light);
    }
    std::stable_sort(lights.begin(), lights.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    const auto budget = static_cast<std::size_t>(std::max(0, get_profile().shadow_casting_lights));
    for (std::size_t i = 0; i < lights.size(); ++i) {
        Light3D* light = lights[i].second;
        const bool wanted = authored(light, "shadow", light->has_shadow());
        light->set_shadow(wanted && i < budget);
    }
}

// Started with `-- --quality-selftest`: switches through every tier the
// running renderer offers and checks that each turns exactly its features
// on, on a scratch environment, camera attributes and tagged nodes.
void QualityTiers::run_self_test() {
    int checks = 0;
    int failures = 0;
    auto check = [&](bool ok, const String& tier, const char* what) {
        ++checks;
        if (!ok) {
            ++failures;
            UtilityFunctions::push_error("QualityTiers self-test: ", tier, ": ", what);
        }
    };

    Ref<Environment> env;
    env.instantiate();
    env->set_ssao_enabled(true);
    env->set_ssil_enabled(true);
    env->set_ssr_enabled(true);
    env->set_sdfgi_enabled(false);  // a theme that picked VoxelGI leaves SDFGI off on every tier
    env->set_volumetric_fog_enabled(true);
    env->set_glow_enabled(true);
    Ref<CameraAttributesPractical> attrs;
    attrs.instantiate();
    attrs->set_auto_exposure_enabled(true);
    attrs->set_dof_blur_far_enabled(true);

    Node3D* scratch = memnew(Node3D);
    scratch->set_name("QualitySelfTest");
    auto* world = memnew(WorldEnvironment);
    world->set_environment(env);
    world->set_camera_attributes(attrs);
    scratch->add_child(world);
    auto* voxel = memnew(Node3D);
    voxel->add_to_group("quality:voxel_gi");
    scratch->add_child(voxel);
    auto* cards = memnew(Node3D);
    cards->add_to_group("quality_fallback:volumetric_fog");
    scratch->add_child(cards);
    auto* trails = memnew(GPUParticles3D);
    trails->set_emitting(false);
    trails->add_to_group("quality:particle_trails");
    const Ref<TubeTrailMesh> trail_mesh = memnew(TubeTrailMesh);
    trails->set_meta("trail_mesh", trail_mesh);
    scratch->add_child(trails);
    std::vector<Light3D*> lights;
    for (int i = 0; i < 3; ++i) {
        auto* light = memnew(OmniLight3D);
        light->set_shadow(true);
        light->add_to_group(kShadowGroup);
        scratch->add_child(light);
        lights.push_back(light);
    }
    // Material variants: gold has a hi/ variant, lapis has none; a material
    // made in code has no file and is never swapped.
    const String gold_path = "res://assets/materials/temple/MI_Gold.tres";
    const String lapis_path = "res://assets/materials/temple/MI_Lapis.tres";
    ResourceLoader* loader = ResourceLoader::get_singleton();
    const Ref<Material> gold = loader->exists(gold_path) ? Ref<Material>(loader->load(gold_path)) : Ref<Material>();
    const Ref<Material> lapis = loader->exists(lapis_path) ? Ref<Material>(loader->load(lapis_path)) : Ref<Material>();
    check(gold.is_valid() && lapis.is_valid(), get_tier(), "temple materials do not load");
    Ref<StandardMaterial3D> made;
    made.instantiate();
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_material(gold);  // the mesh's own material, no override
    auto* surfaces = memnew(MeshInstance3D);
    surfaces->set_mesh(box);
    surfaces->set_material_overlay(made);
    scratch->add_child(surfaces);
    Ref<BoxMesh> plain;
    plain.instantiate();
    auto* overrides = memnew(MeshInstance3D);
    overrides->set_mesh(plain);
    overrides->set_surface_override_material(0, lapis);
    overrides->set_material_override(gold);
    scratch->add_child(overrides);
    auto material_path = [](const Ref<Material>& m) { return m.is_valid() ? m->get_path() : String(); };

    // Cost: once a scene's variants are preloaded (PieceLibrary does it when
    // a theme loads), its first copy entering the tree only looks them up.
    // Budget 0.05 ms per acquire; 1 ms here so slow CI runners do not flake.
    {
        const String piece_path = "res://assets/models/temple/artifacts/PF_Artifact_SunDisc.tscn";
        const Ref<PackedScene> piece =
            loader->exists(piece_path) ? Ref<PackedScene>(loader->load(piece_path)) : Ref<PackedScene>();
        check(piece.is_valid(), get_tier(), "sun disc piece does not load");
        if (piece.is_valid()) {
            preload_variants(piece);
            Node* first = piece->instantiate();
            Time* time = Time::get_singleton();
            const uint64_t t0 = time->get_ticks_usec();
            get_tree()->get_root()->add_child(first);
            const uint64_t first_us = time->get_ticks_usec() - t0;
            check(first_us < 1000, get_tier(), "first copy of a preloaded piece loads its variant mid-frame");
            get_tree()->get_root()->remove_child(first);
            memdelete(first);
        }
    }

    get_tree()->get_root()->add_child(scratch);

    // switch_to(..., false): a test never writes the player's settings file.
    const String original_choice = choice_;
    for (const String& id : get_available_tiers()) {
        switch_to(id, false);
        apply_shadow_budget();
        const q::Profile& p = get_profile();
        check(get_tier() == id, id, "tier did not take effect");
        check(env->is_ssao_enabled() == p.has(q::Feature::Ssao), id, "SSAO");
        check(env->is_ssil_enabled() == p.has(q::Feature::Ssil), id, "SSIL");
        check(env->is_ssr_enabled() == p.has(q::Feature::Ssr), id, "SSR");
        check(!env->is_sdfgi_enabled(), id, "SDFGI turned on though the theme left it off");
        check(env->is_volumetric_fog_enabled() == p.has(q::Feature::VolumetricFog), id, "volumetric fog");
        check(env->is_glow_enabled() == p.has(q::Feature::Glow), id, "glow");
        check(attrs->is_auto_exposure_enabled() == p.has(q::Feature::AutoExposure), id, "auto exposure");
        check(attrs->is_dof_blur_far_enabled() == p.has(q::Feature::DepthOfField), id, "depth of field");
        check(voxel->is_visible() == p.has(q::Feature::VoxelGi), id, "quality:voxel_gi node");
        check(cards->is_visible() == !p.has(q::Feature::VolumetricFog), id, "fog fallback node");
        check(trails->is_trail_enabled() == p.has(q::Feature::ParticleTrails), id, "particle trails");
        if (p.has(q::Feature::ParticleTrails))
            check(trails->get_draw_pass_mesh(0) == trail_mesh, id, "particle trail mesh not attached");
        int shadowed = 0;
        for (Light3D* light : lights) shadowed += light->has_shadow() ? 1 : 0;
        check(shadowed == std::min(3, p.shadow_casting_lights), id, "shadow-casting light budget");
        check(get_active_features().size() > 0, id, "active feature list empty");
        if (gold.is_valid() && lapis.is_valid()) {
            const String gold_variant = resolve_variant(gold_path);
            const String gold_file = to_godot(q::variant_path(to_std(gold_path), p.texture_variant));
            check(gold_variant == (loader->exists(gold_file) ? gold_file : gold_path), id,
                  "resolve_variant names neither the variant nor the base");
            check(p.texture_variant != "hi" || gold_variant != gold_path, id, "desktop tier shows the base gold");
            check(material_path(surfaces->get_active_material(0)) == gold_variant, id, "mesh material variant");
            check(material_path(overrides->get_material_override()) == gold_variant, id, "material_override variant");
            check(material_path(overrides->get_surface_override_material(0)) == lapis_path, id,
                  "material without a variant changed");
            check(surfaces->get_material_overlay() == made, id, "material made in code changed");
            // A pooled node leaves the tree and comes back.
            scratch->remove_child(overrides);
            scratch->add_child(overrides);
            check(material_path(overrides->get_material_override()) == gold_variant, id, "variant lost on re-entry");

            // A node that showed its mesh's own material follows a new mesh:
            // in the tree after apply_to, and on re-entry after a change
            // while pooled.
            Ref<BoxMesh> gold_box;
            gold_box.instantiate();
            gold_box->set_material(gold);
            Ref<BoxMesh> lapis_box;
            lapis_box.instantiate();
            lapis_box->set_material(lapis);
            auto* reshaped = memnew(MeshInstance3D);
            reshaped->set_mesh(gold_box);
            scratch->add_child(reshaped);
            check(material_path(reshaped->get_active_material(0)) == gold_variant, id, "reshaped: gold variant");
            reshaped->set_mesh(lapis_box);
            apply_to(reshaped);
            check(material_path(reshaped->get_active_material(0)) == lapis_path, id,
                  "mesh changed in the tree keeps the old mesh's material variant");
            scratch->remove_child(reshaped);
            reshaped->set_mesh(gold_box);
            scratch->add_child(reshaped);
            check(material_path(reshaped->get_active_material(0)) == gold_variant, id, "re-entry with gold mesh");
            scratch->remove_child(reshaped);
            reshaped->set_mesh(lapis_box);
            scratch->add_child(reshaped);
            check(material_path(reshaped->get_active_material(0)) == lapis_path, id,
                  "mesh changed out of the tree re-enters with the old mesh's material variant");
            scratch->remove_child(reshaped);
            memdelete(reshaped);

            // Particles, multimeshes and CSG show the variant too.
            auto* cpu = memnew(CPUParticles3D);
            cpu->set_mesh(gold_box);
            auto* gpu = memnew(GPUParticles3D);
            gpu->set_draw_pass_mesh(0, gold_box);
            Ref<MultiMesh> multimesh;
            multimesh.instantiate();
            multimesh->set_mesh(gold_box);
            auto* multi = memnew(MultiMeshInstance3D);
            multi->set_multimesh(multimesh);
            auto* csg = memnew(CSGBox3D);
            csg->set_material(gold);
            GeometryInstance3D* drawn[] = {cpu, gpu, multi};
            for (GeometryInstance3D* g : drawn) scratch->add_child(g);
            scratch->add_child(csg);
            for (GeometryInstance3D* g : drawn) {
                const Ref<Material> o = g->get_material_override();
                check(material_path(o.is_valid() ? o : gold) == gold_variant, id,
                      "particle or multimesh material variant");
            }
            check(material_path(csg->get_material()) == gold_variant, id, "CSG material variant");
            // The particle mesh changes to one without a variant: the override goes.
            cpu->set_mesh(lapis_box);
            apply_to(cpu);
            check(cpu->get_material_override().is_null(), id, "particle override kept after its mesh changed");
            for (GeometryInstance3D* g : drawn) {
                scratch->remove_child(g);
                memdelete(g);
            }
            scratch->remove_child(csg);
            memdelete(csg);
        }
    }
    // Back to the first tier: everything the theme authored comes back.
    if (get_available_tiers().size() > 0) {
        switch_to(get_available_tiers()[0], false);
        check(env->is_ssao_enabled() == get_profile().has(q::Feature::Ssao), get_tier(), "authored SSAO not restored");
    }

    scratch->queue_free();
    switch_to(original_choice, false);
    UtilityFunctions::print("QualityTiers self-test: ", checks, " checks, ", failures, " failures");
}

}  // namespace montezuma::godot_binding
