// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "benchmark_runner.hpp"

#include <algorithm>
#include <exception>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "board/board_view_3d.hpp"
#include "frame_stats.hpp"
#include "render/quality_tiers.hpp"
#include "render_probe.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// A beat with no board change (a totem the Board does not model) holds the
// board this long, so the effect scene that will play there has its time.
constexpr uint64_t kMarkerHoldUsec = 600'000;
// Frames reserved up front: 60 s at 240 fps, so sampling never allocates on
// fast displays; past it, vectors grow as usual.
constexpr std::size_t kReservedFramesPerSecond = 240;

double seconds_between(uint64_t from_usec, uint64_t to_usec) {
    return static_cast<double>(to_usec - from_usec) / 1'000'000.0;
}

Dictionary to_dictionary(const bench::FrameSummary& s) {
    Dictionary d;
    d["frames"] = static_cast<int64_t>(s.frames);
    d["mean"] = s.mean;
    d["p50"] = s.p50;
    d["p95"] = s.p95;
    d["p99"] = s.p99;
    d["max"] = s.max;
    d["over_budget"] = static_cast<int64_t>(s.over_budget);
    return d;
}

Array to_array(const std::vector<Cell>& cells) {
    Array out;
    for (const Cell c : cells) out.push_back(Vector2i(c.x, c.y));
    return out;
}

std::uint64_t hash_board(const Ref<Match3Board>& board) {
    const PackedInt32Array cells = board->get_cells();
    std::vector<int> v(static_cast<std::size_t>(cells.size()));
    for (int64_t i = 0; i < cells.size(); ++i) v[static_cast<std::size_t>(i)] = cells[i];
    return bench::hash_cells(v);
}

}  // namespace

BoardView3D* BenchmarkRunner::view() const {
    return board_view_path_.is_empty() ? nullptr : Object::cast_to<BoardView3D>(get_node_or_null(board_view_path_));
}

void BenchmarkRunner::read_command_line() {
    const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
    for (int64_t i = 0; i < args.size(); ++i) {
        const String a = args[i];
        const String value = a.get_slice("=", 1);
        if (a.begins_with("--bench-seconds=")) measure_seconds_ = value.to_float();
        if (a.begins_with("--bench-warmup=")) warmup_seconds_ = value.to_float();
        if (a.begins_with("--bench-out=")) output_path_ = value;
        if (a.begins_with("--bench-tier=")) tier_ = value;
        if (a.begins_with("--bench-budget=")) budget_ms_ = value.to_float();
        if (a.begins_with("--bench-seed=")) seed_ = value.to_int();
        if (a == "--bench-stay") quit_when_done_ = false;
    }
}

void BenchmarkRunner::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    read_command_line();
    if (tier_.is_empty()) {
        const QualityTiers* tiers = QualityTiers::get_singleton();
        tier_ = tiers ? tiers->get_tier() : String("unset");
    }
    // Visual target: p95 within 16.6 ms, 33.3 ms on the web tier.
    if (budget_ms_ <= 0.0) budget_ms_ = tier_ == "web" ? 33.3 : 16.6;
    BoardView3D* v = view();
    ERR_FAIL_NULL_MSG(v, "BenchmarkRunner: board_view_path does not point at a BoardView3D.");
    try {
        bench::ReplayConfig config;
        config.seed = static_cast<std::uint64_t>(seed_);
        replay_ = bench::plan_replay(config);
    } catch (const std::exception& e) {
        ERR_FAIL_MSG(String("BenchmarkRunner: cannot plan the replay: ") + e.what());
    }
    ERR_FAIL_COND_MSG(replay_.actions.empty(), "BenchmarkRunner: the replay has no beats.");

    const std::size_t frames = static_cast<std::size_t>(std::max(1.0, measure_seconds_)) * kReservedFramesPerSecond;
    for (auto* samples : {&cpu_ms_, &gpu_ms_, &frame_ms_, &draw_calls_, &primitives_}) samples->reserve(frames);

    RenderingServer::get_singleton()->viewport_set_measure_render_time(get_viewport()->get_viewport_rid(), true);
    bench::hide_ui_overlays(get_tree()->get_root());
    board_.instantiate();
    restart_replay();
    start_usec_ = Time::get_singleton()->get_ticks_usec();
    last_frame_usec_ = start_usec_;
    phase_ = Phase::Warmup;
    UtilityFunctions::print("Benchmark: busiest-board replay, seed ", seed_, ", ", warmup_seconds_, " s warm-up, ",
                            measure_seconds_, " s capture.");
    set_process(true);
}

