// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// Quality tiers from docs/reference/visual-target.md: which renderer each
// tier needs, which features it turns on and the render settings it uses.
// Godot-free so the tier table and the start-up choice are unit tested;
// QualityTiers (quality_tiers.hpp) applies a profile to the engine.
namespace montezuma::godot_binding::quality {

enum class Tier { DesktopHigh, DesktopStandard, Mobile, Web };

inline constexpr std::array<Tier, 4> kTiers{Tier::DesktopHigh, Tier::DesktopStandard, Tier::Mobile, Tier::Web};

// Ids as written in the visual target and the settings file.
std::string_view tier_id(Tier tier);
std::optional<Tier> tier_from_id(std::string_view id);

// Godot rendering method names, as RenderingServer::get_current_rendering_method returns them.
inline constexpr std::string_view kForwardPlus = "forward_plus";
inline constexpr std::string_view kMobile = "mobile";
inline constexpr std::string_view kCompatibility = "gl_compatibility";

// The rendering method a tier needs. The method is fixed for the whole
// process, so a tier is only available when it matches the running one.
std::string_view tier_renderer(Tier tier);
bool tier_runs_on(Tier tier, std::string_view rendering_method);

// What the running GPU reports, mirroring RenderingServer's video adapter queries.
enum class AdapterType { Other, Integrated, Discrete, Virtual, Cpu };

struct Adapter {
    std::string rendering_method;  // e.g. "forward_plus"
    AdapterType type = AdapterType::Other;
    std::string name;    // e.g. "NVIDIA GeForce RTX 3060", "Apple M1 Pro"
    std::string vendor;  // e.g. "NVIDIA", "Apple"
};

// The tier picked at start. Compatibility runs the web tier and Mobile the
// mobile tier wherever they run. On Forward+, discrete GPUs and Apple
// M-series Pro, Max and Ultra chips get desktop-high; integrated, virtual,
// software and unknown adapters get desktop-standard.
Tier detect_tier(const Adapter& adapter);

// The player's choice ("auto" or a tier id) resolved against the running
// method: an unknown id or a tier the method cannot run falls back to the
// detected tier.
Tier resolve_tier(std::string_view choice, const Adapter& adapter);

// Features a tier switches, one id per row of the visual target that the
// quality system turns on or off. Node groups use the same ids.
enum class Feature {
    Ssao,
    Ssil,
    Ssr,
    Sdfgi,
    VoxelGi,
    VolumetricFog,
    FogFireLight,
    AutoExposure,
    DepthOfField,
    Glow,
    SubsurfaceScattering,
    ParallaxOcclusion,
    GemRefraction,
    WaterRefraction,
    HeatHaze,
    Decals,
    AreaLight,
    ParticleTrails,
    FireFlipbook8x8,
    Count
};

inline constexpr std::size_t kFeatureCount = static_cast<std::size_t>(Feature::Count);

std::string_view feature_id(Feature feature);
std::optional<Feature> feature_from_id(std::string_view id);

// Upscaling for the 3D view. TemporalUpscale is MetalFX temporal on Metal
// and FSR 2.2 elsewhere; SpatialUpscale is FSR 1. Both need Forward+.
enum class Scaling { Bilinear, SpatialUpscale, TemporalUpscale };

enum class SoftShadows { Hard, VeryLow, Low, Medium, High };

enum class SubsurfaceQuality { Disabled, Low, Medium, High };

struct Profile {
    Tier tier = Tier::DesktopStandard;
    std::array<bool, kFeatureCount> features{};

    // Viewport.
    Scaling scaling = Scaling::Bilinear;
    float render_scale = 1.0f;
    bool taa = false;
    int msaa = 0;  // 0, 2, 4 or 8 samples
    bool smaa = false;
    int anisotropy = 4;  // 1 (off), 2, 4, 8 or 16
    float mesh_lod_threshold = 1.0f;  // pixels; higher picks coarser LODs sooner

    // Shadows.
    int sun_shadow_size = 2048;
    // Most cascades a directional light's shadow may use (1 is orthogonal,
    // 2 or 4 parallel splits); each cascade draws every caster again.
    int sun_shadow_splits = 4;
    SoftShadows soft_shadows = SoftShadows::Low;
    int positional_shadow_atlas = 2048;  // 0 turns positional shadows off
    int shadow_casting_lights = 2;       // nearest lights in the quality_shadow_light group that keep shadows

    // Environment and materials.
    bool ssr_half_size = false;
    int volumetric_fog_size = 64;  // froxel grid width and height
    int volumetric_fog_depth = 64;
    SubsurfaceQuality subsurface = SubsurfaceQuality::Disabled;

    // Draw the board's pieces and their markers as one multimesh per mesh
    // and material instead of one draw each (the draw-call budget).
    bool batch_pieces = false;

    // Texture variant folder next to the base set ("hi", "lo", or "" for base only).
    std::string_view texture_variant;

    bool has(Feature feature) const { return features[static_cast<std::size_t>(feature)]; }
};

// The profile for a tier, straight from the visual target's tables.
const Profile& profile(Tier tier);

// desktop-high renders 3D at most this many lines high: above it the temporal
// upscaler takes over at kHighInternalHeight / output height, never below
// kHighMinScale; at or below it, native with TAA. Native 1440p with TAA
// measured p95 25.1 ms on a Mac mini M4 before any WP-47 lighting
// (docs/reference/benchmark-results.md); the cost estimate behind 720 lines
// is in docs/explanation/about-lighting.md#costs.
inline constexpr int kHighInternalHeight = 720;
inline constexpr float kHighMinScale = 0.5f;

// Viewport scaling for a profile at an output height; only desktop-high
// changes with the output size.
struct ViewportPlan {
    Scaling scaling;
    float render_scale;
    bool taa;
};
ViewportPlan viewport_plan(const Profile& profile, int output_height);

// The tier variant of an asset path: "dir/file" becomes "dir/<variant>/file".
// Returns the path unchanged when the profile has no variant folder.
std::string variant_path(std::string_view path, std::string_view variant);

// The base asset a variant path stands for: "dir/<variant>/file" becomes
// "dir/file" when <variant> is any tier's variant folder. Other paths come
// back unchanged, so base_path(variant_path(p, v)) == p for every tier.
std::string base_path(std::string_view path);

}  // namespace montezuma::godot_binding::quality
