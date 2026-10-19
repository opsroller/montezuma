// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "benchmark_runner.hpp"

#include <algorithm>
#include <cmath>
#include <exception>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
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
#include <godot_cpp/classes/viewport_texture.hpp>
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

Dictionary to_dictionary(const bench::BudgetVerdict& v, const bench::BudgetSpec& spec) {
    Dictionary d;
    d["metric"] = "frame_ms";
    d["target_hz"] = spec.target_hz;
    d["period_ms"] = v.period_ms;
    d["tolerance"] = spec.tolerance;
    Array limits;
    for (const bench::BudgetLimit& l : v.limits) {
        Dictionary e;
        e["percentile"] = l.percentile;
        e["budget_ms"] = l.budget_ms;
        e["limit_ms"] = l.limit_ms;
        e["value_ms"] = l.value_ms;
        e["pass"] = l.pass;
        limits.push_back(e);
    }
    d["limits"] = limits;
    d["pass"] = v.pass;
    d["reason"] = String::utf8(v.reason.c_str());
    return d;
}

Array to_array(bench::Size2 s) {
    Array a;
    a.push_back(s.w);
    a.push_back(s.h);
    return a;
}

String vsync_name(DisplayServer::VSyncMode m) {
    switch (m) {
        case DisplayServer::VSYNC_DISABLED: return "disabled";
        case DisplayServer::VSYNC_ENABLED: return "enabled";
        case DisplayServer::VSYNC_ADAPTIVE: return "adaptive";
        case DisplayServer::VSYNC_MAILBOX: return "mailbox";
        default: return "other";
    }
}

Array to_array(const std::vector<Cell>& cells) {
    Array out;
    for (const Cell c : cells) out.push_back(Vector2i(c.x, c.y));
    return out;
}

// Pipeline-compilation sources in the order the report lists them.
constexpr std::array<RenderingServer::RenderingInfo, 5> kPipelineInfos = {
    RenderingServer::RENDERING_INFO_PIPELINE_COMPILATIONS_CANVAS,
    RenderingServer::RENDERING_INFO_PIPELINE_COMPILATIONS_MESH,
    RenderingServer::RENDERING_INFO_PIPELINE_COMPILATIONS_SURFACE,
    RenderingServer::RENDERING_INFO_PIPELINE_COMPILATIONS_DRAW,
    RenderingServer::RENDERING_INFO_PIPELINE_COMPILATIONS_SPECIALIZATION,
};
constexpr std::array<const char*, 5> kPipelineNames = {"canvas", "mesh", "surface", "draw", "specialization"};

std::array<uint64_t, 5> pipeline_counts() {
    RenderingServer* rs = RenderingServer::get_singleton();
    std::array<uint64_t, 5> out{};
    for (std::size_t i = 0; i < kPipelineInfos.size(); ++i) out[i] = rs->get_rendering_info(kPipelineInfos[i]);
    return out;
}

Dictionary pipeline_delta(const std::array<uint64_t, 5>& from, const std::array<uint64_t, 5>& to) {
    Dictionary d;
    for (std::size_t i = 0; i < kPipelineNames.size(); ++i)
        d[kPipelineNames[i]] = static_cast<int64_t>(to[i] >= from[i] ? to[i] - from[i] : 0);
    return d;
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
        if (a.begins_with("--bench-tolerance=")) {
            tolerance_arg_invalid_ = !value.is_valid_float();
            tolerance_ = value.to_float();
        }
        if (a.begins_with("--bench-size=")) {
            const auto size = bench::parse_size(value.utf8().get_data());
            size_arg_invalid_ = !size;
            render_size_ = size ? Vector2i(size->w, size->h) : Vector2i();
        }
        if (a == "--bench-vsync=off") disable_vsync_ = true;
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
    // Visual target: p95 within one 60 Hz period, two on the web tier;
    // budget_ms is that p95 budget before the tolerance.
    const bench::BudgetSpec spec = bench::budget_for_tier(tier_.utf8().get_data(), tolerance_, budget_ms_);
    tolerance_ = spec.tolerance;
    budget_ms_ = spec.rules.empty() ? 0.0 : spec.rules.front().budget_ms;
    if (size_arg_invalid_ || tolerance_arg_invalid_) {
        ERR_PRINT("BenchmarkRunner: --bench-size must look like 1920x1080 and --bench-tolerance be a number.");
        if (quit_when_done_) get_tree()->quit(1);
        return;
    }
    lock_render_size();
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
    for (auto* samples : {&process_ms_, &render_cpu_ms_, &cpu_ms_, &gpu_ms_, &frame_ms_, &draw_calls_, &scene_draw_calls_, &shadow_draw_calls_, &canvas_draw_calls_, &primitives_}) samples->reserve(frames);

    RenderingServer::get_singleton()->viewport_set_measure_render_time(get_viewport()->get_viewport_rid(), true);
    bench::hide_ui_overlays(get_tree()->get_root());
    // Last in the process pass, so process_ms covers every other node's
    // _process this frame (TIME_PROCESS is a once-a-second maximum).
    set_process_priority(1 << 30);
    get_tree()->connect("process_frame", callable_mp(this, &BenchmarkRunner::on_process_frame));
    pipelines_at_start_ = pipeline_counts();
    board_.instantiate();
    restart_replay();
    start_usec_ = Time::get_singleton()->get_ticks_usec();
    last_frame_usec_ = start_usec_;
    phase_ = Phase::Warmup;
    UtilityFunctions::print("Benchmark: busiest-board replay, seed ", seed_, ", ", warmup_seconds_, " s warm-up, ",
                            measure_seconds_, " s capture.");
    set_process(true);
}

