// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Quality tier table and start-up choice tests. No Godot, no framework; run via ctest.
#include <cstdio>
#include <string>

#include "quality.hpp"

using namespace montezuma::godot_binding::quality;

namespace {

int checks = 0;
int failures = 0;

void check(bool cond, const std::string& label) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    }
}

Adapter forward(AdapterType type, std::string name, std::string vendor) {
    return Adapter{std::string(kForwardPlus), type, std::move(name), std::move(vendor)};
}

void test_ids_round_trip() {
    for (Tier t : kTiers) check(tier_from_id(tier_id(t)) == t, "tier id round trip " + std::string(tier_id(t)));
    check(!tier_from_id("ultra"), "unknown tier id");
    check(!tier_from_id("auto"), "auto is not a tier");
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        const auto f = static_cast<Feature>(i);
        check(!feature_id(f).empty(), "feature has an id");
        check(feature_from_id(feature_id(f)) == f, "feature id round trip " + std::string(feature_id(f)));
    }
    check(!feature_from_id("ray_tracing"), "unknown feature id");
}

void test_detect_tier() {
    check(detect_tier(forward(AdapterType::Discrete, "NVIDIA GeForce RTX 3060", "NVIDIA")) == Tier::DesktopHigh,
          "discrete GPU is desktop-high");
    check(detect_tier(forward(AdapterType::Integrated, "Intel(R) Iris(R) Xe Graphics", "Intel")) ==
              Tier::DesktopStandard,
          "Iris Xe is desktop-standard");
    check(detect_tier(forward(AdapterType::Integrated, "AMD Custom GPU 0405", "AMD")) == Tier::DesktopStandard,
          "Steam Deck is desktop-standard");
    check(detect_tier(forward(AdapterType::Integrated, "Apple M1", "Apple")) == Tier::DesktopStandard,
          "Apple M1 is desktop-standard");
    check(detect_tier(forward(AdapterType::Integrated, "Apple M1 Pro", "Apple")) == Tier::DesktopHigh,
          "Apple M1 Pro is desktop-high");
    check(detect_tier(forward(AdapterType::Integrated, "Apple M3 Max", "Apple")) == Tier::DesktopHigh,
          "Apple M3 Max is desktop-high");
    check(detect_tier(forward(AdapterType::Cpu, "llvmpipe (LLVM 17.0.6, 256 bits)", "Mesa")) ==
              Tier::DesktopStandard,
          "software renderer is desktop-standard");
    check(detect_tier(forward(AdapterType::Other, "", "")) == Tier::DesktopStandard, "unknown is desktop-standard");
    check(detect_tier(Adapter{std::string(kMobile), AdapterType::Discrete, "RTX", "NVIDIA"}) == Tier::Mobile,
          "Mobile renderer is mobile on any GPU");
    check(detect_tier(Adapter{std::string(kCompatibility), AdapterType::Discrete, "RTX", "NVIDIA"}) == Tier::Web,
          "Compatibility renderer is web on any GPU");
}

void test_resolve_tier() {
    const Adapter rtx = forward(AdapterType::Discrete, "NVIDIA GeForce RTX 3060", "NVIDIA");
    check(resolve_tier("auto", rtx) == Tier::DesktopHigh, "auto picks detected");
    check(resolve_tier("desktop-standard", rtx) == Tier::DesktopStandard, "player can lower the desktop tier");
    const Adapter xe = forward(AdapterType::Integrated, "Intel(R) Iris(R) Xe Graphics", "Intel");
    check(resolve_tier("desktop-high", xe) == Tier::DesktopHigh, "player can raise the desktop tier");
    check(resolve_tier("mobile", rtx) == Tier::DesktopHigh, "mobile tier needs the Mobile renderer");
    check(resolve_tier("web", rtx) == Tier::DesktopHigh, "web tier needs Compatibility");
    check(resolve_tier("bogus", xe) == Tier::DesktopStandard, "unknown choice falls back to detected");
    const Adapter phone{std::string(kMobile), AdapterType::Integrated, "Adreno (TM) 642L", "Qualcomm"};
    check(resolve_tier("desktop-high", phone) == Tier::Mobile, "phones stay mobile");
}

