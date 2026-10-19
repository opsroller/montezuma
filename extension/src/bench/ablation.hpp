// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace montezuma::godot_binding::bench {

// Render items `--bench-off=` can turn off for one run, to measure what each
// costs where the driver gives no GPU time (docs/reference/benchmark.md#ablation).
enum class OffItem : std::uint8_t {
    VoxelGi,
    LightmapGi,
    AreaLight,
    ReflectionProbe,
    Ssao,
    Ssil,
    Glow,
    AutoExposure,
    SunShadow,
    FireShadows,
    FireLights,
    SoftShadows,
    Particles,
    TemporalUpscaler,
    PieceDynamicGi,
};

inline constexpr std::array<std::string_view, 15> kOffItemNames{
    "voxel_gi",    "lightmap_gi",  "area_light",   "reflection_probe", "ssao",
    "ssil",        "glow",         "auto_exposure", "sun_shadow",      "fire_shadows",
    "fire_lights", "soft_shadows", "particles",    "temporal_upscaler", "piece_dynamic_gi",
};

inline std::string_view off_item_name(OffItem item) { return kOffItemNames[static_cast<std::size_t>(item)]; }

struct OffItems {
    std::uint32_t bits = 0;

    bool has(OffItem item) const { return (bits >> static_cast<unsigned>(item)) & 1u; }
    bool empty() const { return bits == 0; }
    std::vector<std::string_view> names() const {
        std::vector<std::string_view> out;
        for (std::size_t i = 0; i < kOffItemNames.size(); ++i)
            if ((bits >> i) & 1u) out.push_back(kOffItemNames[i]);
        return out;
    }
};

// Parses a comma-separated list of item names. Returns nullopt and sets
// `unknown` to the first name that is not an item; an empty list is valid
// and turns nothing off.
inline std::optional<OffItems> parse_off_items(std::string_view list, std::string& unknown) {
    OffItems out;
    while (!list.empty()) {
        const std::size_t comma = list.find(',');
        const std::string_view name = list.substr(0, comma);
        list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
        if (name.empty()) continue;
        bool found = false;
        for (std::size_t i = 0; i < kOffItemNames.size(); ++i) {
            if (kOffItemNames[i] == name) {
                out.bits |= 1u << i;
                found = true;
            }
        }
        if (!found) {
            unknown = std::string(name);
            return std::nullopt;
        }
    }
    return out;
}

}  // namespace montezuma::godot_binding::bench