void BenchmarkRunner::_exit_tree() {
    if (Viewport* vp = get_viewport()) {
        RenderingServer::get_singleton()->viewport_set_measure_render_time(vp->get_viewport_rid(), false);
    }
}

void BenchmarkRunner::restart_replay() {
    board_->setup(8, 8, 7, seed_);
    next_beat_ = 0;
    if (BoardView3D* v = view()) v->show_board(board_);
}

void BenchmarkRunner::play_next_beat() {
    BoardView3D* v = view();
    if (!v) return;
    if (next_beat_ >= replay_.actions.size()) {
        ++loops_;
        restart_replay();
    }
    const bench::Action& beat = replay_.actions[next_beat_++];
    ++beats_played_;
    emit_signal("bench_event", String(beat.label.c_str()), to_array(beat.cells));

    Dictionary result;
    switch (beat.kind) {
        case bench::Action::Kind::Swap:
            result = board_->try_swap(Vector2i(beat.a.x, beat.a.y), Vector2i(beat.b.x, beat.b.y));
            break;
        case bench::Action::Kind::Clear:
            result = board_->clear_cells(to_array(beat.cells));
            break;
        case bench::Action::Kind::Marker:
            hold_until_usec_ = Time::get_singleton()->get_ticks_usec() + kMarkerHoldUsec;
            return;
    }
    if (!diverged_ && hash_board(board_) != beat.board_hash) {
        diverged_ = true;
        ERR_PRINT("BenchmarkRunner: the board no longer matches the planned replay; results are not comparable.");
    }
    if (result.is_empty()) return;
    deep_cascades_played_ += beat.steps >= 4 ? 1 : 0;
    max_steps_played_ = std::max(max_steps_played_, beat.steps);
    v->play(result);
}

void BenchmarkRunner::sample_frame() {
    RenderingServer* rs = RenderingServer::get_singleton();
    Performance* perf = Performance::get_singleton();
    const RID vp = get_viewport()->get_viewport_rid();
    // Each value is the previous frame's, the newest the engine has.
    const double process_ms = perf->get_monitor(Performance::TIME_PROCESS) * 1000.0;
    const double render_cpu_ms = rs->get_frame_setup_time_cpu() + rs->viewport_get_measured_render_time_cpu(vp);
    cpu_ms_.push_back(process_ms + render_cpu_ms);
    gpu_ms_.push_back(rs->viewport_get_measured_render_time_gpu(vp));
    draw_calls_.push_back(perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME));
    primitives_.push_back(perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME));
    video_mem_max_ = std::max(video_mem_max_, perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED));
}

void BenchmarkRunner::_process(double) {
    if (phase_ == Phase::Idle || phase_ == Phase::Done) return;
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    const double frame_ms = seconds_between(last_frame_usec_, now) * 1000.0;
    last_frame_usec_ = now;

    if (phase_ == Phase::Warmup && seconds_between(start_usec_, now) >= warmup_seconds_) {
        phase_ = Phase::Measure;
        measure_start_usec_ = now;
        features_ = bench::active_features(get_viewport());
    } else if (phase_ == Phase::Measure) {
        frame_ms_.push_back(frame_ms);
        sample_frame();
        if (seconds_between(measure_start_usec_, now) >= measure_seconds_) {
            finish();
            return;
        }
    }

    BoardView3D* v = view();
    if (v && !v->is_busy() && now >= hold_until_usec_) play_next_beat();
}