void test_profiles_match_visual_target() {
    for (Tier t : kTiers) {
        check(profile(t).tier == t, "profile carries its tier");
        check(profile(t).has(Feature::Glow), "glow on every tier");
        check(profile(t).render_scale > 0.0f && profile(t).render_scale <= 1.0f, "render scale in range");
    }
    const Profile& high = profile(Tier::DesktopHigh);
    const Profile& standard = profile(Tier::DesktopStandard);
    const Profile& mobile = profile(Tier::Mobile);
    const Profile& web = profile(Tier::Web);

    // Every feature on a lower tier is also on every higher tier.
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        const auto f = static_cast<Feature>(i);
        if (web.has(f)) check(mobile.has(f), "web feature also on mobile: " + std::string(feature_id(f)));
        if (mobile.has(f)) check(standard.has(f), "mobile feature also on standard: " + std::string(feature_id(f)));
        if (standard.has(f)) check(high.has(f), "standard feature also on high: " + std::string(feature_id(f)));
    }

    check(high.has(Feature::VoxelGi) && !standard.has(Feature::VoxelGi), "VoxelGI on desktop-high only");
    check(high.has(Feature::AreaLight) && !standard.has(Feature::AreaLight), "area light on desktop-high only");
    check(high.has(Feature::FogFireLight) && !standard.has(Feature::FogFireLight), "fire lights fog on high only");
    for (Feature f : {Feature::Ssao, Feature::Ssil, Feature::Ssr, Feature::VolumetricFog, Feature::DepthOfField,
                      Feature::HeatHaze, Feature::SubsurfaceScattering, Feature::GemRefraction,
                      Feature::WaterRefraction, Feature::AutoExposure, Feature::FireFlipbook8x8}) {
        check(standard.has(f), "desktop-standard has " + std::string(feature_id(f)));
        check(!mobile.has(f), "mobile lacks " + std::string(feature_id(f)));
    }
    check(mobile.has(Feature::Decals) && !web.has(Feature::Decals), "decals down to mobile, not web");
    check(mobile.has(Feature::ParticleTrails) && !web.has(Feature::ParticleTrails), "trails down to mobile");

    check(high.sun_shadow_size == 4096 && standard.sun_shadow_size == 2048 && mobile.sun_shadow_size == 2048 &&
              web.sun_shadow_size == 1024,
          "sun shadow sizes 4096/2048/2048/1024");
    check(high.shadow_casting_lights == 8 && standard.shadow_casting_lights == 2 &&
              mobile.shadow_casting_lights == 0 && web.shadow_casting_lights == 0,
          "shadow-casting light budget 8/2/0/0");
    check(standard.ssr_half_size && !high.ssr_half_size, "SSR half resolution on standard only");
    check(standard.scaling == Scaling::TemporalUpscale && standard.render_scale == 0.77f, "standard at 0.77");
    check(web.scaling == Scaling::Bilinear && web.msaa == 2, "web 2x MSAA, no upscaler");
    check(mobile.msaa == 2 || mobile.smaa, "mobile SMAA or 2x MSAA");
    check(high.texture_variant == "hi" && standard.texture_variant == "hi" && mobile.texture_variant.empty() &&
              web.texture_variant == "lo",
          "texture variants hi/hi/base/lo");
}

void test_renderer_per_tier() {
    check(tier_runs_on(Tier::DesktopHigh, kForwardPlus) && tier_runs_on(Tier::DesktopStandard, kForwardPlus),
          "desktop tiers on Forward+");
    check(tier_runs_on(Tier::Mobile, kMobile) && !tier_runs_on(Tier::Mobile, kForwardPlus), "mobile on Mobile");
    check(tier_runs_on(Tier::Web, kCompatibility) && !tier_runs_on(Tier::Web, kMobile), "web on Compatibility");
}

void test_viewport_plan() {
    const Profile& high = profile(Tier::DesktopHigh);
    const ViewportPlan native = viewport_plan(high, 1440);
    check(native.scaling == Scaling::Bilinear && native.render_scale == 1.0f && native.taa, "1440p native TAA");
    const ViewportPlan uhd = viewport_plan(high, 2160);
    check(uhd.scaling == Scaling::TemporalUpscale && uhd.render_scale == 0.67f && !uhd.taa,
          "4K temporal upscale at 0.67 without TAA");
    const ViewportPlan std_plan = viewport_plan(profile(Tier::DesktopStandard), 2160);
    check(std_plan.render_scale == 0.77f, "standard ignores output size");
}

void test_variant_path() {
    check(variant_path("res://assets/textures/temple/stone_albedo.png", "hi") ==
              "res://assets/textures/temple/hi/stone_albedo.png",
          "hi variant folder");
    check(variant_path("res://a/b.png", "") == "res://a/b.png", "no variant leaves the path");
    check(variant_path("b.png", "lo") == "lo/b.png", "bare file name");
}

}  // namespace

int main() {
    test_ids_round_trip();
    test_detect_tier();
    test_resolve_tier();
    test_profiles_match_visual_target();
    test_renderer_per_tier();
    test_viewport_plan();
    test_variant_path();
    std::printf("quality_tests: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
