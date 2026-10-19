// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "rt_reflections.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rd_acceleration_structure_geometry.hpp>
#include <godot_cpp/classes/rd_acceleration_structure_instance.hpp>
#include <godot_cpp/classes/rd_hit_group.hpp>
#include <godot_cpp/classes/rd_pipeline_shader.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/render_scene_data.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/uniform_set_cache_rd.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr int kGroup = 8;  // resolve pass work group size
// Nodes in this group lend their whole subtree to the ray-traced scene.
const char* const kGroupName = "ray_traced";
// A pixel is on a piece when the camera ray hits one within this fraction
// of the depth-buffer distance; a reflection hit counts as on screen when
// the depth there is within the same fraction of the hit's distance.
constexpr float kDepthTolerance = 0.01f;
// Mirror rays stop after this many world units (a board is about 8).
constexpr float kMaxRayDistance = 64.0f;
// The self-test checks after this many drawn frames.
constexpr uint64_t kSelfTestFrames = 90;

const StringName& context() {
    static const StringName name("rt_reflections");
    return name;
}
const StringName& out_name() {
    static const StringName name("out");
    return name;
}

// ---------------------------------------------------------------------------
// Scene snapshot: gathered on the main thread before each frame is drawn,
// read by whichever RtReflections draws it.

struct MeshCpu {
    std::vector<float> positions;  // xyz per vertex
    std::vector<uint32_t> indices;
    std::vector<float> triangle_normals;  // rt::pack_triangle_normals
};

struct InstanceCpu {
    uint64_t mesh = 0;
    Transform3D transform;
    rt::InstanceMaterial material;
};

struct Shared {
    std::mutex mutex;
    std::vector<InstanceCpu> instances;
    std::unordered_map<uint64_t, std::shared_ptr<const MeshCpu>> meshes;
    uint64_t gathered_frames = 0;
    uint64_t last_traced_frame = 0;
    bool connected = false;
    bool warned_unsupported = false;
    bool warned_overflow = false;
    bool announced = false;
    std::vector<ObjectID> effects;
};

Shared& shared() {
    static Shared s;
    return s;
}

// Material values the shader needs, cached per material; empty for a
// transparent base material (an effect drawn over the scene).
struct MaterialInfo {
    rt::Rgb albedo;
    rt::Rgb f0;
    float weight = 0.0f;
};
std::unordered_map<uint64_t, std::optional<MaterialInfo>>& material_cache() {
    static std::unordered_map<uint64_t, std::optional<MaterialInfo>> cache;
    return cache;
}

// Mean of one texture's channels, sampled small; white when there is none.
Color texture_mean(const Ref<Texture2D>& texture) {
    if (texture.is_null()) return Color(1, 1, 1, 1);
    Ref<Image> image = texture->get_image();
    if (image.is_null() || image->is_empty()) return Color(1, 1, 1, 1);
    image = image->duplicate();
    if (image->is_compressed() && image->decompress() != OK) return Color(1, 1, 1, 1);
    image->clear_mipmaps();
    image->resize(8, 8, Image::INTERPOLATE_BILINEAR);
    Color sum(0, 0, 0, 0);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) sum += image->get_pixel(x, y);
    }
    return sum / 64.0f;
}

rt::Rgb rgb(const Color& c) { return {c.r, c.g, c.b}; }

std::optional<MaterialInfo> read_material(const Ref<Material>& material) {
    if (material.is_null()) return std::nullopt;
    float metalness = 0.0f;
    float roughness = 1.0f;
    float ior = rt::kDefaultIor;
    Color albedo(1, 1, 1, 1);
    if (const Ref<BaseMaterial3D> base = material; base.is_valid()) {
        // Pieces are opaque; transparent ones are effects drawn over them.
        if (base->get_transparency() != BaseMaterial3D::TRANSPARENCY_DISABLED) return std::nullopt;
        albedo = base->get_albedo() * texture_mean(base->get_texture(BaseMaterial3D::TEXTURE_ALBEDO));
        if (const Ref<Texture2D> orm = base->get_texture(BaseMaterial3D::TEXTURE_ORM); orm.is_valid()) {
            const Color mean = texture_mean(orm);  // occlusion, roughness, metallic
            roughness = mean.g;
            metalness = mean.b;
        } else {
            roughness = base->get_roughness() * texture_mean(base->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS)).r;
            metalness = base->get_metallic() * texture_mean(base->get_texture(BaseMaterial3D::TEXTURE_METALLIC)).r;
        }
    } else if (const Ref<ShaderMaterial> shader = material; shader.is_valid()) {
        // Custom shaders name their parameters as the project's base
        // materials do (albedo, texture_albedo, texture_orm); a gem's also
        // names its index of refraction. Without an ORM texture a surface
        // counts as fully rough: it is reflected, never traced from.
        const Variant ior_value = shader->get_shader_parameter("ior");
        if (ior_value.get_type() == Variant::FLOAT || ior_value.get_type() == Variant::INT)
            ior = static_cast<float>(double(ior_value));
        const Variant albedo_value = shader->get_shader_parameter("albedo");
        if (albedo_value.get_type() == Variant::COLOR) albedo = albedo_value;
        albedo *= texture_mean(shader->get_shader_parameter("texture_albedo"));
        if (const Ref<Texture2D> orm = shader->get_shader_parameter("texture_orm"); orm.is_valid()) {
            const Color mean = texture_mean(orm);
            roughness = mean.g;
            metalness = mean.b;
        }
    } else {
        return std::nullopt;
    }
    MaterialInfo info;
    info.albedo = rgb(albedo);
    info.f0 = rt::material_f0(info.albedo, metalness, ior);
    info.weight = rt::roughness_weight(roughness);
    return info;
}

