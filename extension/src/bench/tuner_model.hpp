// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The Godot-free parts of the render tuning screen (RenderTuner,
// docs/reference/render-tuner.md): reading Godot's property hint strings
// into slider and option-list settings, and the rolling frame-time window
// behind the performance readout. Tested by bench_tests.
namespace montezuma::godot_binding::tuner {

namespace detail {

inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

inline std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == sep) {
            out.push_back(trim(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    return out;
}

inline std::optional<double> to_number(std::string_view s) {
    if (s.empty()) return std::nullopt;
    const std::string copy(s);
    char* end = nullptr;
    const double v = std::strtod(copy.c_str(), &end);
    if (end != copy.c_str() + copy.size()) return std::nullopt;
    return v;
}

}  // namespace detail

// A PROPERTY_HINT_RANGE hint string: "min,max[,step][,or_greater][,or_less][,exp][,suffix:m]...".
// `valid` is false when the hint is not a range. (A plain struct rather than
// std::optional: GCC's -Wmaybe-uninitialized misreads the optional at -O3.)
struct RangeHint {
    bool valid = false;
    double min = 0.0;
    double max = 1.0;
    double step = 0.0;  // 0: the caller picks one
    bool or_greater = false;
    bool or_less = false;
    bool exp = false;
};

inline RangeHint parse_range_hint(std::string_view hint) {
    RangeHint r;
    const std::vector<std::string_view> parts = detail::split(hint, ',');
    if (parts.size() < 2) return r;
    const std::optional<double> lo = detail::to_number(parts[0]);
    const std::optional<double> hi = detail::to_number(parts[1]);
    if (!lo || !hi || *hi < *lo) return r;
    r.valid = true;
    r.min = *lo;
    r.max = *hi;
    for (std::size_t i = 2; i < parts.size(); ++i) {
        const std::string_view p = parts[i];
        if (i == 2) {
            if (const std::optional<double> step = detail::to_number(p)) {
                r.step = std::max(0.0, *step);
                continue;
            }
        }
        if (p == "or_greater") r.or_greater = true;
        if (p == "or_less") r.or_less = true;
        if (p == "exp") r.exp = true;
    }
    return r;
}

// A PROPERTY_HINT_ENUM hint string: "Off,Low,High" or "Low:1,High:4"; an
// entry without a value takes the previous value plus one, as in Godot.
struct EnumEntry {
    std::string label;
    int64_t value = 0;
};

inline std::vector<EnumEntry> parse_enum_hint(std::string_view hint) {
    std::vector<EnumEntry> out;
    if (detail::trim(hint).empty()) return out;
    int64_t next = 0;
    for (const std::string_view part : detail::split(hint, ',')) {
        EnumEntry e;
        const std::size_t colon = part.rfind(':');
        std::optional<double> value;
        if (colon != std::string_view::npos) value = detail::to_number(detail::trim(part.substr(colon + 1)));
        if (value) {
            e.label = std::string(detail::trim(part.substr(0, colon)));
            e.value = static_cast<int64_t>(*value);
        } else {
            e.label = std::string(part);
            e.value = next;
        }
        next = e.value + 1;
        out.push_back(std::move(e));
    }
    return out;
}

// A slider step for a range with no step of its own: a hundredth of the
// span, rounded down to 1, 2 or 5 times a power of ten, and 1 for integers.
inline double default_step(double min, double max, bool integer) {
    if (integer) return 1.0;
    const double span = max - min;
    if (span <= 0.0) return 0.001;
    double step = 1.0;
    while (step > span / 100.0) step /= 10.0;
    while (step * 10.0 <= span / 100.0) step *= 10.0;
    if (step * 5.0 <= span / 100.0) return step * 5.0;
    if (step * 2.0 <= span / 100.0) return step * 2.0;
    return step;
}

// The last kCapacity frames, in milliseconds: whole frame, render CPU and
// GPU. Fixed storage, so recording a frame never allocates.
struct FrameSample {
    double frame_ms = 0.0;
    double cpu_ms = 0.0;
    double gpu_ms = 0.0;  // 0: the driver gave no timing for this frame
};

struct WindowStats {
    std::size_t frames = 0;
    double fps = 0.0;  // frames over the window's wall time
    double frame_mean = 0.0;
    double frame_p99 = 0.0;  // the 1% slowest frames start here
    double frame_max = 0.0;
    double cpu_mean = 0.0;
    double gpu_mean = 0.0;  // over frames with a GPU timing; 0 when none had one
    double gpu_p99 = 0.0;
};

class FrameWindow {
public:
    static constexpr std::size_t kCapacity = 600;

    void push(const FrameSample& s) {
        samples_[next_] = s;
        next_ = (next_ + 1) % kCapacity;
        size_ = std::min(size_ + 1, kCapacity);
    }

    void clear() {
        next_ = 0;
        size_ = 0;
    }

    std::size_t size() const { return size_; }

    // The i-th oldest of the stored frames, 0 <= i < size().
    const FrameSample& at(std::size_t i) const { return samples_[(next_ + kCapacity - size_ + i) % kCapacity]; }

    // Statistics over the newest `count` frames (all of them when fewer).
    WindowStats stats(std::size_t count) {
        WindowStats w;
        const std::size_t n = std::min(count, size_);
        if (n == 0) return w;
        double frame_sum = 0.0, cpu_sum = 0.0, gpu_sum = 0.0;
        std::size_t gpu_n = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const FrameSample& s = at(size_ - n + i);
            scratch_[i] = s.frame_ms;
            frame_sum += s.frame_ms;
            cpu_sum += s.cpu_ms;
            w.frame_max = std::max(w.frame_max, s.frame_ms);
            if (s.gpu_ms > 0.0) {
                gpu_scratch_[gpu_n++] = s.gpu_ms;
                gpu_sum += s.gpu_ms;
            }
        }
        w.frames = n;
        w.frame_mean = frame_sum / static_cast<double>(n);
        w.cpu_mean = cpu_sum / static_cast<double>(n);
        w.fps = frame_sum > 0.0 ? 1000.0 * static_cast<double>(n) / frame_sum : 0.0;
        w.frame_p99 = percentile(scratch_.data(), n, 99.0);
        if (gpu_n > 0) {
            w.gpu_mean = gpu_sum / static_cast<double>(gpu_n);
            w.gpu_p99 = percentile(gpu_scratch_.data(), gpu_n, 99.0);
        }
        return w;
    }

private:
    // Nearest-rank percentile, as the benchmark's (frame_stats.hpp); reorders `v`.
    static double percentile(double* v, std::size_t n, double p) {
        std::size_t rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(n)));
        rank = std::clamp<std::size_t>(rank, 1, n);
        std::nth_element(v, v + (rank - 1), v + n);
        return v[rank - 1];
    }

    std::array<FrameSample, kCapacity> samples_{};
    std::array<double, kCapacity> scratch_{};
    std::array<double, kCapacity> gpu_scratch_{};
    std::size_t next_ = 0;
    std::size_t size_ = 0;
};

}  // namespace montezuma::godot_binding::tuner
