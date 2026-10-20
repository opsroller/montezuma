// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Benchmark replay and frame statistics tests. No Godot, no framework; run via ctest.
#include <cmath>
#include <cstdio>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "frame_stats.hpp"
#include "montezuma/board.hpp"
#include "replay.hpp"
#include "tuner_model.hpp"

using namespace montezuma;
using namespace montezuma::godot_binding::bench;

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

// The seed game/scenes/bench/bench.tscn uses.
constexpr std::uint64_t kSceneSeed = 52;

void test_nearest_rank_percentiles() {
    std::vector<double> v;
    for (int i = 1; i <= 100; ++i) v.push_back(static_cast<double>(101 - i));  // 100 .. 1, unsorted
    const FrameSummary s = summarize(v, 90.0);
    check(s.frames == 100, "frame count");
    check(s.p50 == 50.0, "p50 of 1..100 is 50");
    check(s.p95 == 95.0, "p95 of 1..100 is 95");
    check(s.p99 == 99.0, "p99 of 1..100 is 99");
    check(s.max == 100.0, "max");
    check(s.mean == 50.5, "mean");
    check(s.over_budget == 10, "frames over a 90 ms budget");
    check(summarize({}, 16.6).frames == 0 && summarize({}, 16.6).p95 == 0.0, "empty capture is all zero");
    check(summarize({7.0}, 0.0).p50 == 7.0 && summarize({7.0}, 0.0).over_budget == 0, "single sample, no budget");
}

void test_replay_is_deterministic() {
    ReplayConfig c;
    c.seed = kSceneSeed;
    const Replay a = plan_replay(c);
    const Replay b = plan_replay(c);
    check(a.actions.size() == b.actions.size(), "same length");
    bool same = true;
    for (std::size_t i = 0; i < a.actions.size() && i < b.actions.size(); ++i) {
        same = same && a.actions[i].board_hash == b.actions[i].board_hash && a.actions[i].label == b.actions[i].label;
    }
    check(same, "same beats and boards for the same seed");
}

// The capture covers about the first three rounds; they must hold the busy
// parts of the scenario on their own.
void test_scene_seed_is_the_busiest_board() {
    ReplayConfig c;
    c.seed = kSceneSeed;
    c.rounds = 3;
    const Replay r = plan_replay(c);
    check(r.deep_cascades >= 5, "at least five four-step cascades in three rounds");
    check(r.power_ups == 6, "two power-ups a round");
    check(r.totems == 21, "every totem fires each round");
    std::set<std::string> labels;
    for (const Action& a : r.actions) labels.insert(a.label);
    for (const char* l : {"totem:fire", "totem:time", "totem:gold", "totem:harvest", "totem:ward", "totem:gift",
                          "totem:paint", "powerup:tnt", "powerup:lightning", "swap"}) {
        check(labels.count(l) == 1, std::string("beat present: ") + l);
    }
}

// Replaying the plan through Board's public moves, as the Godot runner does
// through Match3Board, lands on the planned board after every beat.
void test_replay_matches_a_fresh_board() {
    ReplayConfig c;
    c.seed = kSceneSeed;
    const Replay r = plan_replay(c);
    auto board = Board::try_create(c.width, c.height, c.kinds, c.seed);
    check(board.has_value(), "scene board is valid");
    if (!board) return;
    bool match = true;
    for (const Action& a : r.actions) {
        if (a.kind == Action::Kind::Swap) match = match && board->try_swap(a.a, a.b).has_value();
        if (a.kind == Action::Kind::Clear) match = match && board->clear_cells(a.cells).has_value();
        match = match && hash_cells(board->cells()) == a.board_hash;
    }
    check(match, "every beat replays to the planned board");
}

void test_bad_board_throws() {
    bool threw = false;
    try {
        ReplayConfig c;
        c.width = 2;
        plan_replay(c);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "a board try_create refuses throws invalid_argument");
}

}  // namespace

// A vsync-locked 60 Hz run: every frame one display period, with the jitter
// a compositor adds. It must pass, though the frames sit just above 16.6 ms.
std::vector<double> vsync_locked_60hz(std::size_t n) {
    std::vector<double> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back(1000.0 / 60.0 + (i % 3 == 0 ? 0.25 : -0.1));
    return v;
}