void BenchmarkRunner::finish() {
    phase_ = Phase::Done;
    set_process(false);
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    const bool gpu_timing = std::any_of(gpu_ms_.begin(), gpu_ms_.end(), [](double v) { return v > 0.0; });

    Dictionary replay;
    replay["scenario"] = "busiest-board";
    replay["seed"] = seed_;
    replay["beats"] = beats_played_;
    replay["loops"] = loops_;
    replay["deep_cascades"] = deep_cascades_played_;
    replay["max_cascade_steps"] = max_steps_played_;
    replay["diverged"] = diverged_;

    const bench::FrameSummary draws = bench::summarize(draw_calls_, 0.0);
    const bench::FrameSummary prims = bench::summarize(primitives_, 0.0);
    Dictionary render;
    render["draw_calls_p95"] = draws.p95;
    render["draw_calls_max"] = draws.max;
    render["primitives_p95"] = prims.p95;
    render["primitives_max"] = prims.max;
    render["video_mem_max_mb"] = video_mem_max_ / (1024.0 * 1024.0);

    report_ = Dictionary();
    report_["schema"] = 1;
    report_["tier"] = tier_;
    report_["budget_ms"] = budget_ms_;
    report_["warmup_seconds"] = warmup_seconds_;
    report_["measure_seconds"] = seconds_between(measure_start_usec_, now);
    report_["device"] = bench::device_info(get_viewport());
    report_["gpu_timing"] = gpu_timing;
    report_["cpu_ms"] = to_dictionary(bench::summarize(cpu_ms_, budget_ms_));
    report_["gpu_ms"] = to_dictionary(bench::summarize(gpu_ms_, budget_ms_));
    report_["frame_ms"] = to_dictionary(bench::summarize(frame_ms_, budget_ms_));
    report_["render"] = render;
    report_["replay"] = replay;
    report_["features"] = features_;
    if (const QualityTiers* tiers = QualityTiers::get_singleton()) report_["quality_features"] = tiers->get_active_features();

    const String dir = output_path_.get_base_dir();
    if (!dir.is_empty()) DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
    Ref<FileAccess> file = FileAccess::open(output_path_, FileAccess::WRITE);
    if (file.is_null()) {
        ERR_PRINT("BenchmarkRunner: cannot write " + output_path_);
    } else {
        file->store_string(JSON::stringify(report_, "  ", false) + "\n");
        file->close();
    }
    // The report on one line too, for runs where the file is hard to reach
    // (a phone under remote debug shows the output in the editor).
    UtilityFunctions::print("BENCH_JSON ", JSON::stringify(report_, "", false));
    const Dictionary cpu = report_["cpu_ms"];
    const Dictionary gpu = report_["gpu_ms"];
    UtilityFunctions::print("Benchmark: wrote ", ProjectSettings::get_singleton()->globalize_path(output_path_),
                            " (cpu p95 ", cpu["p95"], " ms, gpu p95 ", gpu_timing ? Variant(gpu["p95"]) : Variant("n/a"),
                            gpu_timing ? " ms" : "", ", ", cpu["frames"], " frames)");
    emit_signal("finished", output_path_);
    if (quit_when_done_) get_tree()->quit(diverged_ ? 1 : 0);
}

void BenchmarkRunner::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_board_view_path", "path"), &BenchmarkRunner::set_board_view_path);
    ClassDB::bind_method(D_METHOD("get_board_view_path"), &BenchmarkRunner::get_board_view_path);
    ClassDB::bind_method(D_METHOD("set_seed", "seed"), &BenchmarkRunner::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &BenchmarkRunner::get_seed);
    ClassDB::bind_method(D_METHOD("set_warmup_seconds", "seconds"), &BenchmarkRunner::set_warmup_seconds);
    ClassDB::bind_method(D_METHOD("get_warmup_seconds"), &BenchmarkRunner::get_warmup_seconds);
    ClassDB::bind_method(D_METHOD("set_measure_seconds", "seconds"), &BenchmarkRunner::set_measure_seconds);
    ClassDB::bind_method(D_METHOD("get_measure_seconds"), &BenchmarkRunner::get_measure_seconds);
    ClassDB::bind_method(D_METHOD("set_output_path", "path"), &BenchmarkRunner::set_output_path);
    ClassDB::bind_method(D_METHOD("get_output_path"), &BenchmarkRunner::get_output_path);
    ClassDB::bind_method(D_METHOD("set_tier", "tier"), &BenchmarkRunner::set_tier);
    ClassDB::bind_method(D_METHOD("get_tier"), &BenchmarkRunner::get_tier);
    ClassDB::bind_method(D_METHOD("set_budget_ms", "ms"), &BenchmarkRunner::set_budget_ms);
    ClassDB::bind_method(D_METHOD("get_budget_ms"), &BenchmarkRunner::get_budget_ms);
    ClassDB::bind_method(D_METHOD("set_quit_when_done", "quit"), &BenchmarkRunner::set_quit_when_done);
    ClassDB::bind_method(D_METHOD("get_quit_when_done"), &BenchmarkRunner::get_quit_when_done);
    ClassDB::bind_method(D_METHOD("get_report"), &BenchmarkRunner::get_report);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "board_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BoardView3D"),
                 "set_board_view_path", "get_board_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "warmup_seconds"), "set_warmup_seconds", "get_warmup_seconds");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "measure_seconds"), "set_measure_seconds", "get_measure_seconds");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "output_path"), "set_output_path", "get_output_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "tier"), "set_tier", "get_tier");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "budget_ms"), "set_budget_ms", "get_budget_ms");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "quit_when_done"), "set_quit_when_done", "get_quit_when_done");
    ADD_SIGNAL(MethodInfo("bench_event", PropertyInfo(Variant::STRING, "label"), PropertyInfo(Variant::ARRAY, "cells")));
    ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::STRING, "path")));
}

}  // namespace montezuma::godot_binding
