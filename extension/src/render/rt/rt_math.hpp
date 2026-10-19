// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The Godot-free half of the ray-traced reflections spike (WP-54): run
// options, when the effect may attach, the reflectance of a piece's
// material and how instances and triangles are packed for the shader.
// RtReflections (rt_reflections.hpp) does the engine side.
// Reference: docs/reference/ray-traced-reflections.md.
namespace montezuma::godot_binding::rt {

// Named defaults (docs/reference/ray-traced-reflections.md, "Defaults").
// Mirror rays are one sample, so they fade out as the surface roughens
// and stop at this roughness, where the engine's probes look the same.
inline constexpr float kRoughnessCutoff = 0.5f;
// Index of refraction for a dielectric piece whose material names none.
inline constexpr float kDefaultIor = 1.5f;
// How much of the traced reflection replaces the engine's, 0..kMaxStrength.
inline constexpr float kDefaultStrength = 1.0f;
inline constexpr float kMaxStrength = 2.0f;
// Light on a hit the screen cannot show (behind another piece or off
// screen): the hit piece's albedo times this.
inline constexpr float kOffscreenAmbient = 0.25f;
// Most piece instances in the scene; more are left out with a warning.
inline constexpr std::uint32_t kMaxInstances = 4096;
// A mesh not gathered for this many frames is freed (BLAS, buffers, normals).
inline constexpr std::uint64_t kEvictFrames = 30;

struct Options {
    bool enabled = false;   // --rt-reflections (or --rt-selftest)
    bool self_test = false; // --rt-selftest
    bool debug = false;     // --rt-debug: paint what it traces instead of shading
    float strength = kDefaultStrength;  // --rt-strength=<0..2>
};

// Reads the user arguments (after `--`). Unknown arguments are ignored; a
// strength that does not parse keeps the default, one out of range clamps.
Options parse_options(const std::vector<std::string>& args);

// The effect attaches only when the build has it, the run asked for it,
// the renderer is Forward+ (the only one with the normal and depth buffers
// it reads) and the GPU has ray-tracing pipelines. (Godot 4.7.2 never
// turns on Vulkan ray queries, so pipelines are the only way in.)
bool should_attach(bool built, const Options& options, std::string_view rendering_method, bool ray_tracing);

// What --rt-selftest does once the attach decision is known: -1 keep
// running and check after its frames; 1 quit now with failure (the effect
// cannot run here); 0 no self-test was asked for.
int self_test_exit_now(const Options& options, bool attached);

// Whether a frame may trace: never again after any step failed, even
// where the pipelines it built still exist.
bool may_trace(bool pipelines_ready, bool failed);

// Meshes last gathered more than `keep_frames` frames before `frame`.
std::vector<std::uint64_t> meshes_to_evict(const std::unordered_map<std::uint64_t, std::uint64_t>& last_seen_frame,
                                           std::uint64_t frame, std::uint64_t keep_frames = kEvictFrames);

struct Rgb {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

// Reflectance at normal incidence of a dielectric with index of refraction `ior`.
float dielectric_f0(float ior);

// F0 of a material: the dielectric value for its ior, blended toward the
// albedo as it becomes metal (metalness 0..1, clamped).
Rgb material_f0(Rgb albedo, float metalness, float ior);

// Schlick's Fresnel for F0 at the cosine between normal and view.
Rgb schlick(Rgb f0, float cos_theta);

// How much of a receiver's reflection the traced ray replaces at this
// roughness: 1 at a mirror, falling to 0 at the cutoff and above.
float roughness_weight(float roughness, float cutoff = kRoughnessCutoff);

// One instance as the shader reads it: two vec4s.
//   [0] albedo.rgb, first triangle in the normals buffer (as a float)
//   [1] f0.rgb, roughness_weight of its material (0: it only blocks and is
//       seen in others, never traced from)
struct InstanceMaterial {
    Rgb albedo;
    Rgb f0;
    std::uint32_t first_triangle = 0;
    float weight = 1.0f;
};
inline constexpr std::size_t kFloatsPerInstance = 8;
std::vector<float> pack_instances(const std::vector<InstanceMaterial>& instances);

// Per-vertex normals of an indexed triangle mesh, packed three vec4s a
// triangle (xyz normal, w unused) in index order, so the shader can
// interpolate a hit's normal from its primitive index and barycentrics.
// `normals` has one xyz per vertex; a degenerate or missing normal is
// replaced by the triangle's face normal from `positions`.
std::vector<float> pack_triangle_normals(const std::vector<float>& positions, const std::vector<float>& normals,
                                         const std::vector<std::uint32_t>& indices);
inline constexpr std::size_t kFloatsPerTriangle = 12;

}  // namespace montezuma::godot_binding::rt
