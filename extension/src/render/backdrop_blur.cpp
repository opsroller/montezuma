// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "backdrop_blur.hpp"

#include <algorithm>
#include <cstring>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/render_scene_data.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/uniform_set_cache_rd.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

const StringName& context() {
    static const StringName name("backdrop_blur");
    return name;
}
const StringName& half_name() {
    static const StringName name("half");
    return name;
}
constexpr int kGroup = 8;
// Godot's DoF radius at blur amount 1, in pixels at the target resolution.
constexpr float kRadiusPerAmount = 64.0f;

// Shared by both passes. `depth_to_distance` turns a depth-buffer value d
// into the distance in front of the camera as (x*d + y) / (z*d + w).
const char* const kCommon = R"(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(push_constant, std430) uniform Params {
    vec4 depth_to_distance;
    float far_begin;
    float far_end;
    float radius;     // blur radius at full blur, in internal pixels
    float mix_scale;  // Godot's blur radius in target pixels: the blend is full from 1 pixel up
    vec2 texel;       // 1 / internal size
    ivec2 size;       // pixels this pass writes
} p;

float coc(float d) {
    vec4 k = p.depth_to_distance;
    return smoothstep(p.far_begin, p.far_end, (k.x * d + k.y) / (k.z * d + k.w));
}
)";

// Half resolution: a 4x4 grid of bilinear taps spanning the pixel's own
// radius, each weighted by its own blur so the board (weight 0) never bleeds
// into the backdrop. Stores premultiplied colour and the summed weight, so
// the blend can filter it bilinearly across the board's edge.
const char* const kBlur = R"(
layout(set = 0, binding = 0) uniform sampler2D color_tex;
layout(set = 0, binding = 1) uniform sampler2D depth_tex;
layout(rgba16f, set = 0, binding = 2) uniform restrict writeonly image2D half_img;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pos, p.size))) return;
    vec2 uv = (vec2(pos) * 2.0 + 1.0) * p.texel;
    float c = coc(textureLod(depth_tex, uv, 0.0).r);
    if (c <= 0.0) {
        imageStore(half_img, pos, vec4(0.0));
        return;
    }
    float spacing = p.radius * c / 1.5;
    vec4 sum = vec4(0.0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 tap = uv + (vec2(x, y) - 1.5) * spacing * p.texel;
            float w = coc(textureLod(depth_tex, tap, 0.0).r);
            sum += vec4(textureLod(color_tex, tap, 0.0).rgb * w, w);
        }
    }
    imageStore(half_img, pos, sum * (1.0 / 16.0));
}
)";

// Full resolution, in place: blends the blurred backdrop over the sharp one
// as Godot's DoF composite does.
const char* const kBlend = R"(
layout(rgba16f, set = 0, binding = 0) uniform restrict image2D color_img;
layout(set = 0, binding = 1) uniform sampler2D depth_tex;
layout(set = 0, binding = 2) uniform sampler2D half_tex;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pos, p.size))) return;
    // The half texture is read first: it is zero across the board, which
    // then costs no full-resolution depth read.
    vec4 blur = textureLod(half_tex, (vec2(pos) + 0.5) * p.texel, 0.0);
    if (blur.a <= 0.0) return;
    float c = coc(texelFetch(depth_tex, pos, 0).r);
    if (c <= 0.0) return;
    vec4 color = imageLoad(color_img, pos);
    color.rgb = mix(color.rgb, blur.rgb / blur.a, clamp(c * p.mix_scale, 0.0, 1.0));
    imageStore(color_img, pos, color);
}
)";

struct Params {
    float depth_to_distance[4];
    float far_begin;
    float far_end;
    float radius;
    float mix_scale;
    float texel[2];
    int32_t size[2];
};
static_assert(sizeof(Params) == 48, "push constants are a multiple of 16 bytes");

RID compile(RenderingDevice* rd, const char* body, const char* name) {
    Ref<RDShaderSource> source;
    source.instantiate();
    source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
    source->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, String(kCommon) + body);
    const Ref<RDShaderSPIRV> spirv = rd->shader_compile_spirv_from_source(source);
    const String error = spirv.is_valid() ? spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE)
                                          : String("no SPIR-V");
    if (!error.is_empty()) {
        UtilityFunctions::push_error("BackdropBlur: ", name, " shader: ", error);
        return {};
    }
    return rd->shader_create_from_spirv(spirv, name);
}