std::optional<MaterialInfo> material_info(const Ref<Material>& material) {
    if (material.is_null()) return std::nullopt;
    auto& cache = material_cache();
    const uint64_t key = material->get_instance_id();
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    return cache[key] = read_material(material);
}

std::shared_ptr<const MeshCpu> read_mesh(const Ref<Mesh>& mesh) {
    auto out = std::make_shared<MeshCpu>();
    std::vector<float> normals;
    for (int32_t s = 0; s < mesh->get_surface_count(); ++s) {
        // Only triangle lists: Godot's other primitives are lines and points.
        const Array arrays = mesh->surface_get_arrays(s);
        if (arrays.size() <= Mesh::ARRAY_INDEX) continue;
        const PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
        const PackedVector3Array surface_normals = arrays[Mesh::ARRAY_NORMAL];
        const PackedInt32Array surface_indices = arrays[Mesh::ARRAY_INDEX];
        const auto base = static_cast<uint32_t>(out->positions.size() / 3);
        for (int64_t i = 0; i < vertices.size(); ++i) {
            out->positions.insert(out->positions.end(), {vertices[i].x, vertices[i].y, vertices[i].z});
            const Vector3 n = i < surface_normals.size() ? surface_normals[i] : Vector3();
            normals.insert(normals.end(), {n.x, n.y, n.z});
        }
        if (surface_indices.is_empty()) {
            for (int64_t i = 0; i + 2 < vertices.size(); i += 3) {
                for (int k = 0; k < 3; ++k) out->indices.push_back(base + static_cast<uint32_t>(i + k));
            }
        } else {
            for (int64_t i = 0; i + 2 < surface_indices.size(); i += 3) {
                for (int k = 0; k < 3; ++k) out->indices.push_back(base + static_cast<uint32_t>(surface_indices[i + k]));
            }
        }
    }
    if (out->indices.empty()) return nullptr;
    out->triangle_normals = rt::pack_triangle_normals(out->positions, normals, out->indices);
    return out;
}

bool known_mesh(const Ref<Mesh>& mesh, std::unordered_map<uint64_t, std::shared_ptr<const MeshCpu>>& meshes) {
    if (mesh.is_null()) return false;
    const uint64_t key = mesh->get_rid().get_id();
    if (auto it = meshes.find(key); it != meshes.end()) return it->second != nullptr;
    meshes[key] = read_mesh(mesh);
    return meshes[key] != nullptr;
}

Ref<Material> surface_material(GeometryInstance3D* geometry, const Ref<Mesh>& mesh) {
    if (Ref<Material> m = geometry->get_material_override(); m.is_valid()) return m;
    if (auto* mi = Object::cast_to<MeshInstance3D>(geometry)) return mi->get_active_material(0);
    return mesh.is_valid() && mesh->get_surface_count() > 0 ? mesh->surface_get_material(0) : Ref<Material>();
}

void add_instance(std::vector<InstanceCpu>& out, const Ref<Mesh>& mesh, const Transform3D& transform,
                  const MaterialInfo& info) {
    InstanceCpu inst;
    inst.mesh = mesh->get_rid().get_id();
    inst.transform = transform;
    inst.material.albedo = info.albedo;
    inst.material.f0 = info.f0;
    inst.material.weight = info.weight;
    out.push_back(inst);
}

void gather_node(Node* node, std::vector<InstanceCpu>& out,
                 std::unordered_map<uint64_t, std::shared_ptr<const MeshCpu>>& meshes) {
    if (auto* geometry = Object::cast_to<GeometryInstance3D>(node); geometry && geometry->is_visible_in_tree()) {
        if (auto* mi = Object::cast_to<MeshInstance3D>(geometry)) {
            const Ref<Mesh> mesh = mi->get_mesh();
            const std::optional<MaterialInfo> info = material_info(surface_material(mi, mesh));
            if (info && known_mesh(mesh, meshes)) add_instance(out, mesh, mi->get_global_transform(), *info);
        } else if (auto* mmi = Object::cast_to<MultiMeshInstance3D>(geometry)) {
            const Ref<MultiMesh> mm = mmi->get_multimesh();
            const Ref<Mesh> mesh = mm.is_valid() ? mm->get_mesh() : Ref<Mesh>();
            const std::optional<MaterialInfo> info = material_info(surface_material(mmi, mesh));
            if (info && known_mesh(mesh, meshes) && mm->get_transform_format() == MultiMesh::TRANSFORM_3D) {
                const int32_t visible = mm->get_visible_instance_count();
                const int32_t count = visible < 0 ? mm->get_instance_count() : visible;
                const Transform3D global = mmi->get_global_transform();
                for (int32_t i = 0; i < count; ++i) add_instance(out, mesh, global * mm->get_instance_transform(i), *info);
            }
        }
    }
    const int32_t children = node->get_child_count();
    for (int32_t i = 0; i < children; ++i) gather_node(node->get_child(i), out, meshes);
}

