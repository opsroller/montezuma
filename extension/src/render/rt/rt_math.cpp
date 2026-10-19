// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "rt_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace montezuma::godot_binding::rt {

namespace {

constexpr std::string_view kStrength = "--rt-strength=";

struct V3 {
    float x, y, z;
};

V3 at(const std::vector<float>& v, std::uint32_t i) {
    if (static_cast<std::size_t>(i) * 3 + 2 >= v.size()) return {0.0f, 0.0f, 0.0f};
    return {v[i * 3], v[i * 3 + 1], v[i * 3 + 2]};
}

float length(V3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

V3 face_normal(V3 a, V3 b, V3 c) {
    const V3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
    const V3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
    // Godot's front faces wind clockwise, so the outward normal is e2 x e1.
    V3 n{e2.y * e1.z - e2.z * e1.y, e2.z * e1.x - e2.x * e1.z, e2.x * e1.y - e2.y * e1.x};
    const float len = length(n);
    if (len <= 1e-12f) return {0.0f, 1.0f, 0.0f};
    return {n.x / len, n.y / len, n.z / len};
}

}  // namespace

Options parse_options(const std::vector<std::string>& args) {
    Options o;
    for (const std::string& arg : args) {
        if (arg == "--rt-reflections") o.enabled = true;
        if (arg == "--rt-selftest") o.enabled = o.self_test = true;
        if (arg == "--rt-debug") o.enabled = o.debug = true;
        if (arg.rfind(kStrength, 0) == 0 && arg.size() > kStrength.size()) {
            const std::string value = arg.substr(kStrength.size());
            char* end = nullptr;
            // strtod saturates to +-HUGE_VAL out of range, which then clamps.
            const double s = std::strtod(value.c_str(), &end);
            if (end && *end == '\0' && !std::isnan(s))
                o.strength = static_cast<float>(std::clamp(s, 0.0, static_cast<double>(kMaxStrength)));
        }
    }
    return o;
}

bool should_attach(bool built, const Options& options, std::string_view rendering_method, bool ray_tracing) {
    return built && options.enabled && rendering_method == "forward_plus" && ray_tracing;
}

int self_test_exit_now(const Options& options, bool attached) {
    if (!options.self_test) return 0;
    return attached ? -1 : 1;
}

bool may_trace(bool pipelines_ready, bool failed) { return !failed && pipelines_ready; }

std::vector<std::uint64_t> meshes_to_evict(const std::unordered_map<std::uint64_t, std::uint64_t>& last_seen_frame,
                                           std::uint64_t frame, std::uint64_t keep_frames) {
    std::vector<std::uint64_t> out;
    for (const auto& [mesh, seen] : last_seen_frame) {
        if (frame > seen && frame - seen > keep_frames) out.push_back(mesh);
    }
    return out;
}

float dielectric_f0(float ior) {
    if (!(ior > 0.0f)) ior = kDefaultIor;
    const float r = (ior - 1.0f) / (ior + 1.0f);
    return r * r;
}

Rgb material_f0(Rgb albedo, float metalness, float ior) {
    const float m = std::clamp(metalness, 0.0f, 1.0f);
    const float d = dielectric_f0(ior);
    return {d + (albedo.r - d) * m, d + (albedo.g - d) * m, d + (albedo.b - d) * m};
}

Rgb schlick(Rgb f0, float cos_theta) {
    const float c = std::clamp(cos_theta, 0.0f, 1.0f);
    const float k = std::pow(1.0f - c, 5.0f);
    return {f0.r + (1.0f - f0.r) * k, f0.g + (1.0f - f0.g) * k, f0.b + (1.0f - f0.b) * k};
}

float roughness_weight(float roughness, float cutoff) {
    if (!(cutoff > 0.0f) || roughness >= cutoff) return 0.0f;
    const float t = 1.0f - std::max(roughness, 0.0f) / cutoff;
    return t * t;
}

std::vector<float> pack_instances(const std::vector<InstanceMaterial>& instances) {
    std::vector<float> out;
    out.reserve(instances.size() * kFloatsPerInstance);
    for (const InstanceMaterial& i : instances) {
        out.insert(out.end(), {i.albedo.r, i.albedo.g, i.albedo.b, static_cast<float>(i.first_triangle), i.f0.r,
                               i.f0.g, i.f0.b, i.weight});
    }
    return out;
}

std::vector<float> pack_triangle_normals(const std::vector<float>& positions, const std::vector<float>& normals,
                                         const std::vector<std::uint32_t>& indices) {
    const std::size_t triangles = indices.size() / 3;
    std::vector<float> out;
    out.reserve(triangles * kFloatsPerTriangle);
    for (std::size_t t = 0; t < triangles; ++t) {
        const std::uint32_t* tri = &indices[t * 3];
        const V3 face = face_normal(at(positions, tri[0]), at(positions, tri[1]), at(positions, tri[2]));
        for (int k = 0; k < 3; ++k) {
            V3 n = at(normals, tri[k]);
            const float len = length(n);
            n = len > 1e-6f ? V3{n.x / len, n.y / len, n.z / len} : face;
            out.insert(out.end(), {n.x, n.y, n.z, 0.0f});
        }
    }
    return out;
}

}  // namespace montezuma::godot_binding::rt
