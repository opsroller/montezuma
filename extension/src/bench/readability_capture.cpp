// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "readability_capture.hpp"

#include <algorithm>
#include <array>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "board/board_view_3d.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

struct EffectSet {
    const char* name;
    std::uint32_t on;
};

// The captures check_readability.py compares, in capture order.
constexpr std::array<EffectSet, 4> kSets{{
    {"all_on", bench::kAllEffects},
    {"all_off", 0},
    {"atmosphere_only", bench::kAtmosphere},
    {"glow_only", bench::kGlow},
}};

Array rect(Vector2 a, Vector2 b) {
    Array r;
    r.push_back(std::min(a.x, b.x));
    r.push_back(std::min(a.y, b.y));
    r.push_back(std::max(a.x, b.x));
    r.push_back(std::max(a.y, b.y));
    return r;
}

}  // namespace

BoardView3D* ReadabilityCapture::view() const {
    return board_view_path_.is_empty() ? nullptr : Object::cast_to<BoardView3D>(get_node_or_null(board_view_path_));
}

void ReadabilityCapture::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
    for (int64_t i = 0; i < args.size(); ++i) {
        const String a = args[i];
        if (a.begins_with("--capture-out=")) output_dir_ = a.get_slice("=", 1);
        if (a.begins_with("--capture-seed=")) seed_ = a.get_slice("=", 1).to_int();
    }
    BoardView3D* v = view();
    ERR_FAIL_NULL_MSG(v, "ReadabilityCapture: board_view_path does not point at a BoardView3D.");
    bench::hide_ui_overlays(get_tree()->get_root());
    board_.instantiate();
    board_->setup(8, 8, 7, seed_);
    v->show_board(board_);
    effects_.capture(get_tree()->get_root(), get_viewport());
    DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(output_dir_));
    running_ = true;
    set_process(true);
}

void ReadabilityCapture::fail(const String& why) {
    running_ = false;
    set_process(false);
    ERR_PRINT("ReadabilityCapture: " + why);
    get_tree()->quit(1);
}

void ReadabilityCapture::save_frame(const char* name) {
    const Ref<ViewportTexture> tex = get_viewport()->get_texture();
    const Ref<Image> image = tex.is_valid() ? tex->get_image() : Ref<Image>();
    if (image.is_null() || image->is_empty()) {
        fail("the viewport gave no image; run with a rendering driver, not --headless.");
        return;
    }
    const String path = output_dir_.path_join(String(name) + ".png");
    if (image->save_png(path) != OK) fail("cannot write " + path);
}

void ReadabilityCapture::write_layout() {
    BoardView3D* v = view();
    const Camera3D* cam = get_viewport()->get_camera_3d();
    if (!v || !cam) {
        fail("no board view or camera to project the cells with.");
        return;
    }
    const real_t half = static_cast<real_t>(v->get_cell_size() * 0.5);
    Array cells;
    Vector2 lo(1e9f, 1e9f);
    Vector2 hi(-1e9f, -1e9f);
    for (int y = 0; y < board_->get_height(); ++y) {
        for (int x = 0; x < board_->get_width(); ++x) {
            const Vector3 c = v->to_global(v->cell_position(Vector2i(x, y)));
            const Vector2 a = cam->unproject_position(c + Vector3(-half, 0, -half));
            const Vector2 b = cam->unproject_position(c + Vector3(half, 0, half));
            const Vector2 a2 = cam->unproject_position(c + Vector3(-half, 0, half));
            const Vector2 b2 = cam->unproject_position(c + Vector3(half, 0, -half));
            const Vector2 mn(std::min({a.x, b.x, a2.x, b2.x}), std::min({a.y, b.y, a2.y, b2.y}));
            const Vector2 mx(std::max({a.x, b.x, a2.x, b2.x}), std::max({a.y, b.y, a2.y, b2.y}));
            lo = Vector2(std::min(lo.x, mn.x), std::min(lo.y, mn.y));
            hi = Vector2(std::max(hi.x, mx.x), std::max(hi.y, mx.y));
            Dictionary d;
            d["x"] = x;
            d["y"] = y;
            d["kind"] = board_->get_cell(x, y);
            d["shade"] = (x + y) % 2;
            d["rect"] = rect(mn, mx);
            cells.push_back(d);
        }
    }
    const Vector2 size = get_viewport()->get_visible_rect().size;
    Array viewport_size;
    viewport_size.push_back(static_cast<int>(size.x));
    viewport_size.push_back(static_cast<int>(size.y));
    Array captures;
    for (const EffectSet& s : kSets) captures.push_back(String(s.name) + ".png");

    Dictionary layout;
    layout["schema"] = 1;
    layout["seed"] = seed_;
    layout["kinds"] = board_->get_kinds();
    layout["viewport"] = viewport_size;
    layout["board_rect"] = rect(lo, hi);
    layout["cell_px"] = (hi.x - lo.x) / static_cast<real_t>(board_->get_width());
    layout["cells"] = cells;
    layout["captures"] = captures;
    layout["features"] = bench::active_features(get_viewport());
    layout["device"] = bench::device_info(get_viewport());
    const String path = output_dir_.path_join("layout.json");
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) {
        fail("cannot write " + path);
        return;
    }
    file->store_string(JSON::stringify(layout, "  ", false) + "\n");
    file->close();
}

void ReadabilityCapture::_process(double) {
    if (!running_) return;
    if (frames_in_set_ == 0) effects_.apply(kSets[static_cast<std::size_t>(set_index_)].on);
    if (++frames_in_set_ < settle_frames_) return;
    save_frame(kSets[static_cast<std::size_t>(set_index_)].name);
    if (!running_) return;
    frames_in_set_ = 0;
    if (++set_index_ < static_cast<int>(kSets.size())) return;

    // The features the scene runs with, recorded with every effect back on.
    effects_.restore();
    write_layout();
    if (!running_) return;
    running_ = false;
    set_process(false);
    UtilityFunctions::print("ReadabilityCapture: wrote ", static_cast<int64_t>(kSets.size()),
                            " captures and layout.json to ", ProjectSettings::get_singleton()->globalize_path(output_dir_));
    get_tree()->quit(0);
}

void ReadabilityCapture::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_board_view_path", "path"), &ReadabilityCapture::set_board_view_path);
    ClassDB::bind_method(D_METHOD("get_board_view_path"), &ReadabilityCapture::get_board_view_path);
    ClassDB::bind_method(D_METHOD("set_seed", "seed"), &ReadabilityCapture::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &ReadabilityCapture::get_seed);
    ClassDB::bind_method(D_METHOD("set_output_dir", "dir"), &ReadabilityCapture::set_output_dir);
    ClassDB::bind_method(D_METHOD("get_output_dir"), &ReadabilityCapture::get_output_dir);
    ClassDB::bind_method(D_METHOD("set_settle_frames", "frames"), &ReadabilityCapture::set_settle_frames);
    ClassDB::bind_method(D_METHOD("get_settle_frames"), &ReadabilityCapture::get_settle_frames);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "board_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BoardView3D"),
                 "set_board_view_path", "get_board_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "output_dir"), "set_output_dir", "get_output_dir");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "settle_frames"), "set_settle_frames", "get_settle_frames");
}

}  // namespace montezuma::godot_binding