void run_self_test(SceneTree* tree);

// Connected to RenderingServer::frame_pre_draw once an effect is attached.
void gather() {
    SceneTree* tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree) return;
    Shared& s = shared();
    std::vector<InstanceCpu> instances;
    std::unordered_map<uint64_t, std::shared_ptr<const MeshCpu>> meshes;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        meshes = s.meshes;
    }
    const TypedArray<Node> roots = tree->get_nodes_in_group(kGroupName);
    for (int64_t i = 0; i < roots.size(); ++i) {
        if (auto* root = Object::cast_to<Node>(roots[i].operator Object*())) gather_node(root, instances, meshes);
    }
    if (instances.size() > rt::kMaxInstances) {
        if (!s.warned_overflow) {
            UtilityFunctions::push_warning("RtReflections: more than ", rt::kMaxInstances,
                                           " pieces; the rest are not ray traced");
            s.warned_overflow = true;
        }
        instances.resize(rt::kMaxInstances);
    }
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.instances = std::move(instances);
        s.meshes = std::move(meshes);
        ++s.gathered_frames;
    }
    // The board view copies its moved pieces into their batches on this same
    // signal; reconnecting keeps this gather after every other handler, so
    // the scene it reads is the one about to be drawn.
    RenderingServer* rs = RenderingServer::get_singleton();
    const Callable self = callable_mp_static(&gather);
    rs->disconnect("frame_pre_draw", self);
    rs->connect("frame_pre_draw", self);
    if (RtReflections::options().self_test && s.gathered_frames == kSelfTestFrames) run_self_test(tree);
}

// ---------------------------------------------------------------------------
// Shaders.

// One shader with the three ray-tracing stages; the ray generation stage
// does the work, the hit stage reports where and what it hit.
const char* const kTraceCommon = R"(
#version 460
#extension GL_EXT_ray_tracing : require

layout(rgba16f, set = 0, binding = 0) uniform restrict readonly image2D color_img;
layout(set = 0, binding = 1) uniform sampler2D depth_tex;
layout(set = 0, binding = 2) uniform accelerationStructureEXT tlas;
// Two vec4s per instance: albedo + first triangle, f0 + trace weight.
layout(set = 0, binding = 3, std430) restrict readonly buffer Instances { vec4 instances[]; };
// Three vec4s per triangle: one normal per corner, object space.
layout(set = 0, binding = 4, std430) restrict readonly buffer Normals { vec4 normals[]; };
layout(set = 0, binding = 5, std140) uniform Frame {
    mat4 clip_to_view;
    mat4 view_to_world;
    mat4 world_to_clip;
    vec4 params;  // strength, depth tolerance, offscreen ambient, max ray distance
    vec4 size;    // width, height, 1 / width, 1 / height
    vec4 debug;   // x > 0: paint receivers blue, screen hits green, hidden hits red
} f;
layout(rgba16f, set = 0, binding = 6) uniform restrict writeonly image2D out_img;

struct Hit {
    mat3 to_world;  // the hit instance's object-to-world rotation and scale
    vec2 bary;
    int primitive;
    int instance;
    float t;        // < 0: missed
};
)";

const char* const kRaygen = R"(
layout(location = 0) rayPayloadEXT Hit hit;

void trace(vec3 origin, float t_min, vec3 dir, float t_max) {
    hit.t = -1.0;
    traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, origin, t_min, dir, t_max, 0);
}

// The hit's world-space normal, interpolated from its triangle's corners.
// Read here rather than in the hit stage: Godot 4.7.2's shader reflection
// gives up on ray-tracing stages, so only this stage's bindings are sure.
vec3 hit_normal() {
    int tri = (int(instances[hit.instance * 2].w) + hit.primitive) * 3;
    vec3 n = normals[tri].xyz * (1.0 - hit.bary.x - hit.bary.y) + normals[tri + 1].xyz * hit.bary.x +
             normals[tri + 2].xyz * hit.bary.y;
    return normalize(transpose(inverse(hit.to_world)) * n);
}

vec3 world_at(ivec2 pos, float depth) {
    vec2 ndc = (vec2(pos) + 0.5) * f.size.zw * 2.0 - 1.0;
    vec4 v = f.clip_to_view * vec4(ndc, depth, 1.0);
    return (f.view_to_world * vec4(v.xyz / v.w, 1.0)).xyz;
}