// The run measures one render size: the root viewport renders at the
// requested size (content scale mode viewport) whatever the OS does to the
// window, and the window may not be resized. sample_frame() checks it held.
void BenchmarkRunner::lock_render_size() {
    Window* root = get_tree()->get_root();
    DisplayServer* ds = DisplayServer::get_singleton();
    if (disable_vsync_) ds->window_set_vsync_mode(DisplayServer::VSYNC_DISABLED);
    const bool requested = render_size_.x > 0 && render_size_.y > 0;
    sizes_ = bench::SizeTracker(requested ? std::optional(bench::Size2{render_size_.x, render_size_.y}) : std::nullopt);
    // The window is followed too: the lock keeps the render size, but a
    // resized window still changes the scale and present cost.
    window_sizes_ = bench::SizeTracker(std::nullopt);
    if (!requested) return;
    root->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    root->set_size(render_size_);
    root->set_content_scale_size(render_size_);
    root->set_content_scale_aspect(Window::CONTENT_SCALE_ASPECT_IGNORE);
    root->set_content_scale_mode(Window::CONTENT_SCALE_MODE_VIEWPORT);
}

Vector2i BenchmarkRunner::current_render_size() const {
    const Ref<ViewportTexture> texture = get_viewport()->get_texture();
    return texture.is_valid() ? Vector2i(texture->get_size()) : Vector2i();
}

void BenchmarkRunner::on_process_frame() { process_start_usec_ = Time::get_singleton()->get_ticks_usec(); }

void BenchmarkRunner::_exit_tree() {
    const Callable stamp = callable_mp(this, &BenchmarkRunner::on_process_frame);
    if (get_tree()->is_connected("process_frame", stamp)) get_tree()->disconnect("process_frame", stamp);
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
    // The render values are the previous frame's, the newest the engine has.
    const double render_cpu_ms = rs->get_frame_setup_time_cpu() + rs->viewport_get_measured_render_time_cpu(vp);
    render_cpu_ms_.push_back(render_cpu_ms);
    cpu_ms_.push_back(render_cpu_ms);  // process_ms is added when the frame's process pass ends
    const Vector2i size = current_render_size();
    sizes_.add({size.x, size.y});
    const Vector2i window = DisplayServer::get_singleton()->window_get_size();
    window_sizes_.add({window.x, window.y});
    gpu_ms_.push_back(rs->viewport_get_measured_render_time_gpu(vp));
    draw_calls_.push_back(perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME));
    const auto draws_in = [&](RenderingServer::ViewportRenderInfoType type) {
        return double(rs->viewport_get_render_info(vp, type, RenderingServer::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME));
    };
    scene_draw_calls_.push_back(draws_in(RenderingServer::VIEWPORT_RENDER_INFO_TYPE_VISIBLE));
    shadow_draw_calls_.push_back(draws_in(RenderingServer::VIEWPORT_RENDER_INFO_TYPE_SHADOW));
    canvas_draw_calls_.push_back(draws_in(RenderingServer::VIEWPORT_RENDER_INFO_TYPE_CANVAS));
    primitives_.push_back(perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME));
    video_mem_max_ = std::max(video_mem_max_, perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED));
}

