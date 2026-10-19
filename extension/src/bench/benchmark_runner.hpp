// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "ablation.hpp"
#include "frame_stats.hpp"

#include "match3_board.hpp"
#include "replay.hpp"

namespace montezuma::godot_binding {

class BoardView3D;

// The frame-time benchmark (WP-46, docs/reference/benchmark.md): replays the
// busiest-board script through a BoardView3D, warms up, then records CPU and
// GPU frame time from RenderingServer every frame and writes the summary, the
// budget verdict, the render size it held and the active feature list as
// JSON. Options come from the properties or from
// user command-line arguments (`-- --bench-seconds=20`).
class BenchmarkRunner : public godot::Node {
    GDCLASS(BenchmarkRunner, godot::Node)

public:
    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void set_board_view_path(const godot::NodePath& p) { board_view_path_ = p; }
    godot::NodePath get_board_view_path() const { return board_view_path_; }
    void set_seed(int64_t v) { seed_ = v; }
    int64_t get_seed() const { return seed_; }
    void set_warmup_seconds(double v) { warmup_seconds_ = v; }
    double get_warmup_seconds() const { return warmup_seconds_; }
    void set_measure_seconds(double v) { measure_seconds_ = v; }
    double get_measure_seconds() const { return measure_seconds_; }
    void set_output_path(const godot::String& v) { output_path_ = v; }
    godot::String get_output_path() const { return output_path_; }
    void set_tier(const godot::String& v) { tier_ = v; }
    godot::String get_tier() const { return tier_; }
    void set_budget_ms(double v) { budget_ms_ = v; }
    double get_budget_ms() const { return budget_ms_; }
    void set_tolerance(double v) { tolerance_ = v; }
    double get_tolerance() const { return tolerance_; }
    // (0, 0): no size requested; the run still fails if the size changes.
    void set_render_size(const godot::Vector2i& v) { render_size_ = v; }
    godot::Vector2i get_render_size() const { return render_size_; }
    void set_disable_vsync(bool v) { disable_vsync_ = v; }
    bool get_disable_vsync() const { return disable_vsync_; }
    void set_quit_when_done(bool v) { quit_when_done_ = v; }
    bool get_quit_when_done() const { return quit_when_done_; }

    // The summary written to output_path; empty until the capture ends.
    godot::Dictionary get_report() const { return report_; }

protected:
    static void _bind_methods();

private:
    enum class Phase { Idle, Warmup, Measure, Done };

    BoardView3D* view() const;
    void read_command_line();
    void lock_render_size();
    godot::Vector2i current_render_size() const;
    void restart_replay();
    void play_next_beat();
    void sample_frame();
    void apply_off_items();
    void push_process_sample();
    void on_process_frame();
    void finish();

    godot::NodePath board_view_path_;
    int64_t seed_ = 52;
    double warmup_seconds_ = 10.0;
    double measure_seconds_ = 60.0;
    godot::String output_path_ = "user://bench/benchmark.json";
    // Empty: the tier QualityTiers has in effect.
    godot::String tier_;
    // 0: the tier's p95 budget from the visual target.
    double budget_ms_ = 0.0;
    double tolerance_ = bench::kDefaultTolerance;
    godot::Vector2i render_size_;
    bool disable_vsync_ = false;
    bool size_arg_invalid_ = false;
    bool tolerance_arg_invalid_ = false;
    bool quit_when_done_ = true;
    // --bench-off: render items turned off for an ablation run; the first
    // unknown name, if any.
    bench::OffItems off_;
    godot::String off_arg_invalid_;

    godot::Ref<Match3Board> board_;
    bench::Replay replay_;
    std::size_t next_beat_ = 0;
    int loops_ = 0;
    int beats_played_ = 0;
    int deep_cascades_played_ = 0;
    int max_steps_played_ = 0;
    bool diverged_ = false;
    uint64_t hold_until_usec_ = 0;

    Phase phase_ = Phase::Idle;
    uint64_t start_usec_ = 0;
    uint64_t measure_start_usec_ = 0;
    uint64_t last_frame_usec_ = 0;
    std::vector<double> process_ms_;
    std::vector<double> render_cpu_ms_;
    std::vector<double> cpu_ms_;
    std::vector<double> gpu_ms_;
    std::vector<double> frame_ms_;
    std::vector<double> draw_calls_;
    // Draw calls of the root viewport by pass: opaque and transparent scene,
    // shadow maps, 2D canvas.
    std::vector<double> scene_draw_calls_;
    std::vector<double> shadow_draw_calls_;
    std::vector<double> canvas_draw_calls_;
    std::vector<double> primitives_;
    double video_mem_max_ = 0.0;
    bench::SizeTracker sizes_{std::nullopt};
    bench::SizeTracker window_sizes_{std::nullopt};
    // When SceneTree.process_frame fired: the start of this frame's _process.
    uint64_t process_start_usec_ = 0;
    double display_hz_ = -1.0;
    godot::String vsync_;
    godot::Dictionary features_;
    godot::Dictionary report_;
};

}  // namespace montezuma::godot_binding
