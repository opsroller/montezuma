// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "quality.hpp"

#include <initializer_list>

namespace montezuma::godot_binding::quality {

namespace {

constexpr std::array<std::string_view, kFeatureCount> kFeatureIds{
    "ssao",           "ssil",          "ssr",           "sdfgi",           "voxel_gi",
    "volumetric_fog", "fog_fire_light", "auto_exposure", "depth_of_field",  "glow",
    "subsurface_scattering", "parallax_occlusion", "gem_refraction", "water_refraction", "heat_haze",
    "decals",         "area_light",    "particle_trails", "fire_flipbook_8x8",
};

std::array<bool, kFeatureCount> features(std::initializer_list<Feature> on) {
    std::array<bool, kFeatureCount> out{};
    for (Feature f : on) out[static_cast<std::size_t>(f)] = true;
    return out;
}

bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// Apple's own M-series names: "Apple M1 Pro", "Apple M3 Max", "Apple M2 Ultra".
bool is_apple_pro_class(const Adapter& a) {
    const bool apple = contains(a.vendor, "Apple") || contains(a.name, "Apple");
    return apple && (contains(a.name, " Pro") || contains(a.name, " Max") || contains(a.name, " Ultra"));
}

Profile make_desktop_high() {
    Profile p;
    p.tier = Tier::DesktopHigh;
    p.features = features({Feature::Ssao, Feature::Ssil, Feature::Ssr, Feature::Sdfgi, Feature::VoxelGi,
                           Feature::VolumetricFog, Feature::FogFireLight, Feature::AutoExposure,
                           Feature::DepthOfField, Feature::Glow, Feature::SubsurfaceScattering,
                           Feature::ParallaxOcclusion, Feature::GemRefraction, Feature::WaterRefraction,
                           Feature::HeatHaze, Feature::Decals, Feature::AreaLight, Feature::ParticleTrails,
                           Feature::FireFlipbook8x8});
    p.scaling = Scaling::Bilinear;
    p.render_scale = 1.0f;
    p.taa = true;
    p.anisotropy = 16;
    p.sun_shadow_size = 4096;
    p.soft_shadows = SoftShadows::High;
    p.positional_shadow_atlas = 4096;
    p.shadow_casting_lights = 8;
    p.volumetric_fog_size = 64;
    p.volumetric_fog_depth = 64;
    p.subsurface = SubsurfaceQuality::High;
    p.texture_variant = "hi";
    return p;
}

Profile make_desktop_standard() {
    Profile p;
    p.tier = Tier::DesktopStandard;
    p.features = features({Feature::Ssao, Feature::Ssil, Feature::Ssr, Feature::VolumetricFog,
                           Feature::AutoExposure, Feature::DepthOfField, Feature::Glow,
                           Feature::SubsurfaceScattering, Feature::ParallaxOcclusion, Feature::GemRefraction,
                           Feature::WaterRefraction, Feature::HeatHaze, Feature::Decals,
                           Feature::ParticleTrails, Feature::FireFlipbook8x8});
    p.scaling = Scaling::TemporalUpscale;
    p.render_scale = 0.77f;
    p.anisotropy = 8;
    p.sun_shadow_size = 2048;
    p.soft_shadows = SoftShadows::Low;
    p.positional_shadow_atlas = 2048;
    p.shadow_casting_lights = 2;
    p.ssr_half_size = true;
    p.volumetric_fog_size = 32;
    p.volumetric_fog_depth = 32;
    p.subsurface = SubsurfaceQuality::Medium;
    p.texture_variant = "hi";
    return p;
}

Profile make_mobile() {
    Profile p;
    p.tier = Tier::Mobile;
    p.features = features({Feature::Glow, Feature::Decals, Feature::ParticleTrails});
    // FSR 1 runs only on Forward+ in Godot 4.7, so the Mobile renderer
    // scales bilinearly; the target's 0.75 to 1.0 range starts at the top.
    p.scaling = Scaling::Bilinear;
    p.render_scale = 1.0f;
    p.msaa = 2;  // resolved on-tile, cheaper than a post pass on tile-based GPUs
    p.anisotropy = 4;
    p.mesh_lod_threshold = 2.0f;
    p.sun_shadow_size = 2048;
    p.soft_shadows = SoftShadows::VeryLow;
    p.positional_shadow_atlas = 0;
    p.shadow_casting_lights = 0;
    p.batch_pieces = true;
    p.texture_variant = "";
    return p;
}

Profile make_web() {
    Profile p;
    p.tier = Tier::Web;
    p.features = features({Feature::Glow});
    p.scaling = Scaling::Bilinear;
    p.render_scale = 1.0f;
    p.msaa = 2;
    p.anisotropy = 2;
    p.mesh_lod_threshold = 2.0f;
    p.sun_shadow_size = 1024;
    p.sun_shadow_splits = 1;
    p.soft_shadows = SoftShadows::Hard;
    p.positional_shadow_atlas = 0;
    p.shadow_casting_lights = 0;
    p.batch_pieces = true;
    p.texture_variant = "lo";
    return p;
}

}  // namespace

std::string_view tier_id(Tier tier) {
    switch (tier) {
        case Tier::DesktopHigh: return "desktop-high";
        case Tier::DesktopStandard: return "desktop-standard";
        case Tier::Mobile: return "mobile";
        case Tier::Web: return "web";
    }
    return "desktop-standard";
}

std::optional<Tier> tier_from_id(std::string_view id) {
    for (Tier t : kTiers)
        if (tier_id(t) == id) return t;
    return std::nullopt;
}

std::string_view tier_renderer(Tier tier) {
    switch (tier) {
        case Tier::DesktopHigh:
        case Tier::DesktopStandard: return kForwardPlus;
        case Tier::Mobile: return kMobile;
        case Tier::Web: return kCompatibility;
    }
    return kForwardPlus;
}

bool tier_runs_on(Tier tier, std::string_view rendering_method) { return tier_renderer(tier) == rendering_method; }

Tier detect_tier(const Adapter& adapter) {
    if (adapter.rendering_method == kCompatibility) return Tier::Web;
    if (adapter.rendering_method == kMobile) return Tier::Mobile;
    if (adapter.type == AdapterType::Discrete || is_apple_pro_class(adapter)) return Tier::DesktopHigh;
    return Tier::DesktopStandard;
}

Tier resolve_tier(std::string_view choice, const Adapter& adapter) {
    const std::optional<Tier> picked = tier_from_id(choice);
    if (picked && tier_runs_on(*picked, adapter.rendering_method)) return *picked;
    return detect_tier(adapter);
}

std::string_view feature_id(Feature feature) {
    const auto i = static_cast<std::size_t>(feature);
    return i < kFeatureCount ? kFeatureIds[i] : std::string_view{};
}

std::optional<Feature> feature_from_id(std::string_view id) {
    for (std::size_t i = 0; i < kFeatureCount; ++i)
        if (kFeatureIds[i] == id) return static_cast<Feature>(i);
    return std::nullopt;
}

const Profile& profile(Tier tier) {
    static const std::array<Profile, 4> profiles{make_desktop_high(), make_desktop_standard(), make_mobile(),
                                                 make_web()};
    return profiles[static_cast<std::size_t>(tier)];
}

ViewportPlan viewport_plan(const Profile& p, int output_height) {
    if (p.tier == Tier::DesktopHigh && output_height > kHighUpscaleAboveHeight)
        return {Scaling::TemporalUpscale, 0.67f, false};
    return {p.scaling, p.render_scale, p.taa};
}

std::string variant_path(std::string_view path, std::string_view variant) {
    if (variant.empty()) return std::string(path);
    const std::size_t slash = path.rfind('/');
    if (slash == std::string_view::npos) return std::string(variant) + "/" + std::string(path);
    std::string out(path.substr(0, slash + 1));
    out += variant;
    out += '/';
    out += path.substr(slash + 1);
    return out;
}

std::string base_path(std::string_view path) {
    const std::size_t slash = path.rfind('/');
    if (slash == std::string_view::npos) return std::string(path);
    const std::size_t prev = slash == 0 ? std::string_view::npos : path.rfind('/', slash - 1);
    const std::size_t start = prev == std::string_view::npos ? 0 : prev + 1;
    const std::string_view folder = path.substr(start, slash - start);
    for (Tier t : kTiers) {
        const std::string_view variant = profile(t).texture_variant;
        if (!variant.empty() && folder == variant) {
            return std::string(path.substr(0, start)) + std::string(path.substr(slash + 1));
        }
    }
    return std::string(path);
}

}  // namespace montezuma::godot_binding::quality
