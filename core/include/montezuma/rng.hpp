// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>

namespace montezuma {

// Small deterministic RNG (xorshift64*). Same seed gives the same sequence on
// every platform and compiler, unlike std:: distributions.
class Rng {
public:
    explicit Rng(std::uint64_t seed = 0) { reseed(seed); }

    void reseed(std::uint64_t seed) {
        // splitmix64 step so small seeds still give well-mixed state.
        std::uint64_t z = seed + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        state_ = (z ^ (z >> 31)) | 1ull;
    }

    std::uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1Dull;
    }

    // Uniform integer in [0, bound). bound must be > 0.
    int below(int bound) { return static_cast<int>(next() % static_cast<std::uint64_t>(bound)); }

private:
    std::uint64_t state_ = 1;
};

}  // namespace montezuma