void main() {
    ivec2 pos = ivec2(gl_LaunchIDEXT.xy);
    vec4 result = vec4(0.0);  // alpha 0: leave the pixel as it is
    float depth = texelFetch(depth_tex, pos, 0).r;
    if (depth > 0.0) {  // reversed Z: 0 is the far plane
        vec3 eye = f.view_to_world[3].xyz;
        vec3 dir = world_at(pos, depth) - eye;
        float dist = length(dir);
        dir /= dist;
        float tol = f.params.y * dist;
        trace(eye, 0.0, dir, dist + tol);
        if (hit.t > dist - tol && instances[hit.instance * 2 + 1].w > 0.0) {
            vec4 surface = instances[hit.instance * 2 + 1];
            vec3 n = hit_normal();
            if (dot(n, dir) > 0.0) n = -n;
            float bias = 0.001 * dist;
            vec3 p = eye + dir * hit.t + n * bias;
            vec3 r = reflect(dir, n);
            trace(p, bias, r, f.params.w);
            if (hit.t >= 0.0) {
                vec3 h = p + r * hit.t;
                vec3 seen = instances[hit.instance * 2].rgb * f.params.z;
                bool on_screen = false;
                vec4 clip = f.world_to_clip * vec4(h, 1.0);
                if (clip.w > 0.0) {
                    ivec2 at = ivec2((clip.xy / clip.w * 0.5 + 0.5) * f.size.xy);
                    if (all(greaterThanEqual(at, ivec2(0))) && all(lessThan(at, ivec2(f.size.xy)))) {
                        float d = texelFetch(depth_tex, at, 0).r;
                        if (d > 0.0 && distance(world_at(at, d), h) < f.params.y * distance(eye, h)) {
                            seen = imageLoad(color_img, at).rgb;
                            on_screen = true;
                        }
                    }
                }
                vec3 fresnel = surface.rgb + (1.0 - surface.rgb) * pow(1.0 - clamp(dot(n, -dir), 0.0, 1.0), 5.0);
                vec3 w = clamp(fresnel * surface.w * f.params.x, 0.0, 1.0);
                result = vec4(mix(imageLoad(color_img, pos).rgb, seen, w), 1.0);
                if (f.debug.x > 0.0) result.rgb = on_screen ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
            } else if (f.debug.x > 0.0) {
                result = vec4(0.0, 0.0, 0.6 * surface.w, 1.0);
            }
        }
    }
    imageStore(out_img, pos, result);
}
)";

const char* const kMiss = R"(
layout(location = 0) rayPayloadInEXT Hit hit;

void main() { hit.t = -1.0; }
)";

const char* const kClosestHit = R"(
layout(location = 0) rayPayloadInEXT Hit hit;
hitAttributeEXT vec2 bary;

void main() {
    hit.to_world = mat3(gl_ObjectToWorldEXT);
    hit.bary = bary;
    hit.primitive = gl_PrimitiveID;
    hit.instance = gl_InstanceCustomIndexEXT;
    hit.t = gl_HitTEXT;
}
)";

// Copies the traced pixels back over the frame.
const char* const kResolve = R"(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba16f, set = 0, binding = 0) uniform restrict image2D color_img;
layout(rgba16f, set = 0, binding = 1) uniform restrict readonly image2D out_img;
layout(push_constant, std430) uniform Params { ivec2 size; ivec2 pad; } p;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pos, p.size))) return;
    vec4 traced = imageLoad(out_img, pos);
    if (traced.a <= 0.0) return;
    imageStore(color_img, pos, vec4(traced.rgb, imageLoad(color_img, pos).a));
}
)";

struct Frame {
    float clip_to_view[16];
    float view_to_world[16];
    float world_to_clip[16];
    float params[4];
    float size[4];
    float debug[4];
};
static_assert(sizeof(Frame) == 240, "std140 layout");

void store(const Projection& p, float* out) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) out[c * 4 + r] = static_cast<float>(p.columns[c][r]);
    }
}

RID compile(RenderingDevice* rd, const std::vector<std::pair<RenderingDevice::ShaderStage, String>>& stages,
            const char* name) {
    Ref<RDShaderSource> source;
    source.instantiate();
    source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
    for (const auto& [stage, text] : stages) source->set_stage_source(stage, text);
    const Ref<RDShaderSPIRV> spirv = rd->shader_compile_spirv_from_source(source);
    for (const auto& [stage, text] : stages) {
        const String error = spirv.is_valid() ? spirv->get_stage_compile_error(stage) : String("no SPIR-V");
        if (!error.is_empty()) {
            UtilityFunctions::push_error("RtReflections: ", name, " shader: ", error);
            return {};
        }
    }
    return rd->shader_create_from_spirv(spirv, name);
}

