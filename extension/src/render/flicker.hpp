// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>

namespace montezuma::godot_binding::flicker {

// Smooth, deterministic noise for fire light: a sum of four octaves of 1D
// gradient noise, most energy around `speed` Hz with slower swells and
// faster crackle, like a real flame's light. Godot-free so it is unit
// tested; FlickerLight3D drives a light with it.

// 1D gradient noise in [-1, 1]; 0 at whole numbers, smooth in between.
float gradient_noise(float x, std::uint32_t seed);

// Fire flicker at time t seconds, in [-1, 1], mean about 0.
float fire(float t, float speed, std::uint32_t seed);

// A seed from a position, so lights at different places never flicker in step.
std::uint32_t seed_from_position(float x, float y, float z);

}  // namespace montezuma::godot_binding::flicker