void test_budget_is_one_display_period() {
    const BudgetSpec desk = budget_for_tier("desktop-standard", kDefaultTolerance, 0.0);
    check(desk.rules.size() == 1 && desk.rules[0].percentile == 95.0, "desktop judges p95");
    check(std::abs(desk.rules[0].budget_ms - 1000.0 / 60.0) < 1e-9, "desktop budget is one 60 Hz period");
    const BudgetSpec web = budget_for_tier("web", kDefaultTolerance, 0.0);
    check(web.rules.size() == 2, "web judges p95 and the median");
    check(std::abs(web.rules[0].budget_ms - 2000.0 / 60.0) < 1e-9, "web p95 budget is two periods");
    check(web.rules[1].percentile == 50.0 && std::abs(web.rules[1].budget_ms - 1000.0 / 60.0) < 1e-9,
          "web median budget is one period");
    const BudgetSpec forced = budget_for_tier("desktop-high", kDefaultTolerance, 20.0);
    check(forced.rules[0].budget_ms == 20.0, "--bench-budget replaces the p95 budget");
    check(std::abs(p95_limit_ms(desk) - 1000.0 / 60.0 * 1.05) < 1e-9, "the p95 limit adds the tolerance");
}

void test_vsync_locked_60fps_passes() {
    const BudgetSpec spec = budget_for_tier("desktop-standard", kDefaultTolerance, 0.0);
    const std::vector<double> frames = vsync_locked_60hz(600);
    const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 60.0);
    check(v.pass, "vsync-locked 60 fps is within budget: " + v.reason);
    check(summarize(frames, p95_limit_ms(spec)).over_budget == 0, "no vsync-locked frame counts as over budget");
    const BudgetVerdict no_tol = judge_budget(summarize(frames, 0.0), budget_for_tier("desktop-standard", 0.0, 0.0), 60.0);
    check(!no_tol.pass, "with no tolerance the same run fails (the bug the tolerance fixes)");
}

void test_slow_frames_fail_with_a_reason() {
    const BudgetSpec spec = budget_for_tier("desktop-high", kDefaultTolerance, 0.0);
    const std::vector<double> frames(600, 23.0);  // the Mac mini M4 desktop-high run
    const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 60.0);
    check(!v.pass, "23 ms frames fail the 60 fps budget");
    check(v.limits.size() == 1 && !v.limits[0].pass && v.limits[0].value_ms == 23.0, "the failing limit is reported");
    check(v.reason.find("p95") != std::string::npos, "the reason names the percentile");
}

void test_web_median_counts() {
    const BudgetSpec spec = budget_for_tier("web", kDefaultTolerance, 0.0);
    const std::vector<double> frames(600, 25.0);  // p95 within 33.3 ms, median over 16.7 ms
    const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 60.0);
    check(!v.pass && v.limits[0].pass && !v.limits[1].pass, "web fails on the median alone");
}

void test_slow_display_is_not_judged() {
    const BudgetSpec spec = budget_for_tier("desktop-standard", kDefaultTolerance, 0.0);
    const std::vector<double> frames(600, 20.0);  // vsync-locked to a 50 Hz display
    const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 50.0);
    check(!v.pass && v.reason.find("display") != std::string::npos, "a 50 Hz display fails as display-limited");
    const BudgetVerdict ntsc = judge_budget(summarize(vsync_locked_60hz(600), p95_limit_ms(spec)), spec, 59.94);
    check(ntsc.pass, "a 59.94 Hz display still counts as 60 Hz");
    const BudgetVerdict unknown = judge_budget(summarize(vsync_locked_60hz(600), p95_limit_ms(spec)), spec, -1.0);
    check(unknown.pass, "an unknown refresh rate (headless) is not held against the run");
}

void test_empty_capture_fails() {
    const BudgetSpec spec = budget_for_tier("desktop-standard", kDefaultTolerance, 0.0);
    const BudgetVerdict v = judge_budget(summarize({}, 0.0), spec, 60.0);
    check(!v.pass && !v.reason.empty(), "no frames, no pass");
}

void test_size_parsing() {
    const auto s = parse_size("2560x1440");
    check(s && s->w == 2560 && s->h == 1440, "WxH parses");
    check(!parse_size("abc") && !parse_size("0x10") && !parse_size("10x") && !parse_size("") &&
              !parse_size("1920x1080x2") && !parse_size("-5x10"),
          "bad sizes are rejected");
}