Ref<RDUniform> uniform(RenderingDevice::UniformType type, int binding, const RID& id, const RID& sampler = RID()) {
    Ref<RDUniform> u;
    u.instantiate();
    u->set_uniform_type(type);
    u->set_binding(binding);
    if (sampler.is_valid()) u->add_id(sampler);
    u->add_id(id);
    return u;
}

template <typename T>
PackedByteArray bytes_of(const T* data, size_t count) {
    PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(count * sizeof(T)));
    if (count) std::memcpy(bytes.ptrw(), data, count * sizeof(T));
    return bytes;
}

uint32_t groups(int32_t pixels) { return static_cast<uint32_t>((pixels + kGroup - 1) / kGroup); }

// Vertex and index buffers an acceleration structure is built from.
BitField<RenderingDevice::BufferCreationBits> build_input() {
    return RenderingDevice::BUFFER_CREATION_DEVICE_ADDRESS_BIT |
           RenderingDevice::BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT;
}

void run_self_test(SceneTree* tree) {
    Shared& s = shared();
    int failures = 0;
    auto check = [&](bool ok, const String& what) {
        UtilityFunctions::print(ok ? "  ok   " : "  FAIL ", what);
        if (!ok) ++failures;
    };
    UtilityFunctions::print("RtReflections self-test:");
    check(RtReflections::is_supported(), "Forward+ GPU with ray-tracing pipelines");
    size_t instances = 0;
    std::set<uint64_t> distinct;
    std::vector<ObjectID> effects;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        instances = s.instances.size();
        for (const InstanceCpu& i : s.instances) distinct.insert(i.mesh);
        effects = s.effects;
    }
    check(instances > 0, "pieces gathered: " + String::num_int64(static_cast<int64_t>(instances)));
    RtReflections* drawing = nullptr;
    for (const ObjectID& id : effects) {
        auto* e = Object::cast_to<RtReflections>(ObjectDB::get_instance(id));
        if (e && e->get_enabled() && e->get_traced_frames() > 0) drawing = e;
    }
    check(drawing != nullptr, "an attached RtReflections traced frames");
    if (drawing) {
        check(drawing->get_instance_count() == static_cast<int>(instances),
              "TLAS instances " + String::num_int64(drawing->get_instance_count()) + " == pieces gathered");
        check(drawing->get_blas_count() >= static_cast<int>(distinct.size()),
              "BLASes " + String::num_int64(drawing->get_blas_count()) + " cover the " +
                  String::num_int64(static_cast<int64_t>(distinct.size())) + " meshes on the board");
    }
    check(RtReflections::tracing(), "traced in the last frames");
    UtilityFunctions::print(failures == 0 ? "RtReflections self-test passed" : "RtReflections self-test FAILED");
    tree->quit(failures == 0 ? 0 : 1);
}

}  // namespace

// ---------------------------------------------------------------------------

RtReflections::RtReflections() {
    set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
}

const rt::Options& RtReflections::options() {
    static const rt::Options parsed = [] {
        std::vector<std::string> args;
        const PackedStringArray user = OS::get_singleton()->get_cmdline_user_args();
        for (int64_t i = 0; i < user.size(); ++i) args.emplace_back(user[i].utf8().get_data());
        return rt::parse_options(args);
    }();
    return parsed;
}

bool RtReflections::is_supported() {
    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs ? rs->get_rendering_device() : nullptr;
    return rd && rs->get_current_rendering_method() == "forward_plus" &&
           rd->has_feature(RenderingDevice::SUPPORTS_RAYTRACING_PIPELINE);
}

bool RtReflections::tracing() {
    Shared& s = shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.last_traced_frame > 0 && s.gathered_frames - s.last_traced_frame < 3;
}

Ref<RtReflections> RtReflections::in(const Ref<Compositor>& compositor) {
    if (compositor.is_null()) return {};
    const TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
    for (int64_t i = 0; i < effects.size(); ++i) {
        Ref<RtReflections> rt = effects[i];
        if (rt.is_valid()) return rt;
    }
    return {};
}

Ref<Compositor> RtReflections::attach(Ref<Compositor> compositor) {
    const rt::Options& o = options();
    if (!o.enabled) return compositor;
    RenderingServer* rs = RenderingServer::get_singleton();
    const String method = rs->get_current_rendering_method();
    const bool ray_tracing = is_supported();
    Shared& s = shared();
    if (!rt::should_attach(true, o, method.utf8().get_data(), ray_tracing)) {
        if (!s.warned_unsupported) {
            UtilityFunctions::push_warning("RtReflections: --rt-reflections needs Forward+ on a GPU with ray-tracing pipelines (",
                                           method, "); running without it");
            s.warned_unsupported = true;
        }
        return compositor;
    }
    if (in(compositor).is_valid()) return compositor;
    if (compositor.is_null()) compositor.instantiate();
    Ref<RtReflections> effect;
    effect.instantiate();
    effect->set_strength(o.strength);
    TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
    effects.push_back(effect);
    compositor->set_compositor_effects(effects);
    rs->call_on_render_thread(callable_mp(effect.ptr(), &RtReflections::compile_on_render_thread));
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.effects.push_back(ObjectID(effect->get_instance_id()));
        if (!s.connected) {
            s.connected = true;
            rs->connect("frame_pre_draw", callable_mp_static(&gather));
        }
    }
    if (!s.announced) {
        UtilityFunctions::print("RtReflections: on (strength ", o.strength, ")");
        s.announced = true;
    }
    return compositor;
}

