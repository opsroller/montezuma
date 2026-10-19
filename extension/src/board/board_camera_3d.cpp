// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "board_camera_3d.hpp"

#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "board_layout.hpp"
#include "board_view_3d.hpp"
#include "feel.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Tallest piece above the board plane, in cells, including the selection lift.
constexpr double kPieceTopCells = 0.6;

// Metadata a theme sets on its CameraAttributesPractical: where the far depth
// of field starts, in metres beyond the board's far edge
// (docs/reference/board-themes.md, "Post-processing").
const char* const kDofBeyondBoardMeta = "dof_far_beyond_board";

}  // namespace

void BoardCamera3D::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    set_fov(static_cast<real_t>(feel::kCameraFovDegrees));
    get_viewport()->connect("size_changed", callable_mp(this, &BoardCamera3D::refit));
    if (BoardView3D* view = board_view()) view->connect("layout_changed", callable_mp(this, &BoardCamera3D::refit));
    refit();
}

void BoardCamera3D::_exit_tree() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    const Callable cb = callable_mp(this, &BoardCamera3D::refit);
    if (Viewport* vp = get_viewport(); vp && vp->is_connected("size_changed", cb)) vp->disconnect("size_changed", cb);
    if (BoardView3D* view = board_view(); view && view->is_connected("layout_changed", cb)) {
        view->disconnect("layout_changed", cb);
    }
}

BoardView3D* BoardCamera3D::board_view() const {
    if (board_view_path_.is_empty() || !is_inside_tree()) return nullptr;
    return Object::cast_to<BoardView3D>(get_node_or_null(board_view_path_));
}

void BoardCamera3D::set_board_view_path(const NodePath& p) { board_view_path_ = p; }

void BoardCamera3D::set_tilt_degrees(double v) {
    tilt_degrees_ = Math::clamp(v, 0.0, 60.0);
    if (is_inside_tree()) refit();
}

void BoardCamera3D::set_margin(double v) {
    margin_ = Math::clamp(v, 0.0, 0.4);
    if (is_inside_tree()) refit();
}

void BoardCamera3D::refit() {
    BoardView3D* view = board_view();
    if (!view) return;
    const Vector2 size = get_viewport()->get_visible_rect().size;
    if (size.y <= 0.0f) return;
    const double cell = view->get_cell_size();
    layout::Fit fit;
    // The drawn board's reach past the outer cells (frame, slab edge).
    const double pad = view->get_board_padding();
    fit.half_width = (view->get_board_width() * 0.5 + pad) * cell;
    fit.half_depth = (view->get_board_height() * 0.5 + pad) * cell;
    fit.height = kPieceTopCells * cell;
    fit.tilt_degrees = tilt_degrees_;
    fit.vfov_degrees = get_fov();  // Camera3D keeps height by default, so fov is vertical
    fit.aspect = size.x / size.y;
    fit.margin = margin_;
    const double distance = layout::fit_distance(fit);
    const layout::Vec3 dir = layout::camera_direction(tilt_degrees_);
    const Vector3 target = view->get_global_position();
    const Vector3 eye = target + Vector3(static_cast<real_t>(dir.x), static_cast<real_t>(dir.y),
                                         static_cast<real_t>(dir.z)) *
                                     static_cast<real_t>(distance);
    // Straight down needs another up vector than the usual +Y.
    const Vector3 up = Math::is_zero_approx(tilt_degrees_) ? Vector3(0, 0, -1) : Vector3(0, 1, 0);
    look_at_from_position(eye, target, up);
    fit_depth_of_field(layout::far_depth(fit, distance));
}

void BoardCamera3D::fit_depth_of_field(double board_far_depth) {
    // The camera's own attributes win over the environment's, as in Godot.
    Ref<CameraAttributesPractical> attrs = get_attributes();
    if (attrs.is_null()) {
        const Ref<World3D> world = get_world_3d();
        if (world.is_valid()) attrs = world->get_camera_attributes();
    }
    if (attrs.is_null() || !attrs->has_meta(kDofBeyondBoardMeta)) return;
    const double beyond = attrs->get_meta(kDofBeyondBoardMeta);
    attrs->set_dof_blur_far_distance(static_cast<real_t>(board_far_depth + beyond));
}

void BoardCamera3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_board_view_path", "path"), &BoardCamera3D::set_board_view_path);
    ClassDB::bind_method(D_METHOD("get_board_view_path"), &BoardCamera3D::get_board_view_path);
    ClassDB::bind_method(D_METHOD("set_tilt_degrees", "degrees"), &BoardCamera3D::set_tilt_degrees);
    ClassDB::bind_method(D_METHOD("get_tilt_degrees"), &BoardCamera3D::get_tilt_degrees);
    ClassDB::bind_method(D_METHOD("set_margin", "fraction"), &BoardCamera3D::set_margin);
    ClassDB::bind_method(D_METHOD("get_margin"), &BoardCamera3D::get_margin);
    ClassDB::bind_method(D_METHOD("refit"), &BoardCamera3D::refit);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "board_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BoardView3D"),
                 "set_board_view_path", "get_board_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tilt_degrees", PROPERTY_HINT_RANGE, "0,60,0.5"), "set_tilt_degrees",
                 "get_tilt_degrees");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_RANGE, "0,0.4,0.01"), "set_margin",
                 "get_margin");
}

}  // namespace montezuma::godot_binding
