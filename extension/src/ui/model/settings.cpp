// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "settings.hpp"

#include <algorithm>
#include <cmath>

namespace montezuma::ui {

namespace {
float clamp_finite(float v, float lo, float hi, float fallback) {
    if (!std::isfinite(v)) return fallback;
    return std::clamp(v, lo, hi);
}
}  // namespace

void Settings::sanitize() {
    const Settings defaults;
    music_volume = clamp_finite(music_volume, 0.0f, 1.0f, defaults.music_volume);
    sfx_volume = clamp_finite(sfx_volume, 0.0f, 1.0f, defaults.sfx_volume);
    text_scale = clamp_finite(text_scale, kMinTextScale, kMaxTextScale, defaults.text_scale);
    if (difficulty != "normal" && difficulty != "hard") difficulty = defaults.difficulty;
}

float linear_to_db(float linear) {
    if (!(linear > 0.0f)) return -80.0f;
    return std::max(-80.0f, 20.0f * std::log10(std::min(linear, 1.0f)));
}

}  // namespace montezuma::ui
