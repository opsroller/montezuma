// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

#include "match3_board.hpp"
#include "render_probe.hpp"

namespace montezuma::godot_binding {

class BoardView3D;

// Look-dev capture for the readability rules (WP-46,
// docs/reference/benchmark.md#readability-capture): shows a still board,
// renders it with all effects on, all off, only the persistent atmosphere
// and only glow, and saves each (the mean of several frames, with the fire
// lights held still) as a PNG next to a layout.json that
// gives every cell's kind, tile shade and screen rectangle.
// tools/bench/check_readability.py checks rules 2, 4, 5 and 6 on them.
class ReadabilityCapture : public godot::Node {
    GDCLASS(ReadabilityCapture, godot::Node)

public:
    void _ready() override;
    void _process(double delta) override;

    void set_board_view_path(const godot::NodePath& p) { board_view_path_ = p; }
    godot::NodePath get_board_view_path() const { return board_view_path_; }
    void set_seed(int64_t v) { seed_ = v; }
    int64_t get_seed() const { return seed_; }
    void set_output_dir(const godot::String& v) { output_dir_ = v; }
    godot::String get_output_dir() const { return output_dir_; }
    void set_settle_frames(int v) { settle_frames_ = v; }
    int get_settle_frames() const { return settle_frames_; }
    void set_average_frames(int v) { average_frames_ = v; }
    int get_average_frames() const { return average_frames_; }

protected:
    static void _bind_methods();

private:
    BoardView3D* view() const;
    bool accumulate_frame();
    void save_frame(const char* name);
    void write_layout();
    void fail(const godot::String& why);

    godot::NodePath board_view_path_;
    int64_t seed_ = 52;
    godot::String output_dir_ = "user://bench/readability";
    // Frames each effect set renders before its capture, so temporal
    // effects (TAA, auto exposure, glow history) settle.
    int settle_frames_ = 45;
    // Frames averaged into each capture after it settles. Temporal
    // anti-aliasing and upscaling jitter every frame and the fire lights
    // flicker, so one frame differs from the next by more than the rules'
    // thresholds on desktop tiers; the mean of several does not.
    int average_frames_ = 64;

    godot::Ref<Match3Board> board_;
    bench::EffectSwitch effects_;
    int set_index_ = 0;
    int frames_in_set_ = 0;
    int averaged_ = 0;
    int64_t frame_width_ = 0;
    int64_t frame_height_ = 0;
    std::vector<std::uint32_t> sum_;  // per-channel RGB sums of the averaged frames
    bool running_ = false;
};

}  // namespace montezuma::godot_binding
