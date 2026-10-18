// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "flicker.hpp"

#include <cmath>

namespace montezuma::godot_binding::flicker {

namespace {

std::uint32_t hash(std::uint32_t x) {
    // lowbias32 (Chris Wellons), public domain integer hash.
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float slope(std::int32_t i, std::uint32_t seed) {
    return static_cast<float>(hash(static_cast<std::uint32_t>(i) ^ hash(seed))) / 2147483648.0f - 1.0f;
}

// Octaves: frequency relative to `speed`, and weight.
constexpr float kFreq[4] = {0.23f, 1.0f, 2.17f, 4.71f};
constexpr float kWeight[4] = {0.45f, 0.35f, 0.14f, 0.06f};

}  // namespace

float gradient_noise(float x, std::uint32_t seed) {
    const float fl = std::floor(x);
    const auto i = static_cast<std::int32_t>(fl);
    const float f = x - fl;
    const float a = slope(i, seed) * f;
    const float b = slope(i + 1, seed) * (f - 1.0f);
    const float s = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    // The largest value 1D gradient noise reaches is 0.5; scale to [-1, 1].
    return 2.0f * (a + (b - a) * s);
}

float fire(float t, float speed, std::uint32_t seed) {
    float sum = 0.0f;
    for (int k = 0; k < 4; ++k) {
        sum += kWeight[k] * gradient_noise(t * speed * kFreq[k], seed + 0x9e3779b9U * static_cast<std::uint32_t>(k + 1));
    }
    return sum < -1.0f ? -1.0f : (sum > 1.0f ? 1.0f : sum);
}

std::uint32_t seed_from_position(float x, float y, float z) {
    const auto q = [](float v) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(v * 100.0f))); };
    return hash(q(x) ^ hash(q(y) ^ hash(q(z))));
}

}  // namespace montezuma::godot_binding::flicker
