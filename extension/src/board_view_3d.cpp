// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "board_view_3d.hpp"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/prism_mesh.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Kind order matches docs/rules-spec.md: red, orange, yellow, green, blue, purple, white.
const Color kKindColors[] = {
    Color(0.80f, 0.15f, 0.12f), Color(0.92f, 0.50f, 0.10f), Color(0.95f, 0.80f, 0.20f), Color(0.20f, 0.65f, 0.25f),
    Color(0.15f, 0.40f, 0.85f), Color(0.55f, 0.25f, 0.70f), Color(0.92f, 0.90f, 0.85f),
};

real_t to_real(int v) { return static_cast<real_t>(v); }

const Vector3 kSelectedScale(1.2f, 1.2f, 1.2f);

}  // namespace

void BoardView3D::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    board_.instantiate();
    board_->setup(board_width_, board_height_, kKinds, seed_);
    // setup() keeps the default board on bad input; draw whatever it holds.
    board_width_ = board_->get_width();
    board_height_ = board_->get_height();
    build_meshes();
    build_slab();
    pieces_.assign(board_width_ * board_height_, nullptr);
    shown_kinds_.assign(board_width_ * board_height_, -1);
    for (int y = 0; y < board_height_; ++y) {
        for (int x = 0; x < board_width_; ++x) {
            auto* piece = memnew(MeshInstance3D);
            piece->set_position(cell_position(x, y));
            add_child(piece);
            pieces_[y * board_width_ + x] = piece;
        }
    }
    sync_pieces();
}

Vector3 BoardView3D::cell_position(int x, int y) const {
    return Vector3((to_real(x) - to_real(board_width_ - 1) * 0.5f) * cell_size_, 0.0f,
                   (to_real(y) - to_real(board_height_ - 1) * 0.5f) * cell_size_);
}

MeshInstance3D* BoardView3D::piece_at(Vector2i c) const { return pieces_[c.y * board_width_ + c.x]; }

void BoardView3D::build_meshes() {
    const real_t s = cell_size_ * 0.7f;
    Ref<SphereMesh> sphere;
    sphere.instantiate();
    sphere->set_radius(s * 0.5f);
    sphere->set_height(s);
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(s * 0.8f, s * 0.8f, s * 0.8f));
    Ref<CylinderMesh> cylinder;
    cylinder.instantiate();
    cylinder->set_top_radius(s * 0.45f);
    cylinder->set_bottom_radius(s * 0.45f);
    cylinder->set_height(s * 0.6f);
    Ref<PrismMesh> prism;
    prism.instantiate();
    prism->set_size(Vector3(s, s * 0.8f, s * 0.6f));
    Ref<TorusMesh> torus;
    torus.instantiate();
    torus->set_inner_radius(s * 0.18f);
    torus->set_outer_radius(s * 0.45f);
    Ref<CapsuleMesh> capsule;
    capsule.instantiate();
    capsule->set_radius(s * 0.3f);
    capsule->set_height(s);
    Ref<CylinderMesh> cone;
    cone.instantiate();
    cone->set_top_radius(0.0f);
    cone->set_bottom_radius(s * 0.45f);
    cone->set_height(s * 0.8f);

    meshes_[0] = sphere;
    meshes_[1] = box;
    meshes_[2] = cylinder;
    meshes_[3] = prism;
    meshes_[4] = torus;
    meshes_[5] = capsule;
    meshes_[6] = cone;
    for (int k = 0; k < kKinds; ++k) {
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(kKindColors[k]);
        mat->set_roughness(0.45f);
        mat->set_metallic(k == 2 ? 0.8f : 0.1f);  // gold reads as gold
        materials_[k] = mat;
    }
}

void BoardView3D::build_slab() {
    Ref<BoxMesh> slab;
    slab.instantiate();
    slab->set_size(Vector3(to_real(board_width_) * cell_size_ + 0.4f, 0.3f, to_real(board_height_) * cell_size_ + 0.4f));
    Ref<StandardMaterial3D> stone;
    stone.instantiate();
    stone->set_albedo(Color(0.42f, 0.38f, 0.32f));
    stone->set_roughness(0.95f);
    auto* node = memnew(MeshInstance3D);
    node->set_mesh(slab);
    node->set_surface_override_material(0, stone);
    node->set_position(Vector3(0, -0.5f * cell_size_, 0));
    add_child(node);
}

void BoardView3D::sync_pieces() {
    for (int y = 0; y < board_height_; ++y) {
        for (int x = 0; x < board_width_; ++x) {
            const int idx = y * board_width_ + x;
            const int kind = board_->get_cell(x, y);
            auto* piece = pieces_[idx];
            piece->set_position(cell_position(x, y));
            piece->set_scale(Vector3(1, 1, 1));
            if (kind == shown_kinds_[idx]) continue;
            shown_kinds_[idx] = kind;
            if (kind < 0 || kind >= kKinds) {
                piece->set_mesh(Ref<Mesh>());
                continue;
            }
            piece->set_mesh(meshes_[kind]);
            piece->set_surface_override_material(0, materials_[kind]);
            // New or changed pieces drop in from above, like a refill.
            piece->set_position(cell_position(x, y) + Vector3(0, cell_size_ * 1.5f, 0));
            Ref<Tween> tween = create_tween();
            tween->tween_property(piece, "position", cell_position(x, y), kSettleSeconds);
        }
    }
}

