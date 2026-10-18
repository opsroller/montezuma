// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
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

// ---- Frame budget (docs/reference/benchmark.md#budget) ----------------------

// The visual target's "60 fps": budgets are whole periods of this refresh.
inline constexpr double kTargetHz = 60.0;
// How far over one budget a percentile may sit and still pass: vsync jitter
// puts a locked 60 fps run a little either side of 16.67 ms. Owner: the
// graphics-programmer; a change is a change to the benchmark reference.
inline constexpr double kDefaultTolerance = 0.05;

struct BudgetRule {
    double percentile = 95.0;  // 50, 95 or 99
    double budget_ms = 0.0;    // before the tolerance
};

struct BudgetSpec {
    double target_hz = kTargetHz;
    double tolerance = kDefaultTolerance;
    std::vector<BudgetRule> rules;
};

// The visual target's budgets for `tier`: p95 within one period on desktop
// and mobile; p95 within two periods and the median within one on the web.
// `p95_override_ms` > 0 replaces the p95 budget (--bench-budget).
inline BudgetSpec budget_for_tier(std::string_view tier, double tolerance, double p95_override_ms) {
    BudgetSpec spec;
    spec.tolerance = std::max(0.0, tolerance);
    const double period = 1000.0 / spec.target_hz;
    const bool web = tier == "web";
    spec.rules.push_back({95.0, p95_override_ms > 0.0 ? p95_override_ms : (web ? 2.0 : 1.0) * period});
    if (web) spec.rules.push_back({50.0, period});
    return spec;
}

// The p95 budget with its tolerance: the line a frame must cross to count
// as over budget.
inline double p95_limit_ms(const BudgetSpec& spec) {
    for (const BudgetRule& r : spec.rules)
        if (r.percentile == 95.0) return r.budget_ms * (1.0 + spec.tolerance);
    return 0.0;
}

struct BudgetLimit {
    double percentile = 0.0;
    double budget_ms = 0.0;
    double limit_ms = 0.0;  // budget_ms with the tolerance
    double value_ms = 0.0;
    bool pass = false;
};

struct BudgetVerdict {
    double period_ms = 0.0;
    std::vector<BudgetLimit> limits;
    bool pass = false;
    // Why it failed, empty on a pass.
    std::string reason;
};

inline double percentile_of(const FrameSummary& s, double p) {
    if (p == 50.0) return s.p50;
    if (p == 99.0) return s.p99;
    return s.p95;
}

// Judges a frame-time summary against `spec`. `display_hz` is the refresh
// the frames were shown at (<= 0 when unknown, as headless): a display
// slower than the target cannot show it, so the run is not judged.
inline BudgetVerdict judge_budget(const FrameSummary& frames, const BudgetSpec& spec, double display_hz) {
    BudgetVerdict v;
    v.period_ms = 1000.0 / spec.target_hz;
    char buf[160];
    if (frames.frames == 0) {
        v.reason = "no frames captured";
        return v;
    }
    v.pass = true;
    for (const BudgetRule& r : spec.rules) {
        BudgetLimit l;
        l.percentile = r.percentile;
        l.budget_ms = r.budget_ms;
        l.limit_ms = r.budget_ms * (1.0 + spec.tolerance);
        l.value_ms = percentile_of(frames, r.percentile);
        l.pass = l.value_ms <= l.limit_ms;
        if (!l.pass && v.pass) {
            std::snprintf(buf, sizeof buf, "frame_ms p%.0f %.2f ms over %.2f ms (%.2f ms budget + %.0f%%)", r.percentile,
                          l.value_ms, l.limit_ms, l.budget_ms, spec.tolerance * 100.0);
            v.reason = buf;
        }
        v.pass = v.pass && l.pass;
        v.limits.push_back(l);
    }
    if (display_hz > 0.0 && display_hz < spec.target_hz * (1.0 - spec.tolerance)) {
        std::snprintf(buf, sizeof buf, "display-limited: the display refreshes at %.2f Hz, below the %.0f Hz target",
                      display_hz, spec.target_hz);
        v.reason = buf;
        v.pass = false;
    }
    return v;
}

// ---- Render size ------------------------------------------------------------

struct Size2 {
    int w = 0;
    int h = 0;
    bool operator==(const Size2&) const = default;
};

// "WIDTHxHEIGHT", both positive; nothing else.
inline std::optional<Size2> parse_size(std::string_view text) {
    const std::size_t x = text.find('x');
    if (x == std::string_view::npos || x == 0 || x + 1 >= text.size()) return std::nullopt;
    auto number = [](std::string_view t) -> int {
        if (t.empty() || t.size() > 5) return 0;
        int n = 0;
        for (const char c : t) {
            if (c < '0' || c > '9') return 0;
            n = n * 10 + (c - '0');
        }
        return n;
    };
    const int w = number(text.substr(0, x));
    const int h = number(text.substr(x + 1));
    if (w <= 0 || h <= 0) return std::nullopt;
    return Size2{w, h};
}

// Follows the render size frame by frame. It passes when every frame had
// the requested size or, with none requested, the first frame's size.
struct SizeTracker {
    explicit SizeTracker(std::optional<Size2> requested) : requested(requested) {}

    void add(Size2 s) {
        if (frames == 0) first = min = max = s;
        min = {std::min(min.w, s.w), std::min(min.h, s.h)};
        max = {std::max(max.w, s.w), std::max(max.h, s.h)};
        if (!(s == requested.value_or(first))) ++mismatched;
        ++frames;
    }
    bool pass() const { return frames > 0 && mismatched == 0; }

    std::optional<Size2> requested;
    Size2 first, min, max;
    std::size_t frames = 0;
    std::size_t mismatched = 0;
};

}  // namespace montezuma::godot_binding::bench
