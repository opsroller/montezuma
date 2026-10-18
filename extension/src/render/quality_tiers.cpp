// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "quality_tiers.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/camera_attributes.hpp>
#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
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
    std::vector<Light3D*> lights;
    for (int i = 0; i < 3; ++i) {
        auto* light = memnew(OmniLight3D);
        light->set_shadow(true);
        light->add_to_group(kShadowGroup);
        scratch->add_child(light);
        lights.push_back(light);
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
        int shadowed = 0;
        for (Light3D* light : lights) shadowed += light->has_shadow() ? 1 : 0;
        check(shadowed == std::min(3, p.shadow_casting_lights), id, "shadow-casting light budget");
        check(get_active_features().size() > 0, id, "active feature list empty");
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