bool BoardView3D::pick_cell(Vector2 screen_pos, Vector2i& out) const {
    Camera3D* camera = get_viewport()->get_camera_3d();
    if (!camera) return false;
    const Vector3 origin = camera->project_ray_origin(screen_pos);
    const Vector3 dir = camera->project_ray_normal(screen_pos);
    const real_t plane_y = get_global_position().y;
    if (Math::is_zero_approx(dir.y)) return false;
    const real_t t = (plane_y - origin.y) / dir.y;
    if (t < 0) return false;
    const Vector3 local = to_local(origin + dir * t);
    const int x = static_cast<int>(Math::floor(local.x / cell_size_ + to_real(board_width_) * 0.5f));
    const int y = static_cast<int>(Math::floor(local.z / cell_size_ + to_real(board_height_) * 0.5f));
    if (x < 0 || y < 0 || x >= board_width_ || y >= board_height_) return false;
    out = Vector2i(x, y);
    return true;
}

void BoardView3D::_unhandled_input(const Ref<InputEvent>& event) {
    if (Ref<InputEventMouseButton> mb = event; mb.is_valid()) {
        if (mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) handle_press(mb->get_position());
    } else if (Ref<InputEventScreenTouch> touch = event; touch.is_valid()) {
        if (touch->is_pressed()) handle_press(touch->get_position());
    }
}

void BoardView3D::handle_press(Vector2 screen_pos) {
    if (animating_ || !board_.is_valid()) return;
    Vector2i cell;
    if (!pick_cell(screen_pos, cell)) {
        clear_selection();
        return;
    }
    if (!has_selection_) {
        select(cell);
        return;
    }
    const Vector2i from = selected_;
    clear_selection();
    if (from == cell) return;
    if (Math::abs(from.x - cell.x) + Math::abs(from.y - cell.y) != 1) {
        select(cell);
        return;
    }
    request_swap(from, cell);
}

void BoardView3D::select(Vector2i cell) {
    has_selection_ = true;
    selected_ = cell;
    piece_at(cell)->set_scale(kSelectedScale);
}

void BoardView3D::clear_selection() {
    if (has_selection_) piece_at(selected_)->set_scale(Vector3(1, 1, 1));
    has_selection_ = false;
}

bool BoardView3D::request_swap(Vector2i a, Vector2i b) {
    if (animating_ || !board_.is_valid()) return false;
    auto* pa = piece_at(a);
    auto* pb = piece_at(b);
    const Vector3 pos_a = cell_position(a.x, a.y);
    const Vector3 pos_b = cell_position(b.x, b.y);
    animating_ = true;
    Ref<Tween> tween = create_tween();
    tween->set_parallel(true);
    tween->tween_property(pa, "position", pos_b, kSwapSeconds);
    tween->tween_property(pb, "position", pos_a, kSwapSeconds);
    tween->set_parallel(false);

    Dictionary result = board_->try_swap(a, b);
    if (result.is_empty()) {
        // Illegal swap bounces back, as in the original.
        tween->set_parallel(true);
        tween->tween_property(pa, "position", pos_a, kSwapSeconds);
        tween->tween_property(pb, "position", pos_b, kSwapSeconds);
        tween->set_parallel(false);
        tween->tween_callback(callable_mp(this, &BoardView3D::finish_animation));
        return false;
    }
    score_ += static_cast<int>(result["score"]);
    if (static_cast<bool>(result["stuck"])) {
        // No playable arrangement exists; deal a new board so play continues.
        board_->setup(board_width_, board_height_, kKinds, ++seed_);
    }
    // The view mirrors pieces by cell, so the swapped meshes trade owners.
    std::swap(shown_kinds_[a.y * board_width_ + a.x], shown_kinds_[b.y * board_width_ + b.x]);
    std::swap(pieces_[a.y * board_width_ + a.x], pieces_[b.y * board_width_ + b.x]);
    tween->tween_callback(callable_mp(this, &BoardView3D::sync_pieces));
    tween->tween_interval(kSettleSeconds);
    pending_result_ = result;
    tween->tween_callback(callable_mp(this, &BoardView3D::finish_animation));
    return true;
}

void BoardView3D::finish_animation() {
    animating_ = false;
    // Fire once the move is on screen, so HUD and audio stay in sync with it.
    if (!pending_result_.is_empty()) {
        Dictionary result = pending_result_;
        pending_result_ = Dictionary();
        emit_signal("move_resolved", result, score_);
    }
}

void BoardView3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_board_width", "value"), &BoardView3D::set_board_width);
    ClassDB::bind_method(D_METHOD("get_board_width"), &BoardView3D::get_board_width);
    ClassDB::bind_method(D_METHOD("set_board_height", "value"), &BoardView3D::set_board_height);
    ClassDB::bind_method(D_METHOD("get_board_height"), &BoardView3D::get_board_height);
    ClassDB::bind_method(D_METHOD("set_seed", "value"), &BoardView3D::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &BoardView3D::get_seed);
    ClassDB::bind_method(D_METHOD("set_cell_size", "value"), &BoardView3D::set_cell_size);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &BoardView3D::get_cell_size);
    ClassDB::bind_method(D_METHOD("get_board"), &BoardView3D::get_board);
    ClassDB::bind_method(D_METHOD("get_score"), &BoardView3D::get_score);
    ClassDB::bind_method(D_METHOD("request_swap", "a", "b"), &BoardView3D::request_swap);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "board_width", PROPERTY_HINT_RANGE, "3,16"), "set_board_width",
                 "get_board_width");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "board_height", PROPERTY_HINT_RANGE, "3,16"), "set_board_height",
                 "get_board_height");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.25,4,0.05"), "set_cell_size",
                 "get_cell_size");
    ADD_SIGNAL(MethodInfo("move_resolved", PropertyInfo(Variant::DICTIONARY, "result"),
                          PropertyInfo(Variant::INT, "total_score")));
}

}  // namespace montezuma::godot_binding