void BenchmarkRunner::_process(double) {
    if (phase_ == Phase::Idle || phase_ == Phase::Done) return;
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    const double frame_ms = seconds_between(last_frame_usec_, now) * 1000.0;
    last_frame_usec_ = now;

    bool measured = false;
    if (phase_ == Phase::Warmup && seconds_between(start_usec_, now) >= warmup_seconds_) {
        phase_ = Phase::Measure;
        measure_start_usec_ = now;
        pipelines_at_capture_ = pipeline_counts();
        features_ = bench::active_features(get_viewport());
        DisplayServer* ds = DisplayServer::get_singleton();
        // -1 when unknown (headless); some virtual displays report NaN.
        display_hz_ = ds->screen_get_refresh_rate(ds->window_get_current_screen());
        if (!std::isfinite(display_hz_) || display_hz_ <= 0.0) display_hz_ = -1.0;
        vsync_ = vsync_name(ds->window_get_vsync_mode());
    } else if (phase_ == Phase::Measure) {
        frame_ms_.push_back(frame_ms);
        sample_frame();
        measured = true;
        if (seconds_between(measure_start_usec_, now) >= measure_seconds_) {
            push_process_sample();
            finish();
            return;
        }
    }

    BoardView3D* v = view();
    if (v && !v->is_busy() && now >= hold_until_usec_) play_next_beat();
    if (measured) push_process_sample();
}

// This frame's process pass so far: every node's _process from
// SceneTree.process_frame to here, the replay's own beat included.
void BenchmarkRunner::push_process_sample() {
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    const double ms = process_start_usec_ > 0 && process_start_usec_ <= now
                          ? seconds_between(process_start_usec_, now) * 1000.0
                          : 0.0;
    process_ms_.push_back(ms);
    if (!cpu_ms_.empty()) cpu_ms_.back() += ms;
}