void RtReflections::set_strength(float strength) { strength_ = std::clamp(strength, 0.0f, rt::kMaxStrength); }

void RtReflections::compile_on_render_thread() { ensure_pipelines(); }

bool RtReflections::ensure_pipelines() {
    if (trace_pipeline_.is_valid()) return true;
    if (failed_) return false;
    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    if (!rd) return false;
    const String common(kTraceCommon);
    trace_shader_ = compile(rd,
                            {{RenderingDevice::SHADER_STAGE_RAYGEN, common + String(kRaygen)},
                             {RenderingDevice::SHADER_STAGE_MISS, common + String(kMiss)},
                             {RenderingDevice::SHADER_STAGE_CLOSEST_HIT, common + String(kClosestHit)}},
                            "RtReflections trace");
    resolve_shader_ = compile(rd, {{RenderingDevice::SHADER_STAGE_COMPUTE, String(kResolve)}}, "RtReflections resolve");
    if (trace_shader_.is_valid()) {
        Ref<RDPipelineShader> stage;
        stage.instantiate();
        stage->set_shader(trace_shader_);
        Ref<RDHitGroup> group;
        group.instantiate();
        group->set_closest_hit_shader(stage);
        TypedArray<RDPipelineShader> raygen;
        raygen.push_back(stage);
        TypedArray<RDPipelineShader> miss;
        miss.push_back(stage);
        TypedArray<RDHitGroup> hits;
        hits.push_back(group);
        trace_pipeline_ = rd->raytracing_pipeline_create(raygen, miss, hits, 1);
        // Every instance uses the one hit group.
        if (trace_pipeline_.is_valid()) hit_table_ = rd->hit_sbt_create(trace_pipeline_, 1);
        if (hit_table_.is_valid()) hit_range_ = rd->hit_sbt_range_alloc(hit_table_, 1);
        if (hit_range_ != 0) rd->hit_sbt_range_update(hit_table_, hit_range_, 0, PackedInt32Array({0}));
    }
    if (resolve_shader_.is_valid()) resolve_pipeline_ = rd->compute_pipeline_create(resolve_shader_);
    Ref<RDSamplerState> state;
    state.instantiate();
    state->set_repeat_u(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
    state->set_repeat_v(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
    nearest_sampler_ = rd->sampler_create(state);
    frame_buffer_ = rd->uniform_buffer_create(sizeof(Frame));
    instance_buffer_ = rd->storage_buffer_create(rt::kMaxInstances * rt::kFloatsPerInstance * sizeof(float));
    tlas_ = rd->tlas_create(rt::kMaxInstances, RenderingDevice::ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT);
    if (!trace_pipeline_.is_valid() || hit_range_ == 0 || !resolve_pipeline_.is_valid() || !nearest_sampler_.is_valid() ||
        !frame_buffer_.is_valid() || !instance_buffer_.is_valid() || !tlas_.is_valid()) {
        UtilityFunctions::push_error("RtReflections: cannot create its pipelines or buffers; turned off");
        failed_ = true;
        free_all();
        return false;
    }
    return true;
}

bool RtReflections::upload_meshes() {
    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    std::vector<std::pair<uint64_t, std::shared_ptr<const MeshCpu>>> missing;
    {
        Shared& s = shared();
        std::lock_guard<std::mutex> lock(s.mutex);
        for (const auto& [key, mesh] : s.meshes) {
            if (mesh && !meshes_.count(key)) missing.emplace_back(key, mesh);
        }
    }
    for (const auto& [key, mesh] : missing) {
        GpuMesh gpu;
        const auto vertex_count = static_cast<uint32_t>(mesh->positions.size() / 3);
        const PackedByteArray vertex_bytes = bytes_of(mesh->positions.data(), mesh->positions.size());
        const PackedByteArray index_bytes = bytes_of(mesh->indices.data(), mesh->indices.size());
        gpu.vertices = rd->vertex_buffer_create(vertex_bytes.size(), vertex_bytes, build_input());
        gpu.indices = rd->index_buffer_create(static_cast<int64_t>(mesh->indices.size()),
                                              RenderingDevice::INDEX_BUFFER_FORMAT_UINT32, index_bytes, false,
                                              build_input());
        Ref<RDAccelerationStructureGeometry> geometry;
        geometry.instantiate();
        geometry->set_flags(RenderingDevice::ACCELERATION_STRUCTURE_GEOMETRY_OPAQUE_BIT);
        geometry->set_vertex_buffer(gpu.vertices);
        geometry->set_vertex_format(RenderingDevice::DATA_FORMAT_R32G32B32_SFLOAT);
        geometry->set_vertex_stride(3 * sizeof(float));
        geometry->set_vertex_count(vertex_count);
        geometry->set_index_buffer(gpu.indices);
        geometry->set_index_count(static_cast<int64_t>(mesh->indices.size()));
        TypedArray<RDAccelerationStructureGeometry> geometries;
        geometries.push_back(geometry);
        if (gpu.vertices.is_valid() && gpu.indices.is_valid())
            gpu.blas = rd->blas_create(geometries, RenderingDevice::ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT);
        if (!gpu.blas.is_valid() || rd->blas_build(gpu.blas) != OK) {
            UtilityFunctions::push_error("RtReflections: cannot build a mesh's acceleration structure; turned off");
            for (RID* rid : {&gpu.blas, &gpu.indices, &gpu.vertices}) {
                if (rid->is_valid()) rd->free_rid(*rid);
            }
            failed_ = true;
            return false;
        }
        gpu.first_triangle = static_cast<uint32_t>(normals_.size() / rt::kFloatsPerTriangle);
        normals_.insert(normals_.end(), mesh->triangle_normals.begin(), mesh->triangle_normals.end());
        normals_dirty_ = true;
        meshes_.emplace(key, gpu);
    }
    if (normals_dirty_) {
        if (normal_buffer_.is_valid()) rd->free_rid(normal_buffer_);
        const PackedByteArray bytes = bytes_of(normals_.data(), normals_.size());
        normal_buffer_ = rd->storage_buffer_create(bytes.size(), bytes);
        normals_dirty_ = false;
    }
    return normal_buffer_.is_valid();
}

void RtReflections::_render_callback(int32_t p_effect_callback_type, RenderData* p_render_data) {
    if (p_effect_callback_type != EFFECT_CALLBACK_TYPE_POST_TRANSPARENT || strength_ <= 0.0f) return;
    if (!ensure_pipelines()) return;
    Ref<RenderSceneBuffersRD> buffers = p_render_data->get_render_scene_buffers();
    RenderSceneData* scene = p_render_data->get_render_scene_data();
    if (buffers.is_null() || !scene) return;
    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();

    std::vector<InstanceCpu> instances;
    uint64_t frame = 0;
    {
        Shared& s = shared();
        std::lock_guard<std::mutex> lock(s.mutex);
        instances = s.instances;
        frame = s.gathered_frames;
    }
    instance_count_ = static_cast<int>(instances.size());
    if (instances.empty() || !upload_meshes()) return;

    // The acceleration structure for this frame's pieces.
    TypedArray<RDAccelerationStructureInstance> tlas_instances;
    std::vector<rt::InstanceMaterial> materials;
    materials.reserve(instances.size());
    for (const InstanceCpu& inst : instances) {
        const auto it = meshes_.find(inst.mesh);
        if (it == meshes_.end()) continue;
        Ref<RDAccelerationStructureInstance> as;
        as.instantiate();
        as->set_transform(inst.transform);
        as->set_id(static_cast<int64_t>(materials.size()));
        as->set_mask(0xFF);
        as->set_hit_sbt_range(hit_range_);
        as->set_flags(RenderingDevice::ACCELERATION_STRUCTURE_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT |
                      RenderingDevice::ACCELERATION_STRUCTURE_INSTANCE_FORCE_OPAQUE_BIT);
        as->set_blas(it->second.blas);
        tlas_instances.push_back(as);
        rt::InstanceMaterial m = inst.material;
        m.first_triangle = it->second.first_triangle;
        materials.push_back(m);
    }
    instance_count_ = static_cast<int>(materials.size());
    if (materials.empty()) return;
    if (rd->tlas_build(tlas_, tlas_instances) != OK) {
        UtilityFunctions::push_error("RtReflections: cannot build the scene's acceleration structure; turned off");
        failed_ = true;
        free_all();
        return;
    }
    const std::vector<float> packed = rt::pack_instances(materials);
    const PackedByteArray instance_bytes = bytes_of(packed.data(), packed.size());
    rd->buffer_update(instance_buffer_, 0, instance_bytes.size(), instance_bytes);

    if (rd->get_captured_timestamps_count() > 0) rd->capture_timestamp("RT Reflections");

    const Vector2i internal = buffers->get_internal_size();
    if (internal.x <= 0 || internal.y <= 0) return;
    if (!buffers->has_texture(context(), out_name())) {
        buffers->create_texture(context(), out_name(), RenderingDevice::DATA_FORMAT_R16G16B16A16_SFLOAT,
                                RenderingDevice::TEXTURE_USAGE_STORAGE_BIT | RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT,
                                RenderingDevice::TEXTURE_SAMPLES_1, internal, buffers->get_view_count(), 1, true, false);
    }

    const Transform3D camera = scene->get_cam_transform();
    for (uint32_t view = 0; view < buffers->get_view_count(); ++view) {
        // Already maps to the reversed 0..1 depth buffer (as BackdropBlur uses it).
        const Projection projection = scene->get_view_projection(view);
        const Transform3D eye = camera * Transform3D(Basis(), scene->get_view_eye_offset(view));
        Frame data{};
        store(projection.inverse(), data.clip_to_view);
        store(Projection(eye), data.view_to_world);
        store(projection * Projection(eye.affine_inverse()), data.world_to_clip);
        data.params[0] = strength_;
        data.params[1] = kDepthTolerance;
        data.params[2] = rt::kOffscreenAmbient;
        data.params[3] = kMaxRayDistance;
        data.size[0] = static_cast<float>(internal.x);
        data.size[1] = static_cast<float>(internal.y);
        data.size[2] = 1.0f / static_cast<float>(internal.x);
        data.size[3] = 1.0f / static_cast<float>(internal.y);
        data.debug[0] = options().debug ? 1.0f : 0.0f;
        const PackedByteArray frame_bytes = bytes_of(&data, 1);
        rd->buffer_update(frame_buffer_, 0, frame_bytes.size(), frame_bytes);

        const RID color = buffers->get_color_layer(view);
        const RID depth = buffers->get_depth_layer(view);
        const RID out = buffers->get_texture_slice(context(), out_name(), view, 0, 1, 1);

        TypedArray<RDUniform> trace_set;
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_IMAGE, 0, color));
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, depth, nearest_sampler_));
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_ACCELERATION_STRUCTURE, 2, tlas_));
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER, 3, instance_buffer_));
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER, 4, normal_buffer_));
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER, 5, frame_buffer_));
        trace_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_IMAGE, 6, out));
        TypedArray<RDUniform> resolve_set;
        resolve_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_IMAGE, 0, color));
        resolve_set.push_back(uniform(RenderingDevice::UNIFORM_TYPE_IMAGE, 1, out));

        const int64_t rays = rd->raytracing_list_begin();
        rd->raytracing_list_bind_raytracing_pipeline(rays, trace_pipeline_);
        rd->raytracing_list_bind_uniform_set(rays, UniformSetCacheRD::get_cache(trace_shader_, 0, trace_set), 0);
        rd->raytracing_list_trace_rays(rays, 0, hit_table_, internal.x, internal.y, 1);
        rd->raytracing_list_end();

        const int64_t list = rd->compute_list_begin();
        rd->compute_list_bind_compute_pipeline(list, resolve_pipeline_);
        rd->compute_list_bind_uniform_set(list, UniformSetCacheRD::get_cache(resolve_shader_, 0, resolve_set), 0);
        const int32_t size[4] = {internal.x, internal.y, 0, 0};
        const PackedByteArray push = bytes_of(size, 4);
        rd->compute_list_set_push_constant(list, push, push.size());
        rd->compute_list_dispatch(list, groups(internal.x), groups(internal.y), 1);
        rd->compute_list_end();
    }
    ++traced_frames_;
    Shared& s = shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.last_traced_frame = frame;
}