void test_resolution_tracker() {
    SizeTracker steady(Size2{1920, 1080});
    for (int i = 0; i < 100; ++i) steady.add({1920, 1080});
    check(steady.pass() && steady.mismatched == 0, "a steady capture at the requested size passes");

    SizeTracker resized(Size2{2560, 1440});
    for (int i = 0; i < 50; ++i) resized.add({2560, 1440});
    for (int i = 0; i < 50; ++i) resized.add({2560, 1386});  // the window shrank mid-run
    check(!resized.pass() && resized.mismatched == 50, "a mid-run resize fails and is counted");
    check(resized.min.h == 1386 && resized.max.h == 1440, "min and max render size are kept");

    SizeTracker wrong(Size2{2560, 1440});
    for (int i = 0; i < 10; ++i) wrong.add({1920, 1080});
    check(!wrong.pass(), "a capture at another size fails");

    SizeTracker free_size(std::nullopt);
    for (int i = 0; i < 10; ++i) free_size.add({1280, 720});
    check(free_size.pass(), "no requested size: a steady size passes");
    free_size.add({1280, 700});
    check(!free_size.pass(), "no requested size: a change of size still fails");
    check(!SizeTracker(Size2{1, 1}).pass(), "no frames, no pass");
}

void test_unknown_tier_is_not_judged_silently() {
    for (const char* tier : {"unset", "desktop-hgih", "", "Web"}) {
        const BudgetSpec spec = budget_for_tier(tier, kDefaultTolerance, 0.0);
        const std::vector<double> frames(600, 16.9);
        const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 60.0);
        check(!v.pass && v.reason.find("tier") != std::string::npos,
              std::string("unknown tier '") + tier + "' fails with a reason naming the tier");
    }
    for (const char* tier : {"desktop-high", "desktop-standard", "mobile", "web"})
        check(!budget_for_tier(tier, kDefaultTolerance, 0.0).rules.empty(), std::string("known tier ") + tier);
}

void test_tolerance_is_bounded() {
    const BudgetSpec spec = budget_for_tier("desktop-high", 10.0, 0.0);
    check(spec.tolerance <= kMaxTolerance, "a tolerance above the maximum is clamped");
    const std::vector<double> frames(600, 100.0);
    check(!judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 60.0).pass, "10 fps never passes the 60 fps budget");
    check(budget_for_tier("desktop-high", -1.0, 0.0).tolerance == 0.0, "a negative tolerance is 0");
}

void test_too_few_frames_fail() {
    const BudgetSpec spec = budget_for_tier("desktop-high", kDefaultTolerance, 0.0);
    const std::vector<double> frames(kMinFrames - 1, 10.0);
    const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, 60.0);
    check(!v.pass && v.reason.find("frames") != std::string::npos, "a capture of a handful of frames is not judged");
}

void test_unmeasured_runs_do_not_pass() {
    const BudgetSpec spec = budget_for_tier("desktop-standard", kDefaultTolerance, 0.0);
    const std::vector<double> frames(600, 7.0);
    const BudgetVerdict v = judge_budget(summarize(frames, p95_limit_ms(spec)), spec, -1.0, "headless, nothing rendered");
    check(!v.pass && v.reason.find("not a measurement") != std::string::npos && v.limits.size() == 1,
          "a headless run keeps its numbers but never passes");
    check(software_adapter("llvmpipe (LLVM 20.1.2, 256 bits)") && software_adapter("lavapipe") &&
              software_adapter("SwiftShader Device (Subzero)") && software_adapter("Microsoft Basic Render Driver") &&
              software_adapter("softpipe"),
          "software rasterisers are recognised");
    check(!software_adapter("Apple M4") && !software_adapter("NVIDIA GeForce RTX 3060") && !software_adapter(""),
          "GPUs are not software");
}

void test_missing_gpu_samples_are_dropped() {
    const std::vector<double> some = positive_only({0.0, 4.0, 0.0, 6.0});
    check(some.size() == 2 && some[0] == 4.0 && some[1] == 6.0, "frames without a GPU timestamp are left out");
    check(positive_only({0.0, 0.0}).empty(), "no timestamps, no samples");
}