void BenchmarkRunner::finish() {
    phase_ = Phase::Done;
    set_process(false);
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    const std::vector<double> gpu_samples = bench::positive_only(gpu_ms_);
    const bool gpu_timing = !gpu_samples.empty();

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
    Dictionary passes;
    passes["scene_p95"] = bench::summarize(scene_draw_calls_, 0.0).p95;
    passes["shadow_p95"] = bench::summarize(shadow_draw_calls_, 0.0).p95;
    passes["canvas_p95"] = bench::summarize(canvas_draw_calls_, 0.0).p95;
    render["draw_calls_by_pass"] = passes;
    // How the board drew its pieces at the end (batching: board-view.md).
    if (const BoardView3D* v = view()) render["piece_draws"] = v->get_piece_draws();
    render["primitives_p95"] = prims.p95;
    render["primitives_max"] = prims.max;
    render["video_mem_max_mb"] = video_mem_max_ / (1024.0 * 1024.0);
    // Pipelines the renderer compiled, by source: "draw" ones stalled the
    // frame that needed them; any in the capture were not warmed up.
    const std::array<uint64_t, 5> pipelines_now = pipeline_counts();
    Dictionary pipelines;
    pipelines["before_capture"] = pipeline_delta(pipelines_at_start_, pipelines_at_capture_);
    pipelines["during_capture"] = pipeline_delta(pipelines_at_capture_, pipelines_now);
    render["pipeline_compilations"] = pipelines;

    // Frames not paced by the display cannot be display-limited.
    const bench::BudgetSpec spec = bench::budget_for_tier(tier_.utf8().get_data(), tolerance_, budget_ms_);
    const double limit_ms = bench::p95_limit_ms(spec);
    const bench::FrameSummary frames = bench::summarize(frame_ms_, limit_ms);
    const Dictionary device = bench::device_info(get_viewport());
    String unmeasured;
    if (bool(device["headless"])) unmeasured = "headless, nothing rendered";
    else if (bool(device["software"])) unmeasured = "software renderer (" + String(device["adapter"]) + ")";
    const bench::BudgetVerdict verdict = bench::judge_budget(frames, spec, vsync_ == "disabled" ? -1.0 : display_hz_,
                                                             unmeasured.utf8().get_data());

    Dictionary resolution;
    resolution["requested"] = sizes_.requested ? Variant(to_array(*sizes_.requested)) : Variant();
    resolution["locked"] = sizes_.requested.has_value();
    resolution["render"] = to_array(sizes_.first);
    resolution["min"] = to_array(sizes_.min);
    resolution["max"] = to_array(sizes_.max);
    resolution["mismatched_frames"] = static_cast<int64_t>(sizes_.mismatched);
    Dictionary window;
    window["first"] = to_array(window_sizes_.first);
    window["min"] = to_array(window_sizes_.min);
    window["max"] = to_array(window_sizes_.max);
    window["changed_frames"] = static_cast<int64_t>(window_sizes_.mismatched);
    resolution["window"] = window;
    const bool size_held = sizes_.pass() && window_sizes_.pass();
    resolution["pass"] = size_held;

    Dictionary display;
    display["refresh_hz"] = display_hz_;
    display["vsync"] = vsync_;

    const String driver = RenderingServer::get_singleton()->get_current_rendering_driver_name();
    Dictionary timing;
    timing["gpu"] = gpu_timing;
    timing["gpu_note"] = gpu_timing ? String()
                         : bool(device["headless"])
                             ? String("headless: no GPU work, so no GPU timestamps; gpu_ms is null")
                             : "no GPU timestamps from the " + driver + " driver; gpu_ms is null";
    timing["gpu_frames"] = static_cast<int64_t>(gpu_samples.size());
    // Godot's Metal driver waits for the next drawable inside the measured
    // render CPU time, so with vsync on it fills up to the display period.
    timing["render_cpu_includes_display_wait"] = driver == "metal";

    report_ = Dictionary();
    report_["schema"] = 2;
    report_["tier"] = tier_;
    report_["budget_ms"] = budget_ms_;
    report_["budget"] = to_dictionary(verdict, spec);
    report_["resolution"] = resolution;
    report_["display"] = display;
    report_["timing"] = timing;
    report_["warmup_seconds"] = warmup_seconds_;
    report_["measure_seconds"] = seconds_between(measure_start_usec_, now);
    report_["device"] = device;
    report_["gpu_timing"] = gpu_timing;
    report_["process_ms"] = to_dictionary(bench::summarize(process_ms_, limit_ms));
    report_["render_cpu_ms"] = to_dictionary(bench::summarize(render_cpu_ms_, limit_ms));
    report_["cpu_ms"] = to_dictionary(bench::summarize(cpu_ms_, limit_ms));
    report_["gpu_ms"] = gpu_timing ? Variant(to_dictionary(bench::summarize(gpu_samples, limit_ms))) : Variant();
    report_["frame_ms"] = to_dictionary(frames);
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
    const Dictionary gpu = gpu_timing ? Dictionary(report_["gpu_ms"]) : Dictionary();
    UtilityFunctions::print("Benchmark: wrote ", ProjectSettings::get_singleton()->globalize_path(output_path_), " (frame p95 ",
                            frames.p95, " ms, budget ", verdict.pass ? String("pass") : String::utf8(verdict.reason.c_str()),
                            "; gpu p95 ", gpu_timing ? Variant(gpu["p95"]) : Variant("n/a"), gpu_timing ? " ms" : "", ", ",
                            static_cast<int64_t>(frames.frames), " frames)");
    if (!size_held) {
        ERR_PRINT(vformat("BenchmarkRunner: the render size missed the request in %d of %d frames, the window changed "
                          "size in %d; results are not comparable.",
                          static_cast<int64_t>(sizes_.mismatched), static_cast<int64_t>(sizes_.frames),
                          static_cast<int64_t>(window_sizes_.mismatched)));
    }
    emit_signal("finished", output_path_);
    // A run that drifted or lost its render size measured something else.
    if (quit_when_done_) get_tree()->quit(diverged_ || !size_held ? 1 : 0);
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
    ClassDB::bind_method(D_METHOD("set_tolerance", "fraction"), &BenchmarkRunner::set_tolerance);
    ClassDB::bind_method(D_METHOD("get_tolerance"), &BenchmarkRunner::get_tolerance);
    ClassDB::bind_method(D_METHOD("set_render_size", "size"), &BenchmarkRunner::set_render_size);
    ClassDB::bind_method(D_METHOD("get_render_size"), &BenchmarkRunner::get_render_size);
    ClassDB::bind_method(D_METHOD("set_disable_vsync", "disable"), &BenchmarkRunner::set_disable_vsync);
    ClassDB::bind_method(D_METHOD("get_disable_vsync"), &BenchmarkRunner::get_disable_vsync);
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
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tolerance", PROPERTY_HINT_RANGE, "0,0.5,0.01"), "set_tolerance",
                 "get_tolerance");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "render_size"), "set_render_size", "get_render_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_vsync"), "set_disable_vsync", "get_disable_vsync");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "quit_when_done"), "set_quit_when_done", "get_quit_when_done");
    ADD_SIGNAL(MethodInfo("bench_event", PropertyInfo(Variant::STRING, "label"), PropertyInfo(Variant::ARRAY, "cells")));
    ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::STRING, "path")));
}

}  // namespace montezuma::godot_binding
