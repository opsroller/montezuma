// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace montezuma::godot_binding::bench {

// Frame-time summary of one capture, in milliseconds (docs/reference/benchmark.md).
struct FrameSummary {
    std::size_t frames = 0;
    double mean = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double max = 0.0;
    // Frames strictly over the budget; 0 when the budget is 0.
    std::size_t over_budget = 0;
};

// Nearest-rank percentile of an ascending, non-empty sample: the smallest
// value with at least `p` percent of the samples at or below it.
inline double nearest_rank(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    const double rank = std::ceil(p / 100.0 * static_cast<double>(sorted.size()));
    const std::size_t i = rank < 1.0 ? 0 : static_cast<std::size_t>(rank) - 1;
    return sorted[std::min(i, sorted.size() - 1)];
}

// Summarises `samples` (taken by value: it is sorted here, once, after the
// capture, never per frame).
inline FrameSummary summarize(std::vector<double> samples, double budget_ms) {
    FrameSummary s;
    s.frames = samples.size();
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) {
        sum += v;
        if (budget_ms > 0.0 && v > budget_ms) ++s.over_budget;
    }
    s.mean = sum / static_cast<double>(samples.size());
    s.p50 = nearest_rank(samples, 50.0);
    s.p95 = nearest_rank(samples, 95.0);
    s.p99 = nearest_rank(samples, 99.0);
    s.max = samples.back();
    return s;
}

}  // namespace montezuma::godot_binding::bench