RID sampler(RenderingDevice* rd, RenderingDevice::SamplerFilter filter) {
    Ref<RDSamplerState> state;
    state.instantiate();
    state->set_min_filter(filter);
    state->set_mag_filter(filter);
    state->set_repeat_u(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
    state->set_repeat_v(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
    return rd->sampler_create(state);
}

Ref<RDUniform> uniform(RenderingDevice::UniformType type, int binding, const RID& filter, const RID& texture) {
    Ref<RDUniform> u;
    u.instantiate();
    u->set_uniform_type(type);
    u->set_binding(binding);
    if (filter.is_valid()) u->add_id(filter);
    u->add_id(texture);
    return u;
}

void push(RenderingDevice* rd, int64_t list, const Params& params) {
    PackedByteArray bytes;
    bytes.resize(sizeof(Params));
    std::memcpy(bytes.ptrw(), &params, sizeof(Params));
    rd->compute_list_set_push_constant(list, bytes, sizeof(Params));
}

uint32_t groups(int32_t pixels) { return static_cast<uint32_t>((pixels + kGroup - 1) / kGroup); }

}  // namespace

BackdropBlur::BackdropBlur() {
    set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
}

void BackdropBlur::warm() {
    RenderingServer* rs = RenderingServer::get_singleton();
    if (rs && rs->get_rendering_device() && !blur_pipeline_.is_valid() && !failed_)
        rs->call_on_render_thread(callable_mp(this, &BackdropBlur::compile_on_render_thread));
}

void BackdropBlur::compile_on_render_thread() { ensure_pipelines(); }

void BackdropBlur::set_camera_attributes(const Ref<CameraAttributesPractical>& attributes) { attributes_ = attributes; }

Ref<BackdropBlur> BackdropBlur::in(const Ref<Compositor>& compositor) {
    if (compositor.is_null()) return {};
    const TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
    for (int64_t i = 0; i < effects.size(); ++i) {
        Ref<BackdropBlur> blur = effects[i];
        if (blur.is_valid()) return blur;
    }
    return {};
}

Ref<BackdropBlur> BackdropBlur::drawing(Viewport* viewport) {
    if (!viewport) return {};
    // Godot takes the camera's compositor over the world's.
    if (const Camera3D* camera = viewport->get_camera_3d(); camera && camera->get_compositor().is_valid())
        return in(camera->get_compositor());
    return under(viewport);
}

Ref<BackdropBlur> BackdropBlur::under(Node* root) {
    if (!root) return {};
    const TypedArray<Node> worlds = root->find_children("*", "WorldEnvironment", true, false);
    for (int64_t i = 0; i < worlds.size(); ++i) {
        if (auto* world = Object::cast_to<WorldEnvironment>(worlds[i].operator Object*())) {
            if (Ref<BackdropBlur> blur = in(world->get_compositor()); blur.is_valid()) return blur;
        }
    }
    return {};
}

bool BackdropBlur::ensure_pipelines() {
    if (blur_pipeline_.is_valid()) return true;
    if (failed_) return false;
    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    if (!rd) return false;
    blur_shader_ = compile(rd, kBlur, "BackdropBlur blur");
    blend_shader_ = compile(rd, kBlend, "BackdropBlur blend");
    if (!blur_shader_.is_valid() || !blend_shader_.is_valid()) {
        failed_ = true;
        free_pipelines();
        return false;
    }
    blur_pipeline_ = rd->compute_pipeline_create(blur_shader_);
    blend_pipeline_ = rd->compute_pipeline_create(blend_shader_);
    linear_sampler_ = sampler(rd, RenderingDevice::SAMPLER_FILTER_LINEAR);
    nearest_sampler_ = sampler(rd, RenderingDevice::SAMPLER_FILTER_NEAREST);
    if (!blur_pipeline_.is_valid() || !blend_pipeline_.is_valid() || !linear_sampler_.is_valid() ||
        !nearest_sampler_.is_valid()) {
        UtilityFunctions::push_error("BackdropBlur: cannot create its pipelines or samplers");
        failed_ = true;
        free_pipelines();
        return false;
    }
    return true;
}

void BackdropBlur::free_pipelines() {
    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs ? rs->get_rendering_device() : nullptr;
    if (!rd) return;
    // Freeing a shader frees the pipelines made from it.
    for (RID* rid : {&blur_shader_, &blend_shader_, &linear_sampler_, &nearest_sampler_}) {
        if (rid->is_valid()) rd->free_rid(*rid);
        *rid = RID();
    }
    blur_pipeline_ = RID();
    blend_pipeline_ = RID();
}

void BackdropBlur::_render_callback(int32_t p_effect_callback_type, RenderData* p_render_data) {
    if (p_effect_callback_type != EFFECT_CALLBACK_TYPE_POST_TRANSPARENT || attributes_.is_null()) return;
    const float amount = attributes_->get_dof_blur_amount();
    if (amount <= 0.0f || !ensure_pipelines()) return;
    Ref<RenderSceneBuffersRD> buffers = p_render_data->get_render_scene_buffers();
    RenderSceneData* scene = p_render_data->get_render_scene_data();
    if (buffers.is_null() || !scene) return;
    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    // A GPU timestamp orders the whole frame graph around it, so the pass
    // only marks itself when Godot is capturing its own (--gpu-profile).
    if (rd->get_captured_timestamps_count() > 0) rd->capture_timestamp("Backdrop Blur");

    const Vector2i internal = buffers->get_internal_size();
    const Vector2i target = buffers->get_target_size();
    if (internal.x <= 0 || internal.y <= 0 || target.x <= 0) return;
    const Vector2i half((internal.x + 1) / 2, (internal.y + 1) / 2);
    if (!buffers->has_texture(context(), half_name())) {
        buffers->create_texture(context(), half_name(), RenderingDevice::DATA_FORMAT_R16G16B16A16_SFLOAT,
                                RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_STORAGE_BIT,
                                RenderingDevice::TEXTURE_SAMPLES_1, half, buffers->get_view_count(), 1, true, false);
    }

    Params params{};
    params.far_begin = attributes_->get_dof_blur_far_distance();
    // A negative transition is Godot's physical mode; ours always ramps.
    params.far_end = params.far_begin + std::max(attributes_->get_dof_blur_far_transition(), 0.001f);
    params.mix_scale = amount * kRadiusPerAmount;
    params.radius = params.mix_scale * static_cast<float>(internal.x) / static_cast<float>(target.x);
    params.texel[0] = 1.0f / static_cast<float>(internal.x);
    params.texel[1] = 1.0f / static_cast<float>(internal.y);

    for (uint32_t view = 0; view < buffers->get_view_count(); ++view) {
        // The projection already maps to the depth buffer's reversed 0..1 range,
        // so its inverse takes a depth value d straight to view space.
        const Projection inv = scene->get_view_projection(view).inverse();
        params.depth_to_distance[0] = -inv.columns[2].z;
        params.depth_to_distance[1] = -inv.columns[3].z;
        params.depth_to_distance[2] = inv.columns[2].w;
        params.depth_to_distance[3] = inv.columns[3].w;

        const RID color = buffers->get_color_layer(view);
        const RID depth = buffers->get_depth_layer(view);
        const RID half_texture = buffers->get_texture_slice(context(), half_name(), view, 0, 1, 1);

        TypedArray<RDUniform> blur_set;
        blur_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, linear_sampler_, color));
        blur_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, nearest_sampler_, depth));
        blur_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_IMAGE, 2, RID(), half_texture));
        TypedArray<RDUniform> blend_set;
        blend_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_IMAGE, 0, RID(), color));
        blend_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, nearest_sampler_, depth));
        blend_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 2, linear_sampler_, half_texture));

        const int64_t list = rd->compute_list_begin();
        rd->compute_list_bind_compute_pipeline(list, blur_pipeline_);
        rd->compute_list_bind_uniform_set(list, UniformSetCacheRD::get_cache(blur_shader_, 0, blur_set), 0);
        params.size[0] = half.x;
        params.size[1] = half.y;
        push(rd, list, params);
        rd->compute_list_dispatch(list, groups(half.x), groups(half.y), 1);
        rd->compute_list_add_barrier(list);

        rd->compute_list_bind_compute_pipeline(list, blend_pipeline_);
        rd->compute_list_bind_uniform_set(list, UniformSetCacheRD::get_cache(blend_shader_, 0, blend_set), 0);
        params.size[0] = internal.x;
        params.size[1] = internal.y;
        push(rd, list, params);
        rd->compute_list_dispatch(list, groups(internal.x), groups(internal.y), 1);
        rd->compute_list_end();
    }
}

void BackdropBlur::_notification(int p_what) {
    if (p_what == NOTIFICATION_PREDELETE) free_pipelines();
}

void BackdropBlur::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_camera_attributes", "attributes"), &BackdropBlur::set_camera_attributes);
    ClassDB::bind_method(D_METHOD("get_camera_attributes"), &BackdropBlur::get_camera_attributes);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "camera_attributes", PROPERTY_HINT_RESOURCE_TYPE,
                              "CameraAttributesPractical"),
                 "set_camera_attributes", "get_camera_attributes");
}

}  // namespace montezuma::godot_binding
