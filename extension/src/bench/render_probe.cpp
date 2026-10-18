// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "render_probe.hpp"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/camera_attributes.hpp>
#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/decal.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/fog_volume.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/lightmap_gi.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/reflection_probe.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/voxel_gi.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

namespace montezuma::godot_binding::bench {

namespace {

struct Counts {
    int lightmap_gi = 0;
    int voxel_gi = 0;
    int reflection_probes = 0;
    int fog_volumes = 0;
    int gpu_particles = 0;
    int gpu_particle_amount = 0;
    int cpu_particles = 0;
    int decals = 0;
    int omni_spot_lights = 0;
    int shadowed_omni_spot_lights = 0;
    bool sun = false;
    bool sun_shadows = false;
};

bool shown(const Node3D* n) { return n->is_visible_in_tree(); }

void count(const Node* node, Counts& c) {
    if (const auto* n = Object::cast_to<Node3D>(node); n && shown(n)) {
        if (Object::cast_to<LightmapGI>(n)) ++c.lightmap_gi;
        if (Object::cast_to<VoxelGI>(n)) ++c.voxel_gi;
        if (Object::cast_to<ReflectionProbe>(n)) ++c.reflection_probes;
        if (Object::cast_to<FogVolume>(n)) ++c.fog_volumes;
        if (Object::cast_to<Decal>(n)) ++c.decals;
        if (const auto* p = Object::cast_to<GPUParticles3D>(n); p && p->is_emitting()) {
            ++c.gpu_particles;
            c.gpu_particle_amount += p->get_amount();
        }
        if (const auto* p = Object::cast_to<CPUParticles3D>(n); p && p->is_emitting()) ++c.cpu_particles;
        if (const auto* l = Object::cast_to<DirectionalLight3D>(n)) {
            c.sun = true;
            c.sun_shadows = c.sun_shadows || l->has_shadow();
        }
        if (Object::cast_to<OmniLight3D>(n) || Object::cast_to<SpotLight3D>(n)) {
            ++c.omni_spot_lights;
            if (Object::cast_to<Light3D>(n)->has_shadow()) ++c.shadowed_omni_spot_lights;
        }
    }
    for (int i = 0; i < node->get_child_count(); ++i) count(node->get_child(i), c);
}

Ref<Environment> environment_of(Viewport* viewport) {
    const Ref<World3D> world = viewport->find_world_3d();
    if (world.is_null()) return {};
    Ref<Environment> env = world->get_environment();
    if (env.is_null()) env = world->get_fallback_environment();
    return env;
}

Ref<CameraAttributes> camera_attributes_of(Viewport* viewport) {
    Ref<CameraAttributes> attrs;
    if (const Camera3D* cam = viewport->get_camera_3d()) attrs = cam->get_attributes();
    if (attrs.is_null()) {
        const Ref<World3D> world = viewport->find_world_3d();
        if (world.is_valid()) attrs = world->get_camera_attributes();
    }
    return attrs;
}

String tonemapper_name(Environment::ToneMapper t) {
    switch (t) {
        case Environment::TONE_MAPPER_LINEAR: return "linear";
        case Environment::TONE_MAPPER_REINHARDT: return "reinhardt";
        case Environment::TONE_MAPPER_FILMIC: return "filmic";
        case Environment::TONE_MAPPER_ACES: return "aces";
        case Environment::TONE_MAPPER_AGX: return "agx";
        default: return "other";
    }
}

String scaling_name(Viewport::Scaling3DMode m) {
    switch (m) {
        case Viewport::SCALING_3D_MODE_BILINEAR: return "bilinear";
        case Viewport::SCALING_3D_MODE_FSR: return "fsr1";
        case Viewport::SCALING_3D_MODE_FSR2: return "fsr2";
        case Viewport::SCALING_3D_MODE_METALFX_SPATIAL: return "metalfx_spatial";
        case Viewport::SCALING_3D_MODE_METALFX_TEMPORAL: return "metalfx_temporal";
        case Viewport::SCALING_3D_MODE_NEAREST: return "nearest";
        default: return "other";
    }
}

int msaa_samples(Viewport::MSAA m) {
    switch (m) {
        case Viewport::MSAA_2X: return 2;
        case Viewport::MSAA_4X: return 4;
        case Viewport::MSAA_8X: return 8;
        default: return 0;
    }
}

String screen_space_aa_name(Viewport::ScreenSpaceAA aa) {
    switch (aa) {
        case Viewport::SCREEN_SPACE_AA_FXAA: return "fxaa";
        case Viewport::SCREEN_SPACE_AA_SMAA: return "smaa";
        default: return "none";
    }
}

}  // namespace

Dictionary active_features(Viewport* viewport) {
    Dictionary f;
    ERR_FAIL_NULL_V(viewport, f);
    Counts c;
    count(viewport, c);

    const Ref<Environment> env = environment_of(viewport);
    const bool e = env.is_valid();
    f["environment"] = e;
    f["tonemapper"] = e ? tonemapper_name(env->get_tonemapper()) : String("none");
    f["glow"] = e && env->is_glow_enabled();
    f["fog"] = e && env->is_fog_enabled();
    f["volumetric_fog"] = e && env->is_volumetric_fog_enabled();
    f["ssr"] = e && env->is_ssr_enabled();
    f["ssao"] = e && env->is_ssao_enabled();
    f["ssil"] = e && env->is_ssil_enabled();
    f["sdfgi"] = e && env->is_sdfgi_enabled();
    f["color_grade_lut"] = e && env->is_adjustment_enabled() && env->get_adjustment_color_correction().is_valid();

    const Ref<CameraAttributes> attrs = camera_attributes_of(viewport);
    f["auto_exposure"] = attrs.is_valid() && attrs->is_auto_exposure_enabled();
    const Ref<CameraAttributesPractical> practical = attrs;
    f["depth_of_field"] =
        practical.is_valid() && (practical->is_dof_blur_far_enabled() || practical->is_dof_blur_near_enabled());

    f["lightmap_gi"] = c.lightmap_gi > 0;
    f["voxel_gi"] = c.voxel_gi > 0;
    f["reflection_probes"] = c.reflection_probes;
    f["fog_volumes"] = c.fog_volumes;
    f["gpu_particles"] = c.gpu_particles;
    f["gpu_particle_amount"] = c.gpu_particle_amount;
    f["cpu_particles"] = c.cpu_particles;
    f["decals"] = c.decals;
    f["sun_shadows"] = c.sun_shadows;
    f["sun_shadow_size"] = c.sun_shadows ? static_cast<int>(ProjectSettings::get_singleton()->get_setting_with_override(
                                               "rendering/lights_and_shadows/directional_shadow/size"))
                                         : 0;
    f["omni_spot_lights"] = c.omni_spot_lights;
    f["shadowed_omni_spot_lights"] = c.shadowed_omni_spot_lights;

    f["taa"] = viewport->is_using_taa();
    f["msaa_3d"] = msaa_samples(viewport->get_msaa_3d());
    f["screen_space_aa"] = screen_space_aa_name(viewport->get_screen_space_aa());
    f["scaling_3d_mode"] = scaling_name(viewport->get_scaling_3d_mode());
    f["scaling_3d_scale"] = viewport->get_scaling_3d_scale();
    return f;
}

Dictionary device_info(Viewport* viewport) {
    Dictionary d;
    RenderingServer* rs = RenderingServer::get_singleton();
    d["os"] = OS::get_singleton()->get_name();
    d["model"] = OS::get_singleton()->get_model_name();
    d["godot"] = Engine::get_singleton()->get_version_info()["string"];
    d["rendering_method"] = rs->get_current_rendering_method();
    d["rendering_driver"] = rs->get_current_rendering_driver_name();
    d["adapter"] = rs->get_video_adapter_name();
    d["vendor"] = rs->get_video_adapter_vendor();
    d["api_version"] = rs->get_video_adapter_api_version();
    if (viewport) {
        const Vector2 size = viewport->get_visible_rect().size;
        Array res;
        res.push_back(static_cast<int>(size.x));
        res.push_back(static_cast<int>(size.y));
        d["resolution"] = res;
    }
    return d;
}

void hide_ui_overlays(Node* root) {
    ERR_FAIL_NULL(root);
    for (int i = 0; i < root->get_child_count(); ++i) {
        if (auto* layer = Object::cast_to<CanvasLayer>(root->get_child(i))) layer->set_visible(false);
    }
}

void EffectSwitch::capture(Node* root, Viewport* viewport) {
    ERR_FAIL_NULL(root);
    ERR_FAIL_NULL(viewport);
    env_ = environment_of(viewport);
    if (env_.is_valid()) {
        base_ = {env_->is_fog_enabled(), env_->is_volumetric_fog_enabled(), env_->is_glow_enabled(),
                 env_->is_ssr_enabled(), env_->is_ssao_enabled(), env_->is_ssil_enabled(), env_->is_sdfgi_enabled()};
    }
    const Ref<CameraAttributesPractical> practical = camera_attributes_of(viewport);
    if (practical.is_valid()) {
        camera_attributes_ = ObjectID(practical->get_instance_id());
        dof_far_ = practical->is_dof_blur_far_enabled();
        dof_near_ = practical->is_dof_blur_near_enabled();
    }
    atmosphere_nodes_.clear();
    std::vector<Node*> stack{root};
    while (!stack.empty()) {
        Node* n = stack.back();
        stack.pop_back();
        if (auto* n3 = Object::cast_to<Node3D>(n);
            n3 && (Object::cast_to<FogVolume>(n3) || Object::cast_to<GPUParticles3D>(n3) ||
                   Object::cast_to<CPUParticles3D>(n3))) {
            atmosphere_nodes_.push_back({ObjectID(n3->get_instance_id()), n3->is_visible()});
        }
        for (int i = 0; i < n->get_child_count(); ++i) stack.push_back(n->get_child(i));
    }
}

void EffectSwitch::apply(std::uint32_t on) {
    const bool atmosphere = (on & kAtmosphere) != 0;
    const bool glow = (on & kGlow) != 0;
    const bool screen = (on & kScreenSpace) != 0;
    if (env_.is_valid()) {
        env_->set_fog_enabled(base_.fog && atmosphere);
        env_->set_volumetric_fog_enabled(base_.volumetric_fog && atmosphere);
        env_->set_glow_enabled(base_.glow && glow);
        env_->set_ssr_enabled(base_.ssr && screen);
        env_->set_ssao_enabled(base_.ssao && screen);
        env_->set_ssil_enabled(base_.ssil && screen);
        env_->set_sdfgi_enabled(base_.sdfgi && screen);
    }
    if (auto* practical = Object::cast_to<CameraAttributesPractical>(ObjectDB::get_instance(camera_attributes_))) {
        practical->set_dof_blur_far_enabled(dof_far_ && atmosphere);
        practical->set_dof_blur_near_enabled(dof_near_ && atmosphere);
    }
    for (const Visible& v : atmosphere_nodes_) {
        if (auto* n = Object::cast_to<Node3D>(ObjectDB::get_instance(v.node))) n->set_visible(v.visible && atmosphere);
    }
}

}  // namespace montezuma::godot_binding::bench