void RtReflections::free_all() {
    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs ? rs->get_rendering_device() : nullptr;
    if (!rd) return;
    // The TLAS depends on every BLAS, which depend on their buffers: free in that order.
    if (tlas_.is_valid()) rd->free_rid(tlas_);
    tlas_ = RID();
    // The hit table belongs to the pipeline, which the shader frees.
    if (hit_table_.is_valid()) rd->free_rid(hit_table_);
    hit_table_ = RID();
    hit_range_ = 0;
    for (auto& [key, mesh] : meshes_) {
        for (RID* rid : {&mesh.blas, &mesh.indices, &mesh.vertices}) {
            if (rid->is_valid()) rd->free_rid(*rid);
        }
    }
    meshes_.clear();
    normals_.clear();
    // Freeing a shader frees the pipelines made from it.
    for (RID* rid : {&trace_shader_, &resolve_shader_, &nearest_sampler_, &instance_buffer_, &normal_buffer_,
                     &frame_buffer_}) {
        if (rid->is_valid()) rd->free_rid(*rid);
        *rid = RID();
    }
    trace_pipeline_ = RID();
    resolve_pipeline_ = RID();
}

void RtReflections::_notification(int p_what) {
    if (p_what == NOTIFICATION_PREDELETE) free_all();
}

void RtReflections::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_strength", "strength"), &RtReflections::set_strength);
    ClassDB::bind_method(D_METHOD("get_strength"), &RtReflections::get_strength);
    ClassDB::bind_method(D_METHOD("get_instance_count"), &RtReflections::get_instance_count);
    ClassDB::bind_method(D_METHOD("get_blas_count"), &RtReflections::get_blas_count);
    ClassDB::bind_static_method("RtReflections", D_METHOD("is_supported"), &RtReflections::is_supported);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "strength", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_strength",
                 "get_strength");
}

}  // namespace montezuma::godot_binding
