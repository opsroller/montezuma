// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <string>

namespace montezuma::ui {

// Player preferences shown on the settings screen. Values only; the
// Godot side applies and stores them.
struct Settings {
    static constexpr float kMinTextScale = 1.0f;
    static constexpr float kMaxTextScale = 2.0f;

    float music_volume = 0.8f;      // linear 0..1
    float sfx_volume = 0.8f;        // linear 0..1
    bool fullscreen = false;
    std::string locale;             // empty: follow the system
    float text_scale = 1.0f;        // kMinTextScale..kMaxTextScale
    bool reduced_motion = false;
    std::string difficulty = "normal";  // "normal" or "hard" (LevelData gem_goal/time_limit keys)

    // Brings every field into range; unknown difficulty becomes "normal".
    void sanitize();
};

// Decibels for a linear 0..1 volume, with 0 mapped to silence (-80 dB).
float linear_to_db(float linear);

}  // namespace montezuma::ui