// ---- Render tuning screen (tuner_model.hpp) ---------------------------------

void test_range_hints() {
    namespace t = montezuma::godot_binding::tuner;
    const auto r = t::parse_range_hint("0,16,0.01,or_greater,exp");
    check(r && r->min == 0.0 && r->max == 16.0 && r->step == 0.01, "range bounds and step");
    check(r && r->or_greater && r->exp && !r->or_less, "range flags");
    const auto suffix = t::parse_range_hint("-1, 1, suffix:m");
    check(suffix && suffix->min == -1.0 && suffix->max == 1.0 && suffix->step == 0.0, "range without a step keeps 0");
    check(!t::parse_range_hint("5"), "a range needs two bounds");
    check(!t::parse_range_hint("2,1"), "max below min is refused");
    check(!t::parse_range_hint("a,b"), "bounds must be numbers");
    check(t::default_step(0.0, 1.0, false) == 0.01, "a hundredth of 0..1");
    check(t::default_step(0.0, 16.0, false) == 0.1, "rounded down to 1, 2 or 5 x 10^n");
    check(t::default_step(0.0, 1000.0, false) == 10.0, "large spans step in tens");
    check(t::default_step(0.0, 3.0, true) == 1.0, "integers step by one");
}

void test_enum_hints() {
    namespace t = montezuma::godot_binding::tuner;
    const auto plain = t::parse_enum_hint("Off,Low,High");
    check(plain.size() == 3 && plain[0].value == 0 && plain[2].value == 2 && plain[1].label == "Low", "plain enum counts from 0");
    const auto valued = t::parse_enum_hint("Windowed:0,Maximized:2,Full:3,Exclusive");
    check(valued.size() == 4 && valued[1].value == 2 && valued[3].value == 4 && valued[1].label == "Maximized",
          "explicit values, and the next one after them");
    const auto numeric = t::parse_enum_hint("1024:1024,2048:2048");
    check(numeric.size() == 2 && numeric[1].value == 2048 && numeric[1].label == "2048", "numbers as labels");
    check(t::parse_enum_hint("").empty(), "empty hint, no entries");
}

void test_frame_window() {
    namespace t = montezuma::godot_binding::tuner;
    t::FrameWindow w;
    check(w.stats(10).frames == 0, "empty window");
    for (int i = 1; i <= 100; ++i) w.push({static_cast<double>(i), 1.0, i % 2 == 0 ? 2.0 : 0.0});
    const t::WindowStats s = w.stats(100);
    check(s.frames == 100 && std::abs(s.frame_mean - 50.5) < 1e-9, "mean frame time");
    check(s.frame_p99 == 99.0 && s.frame_max == 100.0, "nearest-rank p99 and max");
    check(std::abs(s.fps - 1000.0 * 100.0 / 5050.0) < 1e-9, "fps over the window's time");
    check(s.gpu_mean == 2.0, "frames without GPU timing are left out");
    const t::WindowStats last = w.stats(10);
    check(last.frames == 10 && std::abs(last.frame_mean - 95.5) < 1e-9, "only the newest frames");
    for (std::size_t i = 0; i < t::FrameWindow::kCapacity; ++i) w.push({1.0, 0.0, 0.0});
    check(w.size() == t::FrameWindow::kCapacity && w.at(0).frame_ms == 1.0, "old frames roll out");
    check(w.stats(1000).gpu_mean == 0.0, "no GPU timing at all reads 0");
}

int main() {
    test_nearest_rank_percentiles();
    test_replay_is_deterministic();
    test_scene_seed_is_the_busiest_board();
    test_replay_matches_a_fresh_board();
    test_bad_board_throws();
    test_budget_is_one_display_period();
    test_vsync_locked_60fps_passes();
    test_slow_frames_fail_with_a_reason();
    test_web_median_counts();
    test_slow_display_is_not_judged();
    test_empty_capture_fails();
    test_size_parsing();
    test_resolution_tracker();
    test_unknown_tier_is_not_judged_silently();
    test_tolerance_is_bounded();
    test_too_few_frames_fail();
    test_unmeasured_runs_do_not_pass();
    test_missing_gpu_samples_are_dropped();
    test_range_hints();
    test_enum_hints();
    test_frame_window();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
