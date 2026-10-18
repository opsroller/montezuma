// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "board_view_3d.hpp"

#include <utility>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "animator.hpp"
#include "feel.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

real_t r(double v) { return static_cast<real_t>(v); }

const Vector2i kNoCell(-1, -1);

anim::Cell to_cell(const Variant& v) {
    const Vector2i c = v;
    return {c.x, c.y};
}

// Reads the cell-level events of a try_swap result (contract "MoveResult
// events"). False when the result does not carry them yet.
bool parse_events(const Dictionary& result, anim::Events& ev) {
    if (!result.has("cascades")) return false;
    if (result.has("swap")) {
        const Array swap = result["swap"];
        if (swap.size() == 2) ev.swap = std::array<anim::Cell, 2>{to_cell(swap[0]), to_cell(swap[1])};
    }
    const Array cascades = result["cascades"];
    for (int64_t i = 0; i < cascades.size(); ++i) {
        const Dictionary d = cascades[i];
        anim::Step step;
        const Array cleared = d.get("cleared", Array());
        for (int64_t j = 0; j < cleared.size(); ++j) step.cleared.push_back(to_cell(cleared[j]));
        const Array falls = d.get("falls", Array());
        for (int64_t j = 0; j < falls.size(); ++j) {
            const Dictionary f = falls[j];
            step.falls.push_back({to_cell(f["from"]), to_cell(f["to"]), static_cast<int>(f.get("kind", -1))});
        }
        const Array spawns = d.get("spawns", Array());
        for (int64_t j = 0; j < spawns.size(); ++j) {
            const Dictionary sp = spawns[j];
            step.spawns.push_back({to_cell(sp["to"]), static_cast<int>(sp.get("kind", -1)),
                                   std::max(1, static_cast<int>(sp.get("drop", 1)))});
        }
        ev.steps.push_back(std::move(step));
    }
    ev.shuffled = result.get("shuffled", false);
    ev.redealt = result.get("redealt", false);
    const Array map = result.get("shuffle_map", Array());
    for (int64_t i = 0; i < map.size(); ++i) {
        const Array pair = map[i];
        if (pair.size() == 2) ev.shuffle_map.emplace_back(to_cell(pair[0]), to_cell(pair[1]));
    }
    return true;
}

}  // namespace

// Turns plan phases into tweeners on one parallel tween, each delayed to its
// phase's start, while anim::walk tracks which node sits in which cell.
struct BoardView3D::PlanHooks {
    BoardView3D* view;
    Ref<Tween> tween;
    std::vector<Slot>* grid;

    void on_move(const anim::Phase& p, std::size_t i, const Slot& slot, anim::Cell, anim::Cell to) {
        if (!slot.node) return;
        const bool fall = p.kind == anim::Phase::Kind::Fall;
        const double seconds = fall ? p.move_seconds[i] : p.duration;
        tween->tween_property(slot.node, "position", view->cell_position(to.x, to.y), seconds)
            ->set_delay(p.start)
            ->set_trans(fall ? Tween::TRANS_QUAD : Tween::TRANS_SINE)
            ->set_ease(fall ? Tween::EASE_IN : Tween::EASE_IN_OUT);
    }

    void on_clear(const anim::Phase& p, const Slot& slot, anim::Cell) {
        if (!slot.node) return;
        const real_t pop = r(feel::kClearPopScale);
        tween->tween_property(slot.node, "scale", Vector3(pop, pop, pop), feel::kClearPopSeconds)
            ->set_delay(p.start)
            ->set_ease(Tween::EASE_OUT);
        tween->tween_property(slot.node, "scale", Vector3(0.001f, 0.001f, 0.001f), feel::kClearShrinkSeconds)
            ->set_delay(p.start + feel::kClearPopSeconds)
            ->set_trans(Tween::TRANS_BACK)
            ->set_ease(Tween::EASE_IN);
        view->dying_.push_back(slot);
    }

    Slot make_spawn(const anim::Phase& p, std::size_t i, const anim::Spawn& sp) {
        Node3D* node = view->library_.acquire(sp.kind);
        view->pieces_root_->add_child(node);
        node->set_visible(false);
        node->set_position(view->cell_position(sp.to.x, -sp.drop));
        const real_t s0 = r(feel::kSpawnStartScale);
        node->set_scale(Vector3(s0, s0, s0));
        const double seconds = p.spawn_seconds[i];
        tween->tween_callback(Callable(node, "set_visible").bind(true))->set_delay(p.start);
        tween->tween_property(node, "position", view->cell_position(sp.to.x, sp.to.y), seconds)
            ->set_delay(p.start)
            ->set_trans(Tween::TRANS_QUAD)
            ->set_ease(Tween::EASE_IN);
        tween->tween_property(node, "scale", Vector3(1, 1, 1), seconds * feel::kSpawnGrowFraction)->set_delay(p.start);
        return {node, sp.kind};
    }

    void on_redeal(const anim::Phase& p) {
        view->redealing_ = true;
        for (Slot& slot : *grid) {
            if (!slot.node) continue;
            tween->tween_property(slot.node, "scale", Vector3(0.001f, 0.001f, 0.001f), feel::kRedealOutSeconds)
                ->set_delay(p.start)
                ->set_ease(Tween::EASE_IN);
            view->dying_.push_back(slot);
            slot = Slot{};
        }
        tween->tween_callback(callable_mp(view, &BoardView3D::redeal_in))->set_delay(p.start + feel::kRedealOutSeconds);
    }
};

BoardView3D::~BoardView3D() { library_.clear(); }

void BoardView3D::set_cell_size(double v) {
    grid_.cell = Math::clamp(v, 0.25, 4.0);
    if (board_.is_valid() && is_inside_tree()) rebuild();
}

Vector3 BoardView3D::cell_position(int x, int y) const {
    const layout::Point p = layout::cell_center(grid_, x, y);
    return Vector3(r(p.x), 0.0f, r(p.z));
}

Vector3 BoardView3D::cell_position(Vector2i cell) const { return cell_position(cell.x, cell.y); }

bool BoardView3D::in_bounds(Vector2i c) const { return c.x >= 0 && c.y >= 0 && c.x < grid_.width && c.y < grid_.height; }

Node3D* BoardView3D::piece_at(Vector2i c) const { return in_bounds(c) ? pieces_[index(c)] : nullptr; }

void BoardView3D::show_board(const Ref<Match3Board>& board) {
    ERR_FAIL_COND_MSG(board.is_null(), "show_board needs a Match3Board.");
    board_ = board;
    if (tween_.is_valid()) tween_->kill();
    busy_ = false;
    rebuild();
}

void BoardView3D::rebuild() {
    clear_selection();
    clear_pieces();
    if (slab_) {
        slab_->queue_free();
        slab_ = nullptr;
    }
    grid_.width = board_->get_width();
    grid_.height = board_->get_height();
    // Pooled pieces were built for the old cell size.
    library_.clear();
    library_.load(grid_.cell);

    slab_ = library_.make_board(grid_.width, grid_.height, grid_.cell);
    slab_->set_name("Slab");
    add_child(slab_);
    if (!pieces_root_) {
        pieces_root_ = memnew(Node3D);
        pieces_root_->set_name("Pieces");
        add_child(pieces_root_);
    }
    pieces_.assign(static_cast<size_t>(grid_.width * grid_.height), nullptr);
    kinds_.assign(pieces_.size(), -1);
    sync_to_board(false);
    emit_signal("layout_changed");
}

void BoardView3D::clear_pieces() {
    for (size_t i = 0; i < pieces_.size(); ++i) {
        if (!pieces_[i]) continue;
        pieces_root_->remove_child(pieces_[i]);
        library_.release(kinds_[i], pieces_[i]);
        pieces_[i] = nullptr;
    }
}

void BoardView3D::sync_to_board(bool animate) {
    for (int y = 0; y < grid_.height; ++y) {
        for (int x = 0; x < grid_.width; ++x) {
            const int i = y * grid_.width + x;
            const int kind = board_->get_cell(x, y);
            const Vector3 home = cell_position(x, y);
            if (pieces_[i] && kind == kinds_[i]) {
                pieces_[i]->set_position(home);
                pieces_[i]->set_scale(Vector3(1, 1, 1));
                pieces_[i]->set_visible(true);
                continue;
            }
            if (pieces_[i]) {
                pieces_root_->remove_child(pieces_[i]);
                library_.release(kinds_[i], pieces_[i]);
            }
            Node3D* piece = library_.acquire(kind);
            kinds_[i] = kind;
            pieces_[i] = piece;
            pieces_root_->add_child(piece);
            if (!animate) {
                piece->set_position(home);
                continue;
            }
            // New or changed pieces drop in from above, like a refill.
            piece->set_position(home + Vector3(0, r(grid_.cell * feel::kSettleDropCells), 0));
            tween_->tween_property(piece, "position", home, feel::kSettleSeconds)
                ->set_trans(Tween::TRANS_QUAD)
                ->set_ease(Tween::EASE_IN);
        }
    }
}

Ref<Tween> BoardView3D::begin_animation() {
    if (tween_.is_valid()) tween_->kill();
    clear_selection();
    busy_ = true;
    tween_ = create_tween();
    tween_->set_speed_scale(static_cast<float>(animation_speed_));
    return tween_;
}

void BoardView3D::finish_animation() {
    tween_.unref();
    // Whatever played (or was skipped), the view now shows the board exactly.
    reconcile();
    busy_ = false;
    emit_signal("animation_finished");
}

void BoardView3D::play(const Dictionary& result) {
    ERR_FAIL_COND_MSG(board_.is_null(), "play() before show_board().");
    anim::Events events;
    if (parse_events(result, events)) {
        play_events(events);
        return;
    }
    // The result has no cell events (older binding): animate the swap, then
    // let every changed cell drop in at once.
    Vector2i a = last_request_[0];
    Vector2i b = last_request_[1];
    last_request_[0] = last_request_[1] = kNoCell;
    if (result.has("swap")) {
        const Array swap = result["swap"];
        if (swap.size() == 2) {
            a = swap[0];
            b = swap[1];
        }
    }
    Ref<Tween> tween = begin_animation();
    if (in_bounds(a) && in_bounds(b)) {
        Node3D* pa = piece_at(a);
        Node3D* pb = piece_at(b);
        tween->set_parallel(true);
        tween->tween_property(pa, "position", cell_position(b), feel::kSwapSeconds);
        tween->tween_property(pb, "position", cell_position(a), feel::kSwapSeconds);
        tween->set_parallel(false);
        // The swapped pieces now live in each other's cells.
        std::swap(pieces_[index(a)], pieces_[index(b)]);
        std::swap(kinds_[index(a)], kinds_[index(b)]);
    }
    // Until the cascade animator (WP-13) lands, every changed cell settles in
    // one beat after the swap.
    tween->tween_callback(callable_mp(this, &BoardView3D::settle));
}

void BoardView3D::settle() {
    // A running tween cannot take new steps, so the settle beat is its own tween.
    tween_ = create_tween();
    tween_->set_parallel(true);
    sync_to_board(true);
    tween_->chain()->tween_callback(callable_mp(this, &BoardView3D::finish_animation));
}

void BoardView3D::play_events(const anim::Events& events) {
    const anim::Plan plan = anim::build_plan(events);
    Ref<Tween> tween = begin_animation();
    tween->set_parallel(true);
    std::vector<Slot> grid(pieces_.size());
    for (size_t i = 0; i < pieces_.size(); ++i) grid[i] = {pieces_[i], kinds_[i]};
    PlanHooks hooks{this, tween, &grid};
    anim::walk(plan, grid, grid_.width, hooks);
    for (size_t i = 0; i < grid.size(); ++i) {
        pieces_[i] = grid[i].node;
        kinds_[i] = grid[i].kind;
    }
    for (const anim::Phase& p : plan.phases) {
        if (p.kind == anim::Phase::Kind::Clear) {
            tween->tween_callback(callable_mp(this, &BoardView3D::emit_cascade_step).bind(p.step))->set_delay(p.start);
        }
    }
    // An empty parallel step would end at once; the interval holds the tween
    // to the plan's full length before it finishes.
    tween->tween_interval(plan.total);
    tween->chain()->tween_callback(callable_mp(this, &BoardView3D::finish_animation));
}

void BoardView3D::emit_cascade_step(int step) { emit_signal("cascade_step", step); }

void BoardView3D::redeal_in() {
    // The old pieces have shrunk away; deal the board's new layout in.
    sync_to_board(false);
    Ref<Tween> grow = create_tween();
    grow->set_parallel(true);
    grow->set_speed_scale(static_cast<float>(animation_speed_));
    for (Node3D* piece : pieces_) {
        if (!piece) continue;
        piece->set_scale(Vector3(0.001f, 0.001f, 0.001f));
        grow->tween_property(piece, "scale", Vector3(1, 1, 1), feel::kRedealInSeconds)
            ->set_trans(Tween::TRANS_BACK)
            ->set_ease(Tween::EASE_OUT);
    }
}

void BoardView3D::release_dying() {
    for (const Slot& slot : dying_) {
        if (!slot.node) continue;
        if (slot.node->get_parent() == pieces_root_) pieces_root_->remove_child(slot.node);
        library_.release(slot.kind, slot.node);
    }
    dying_.clear();
}

void BoardView3D::reconcile() {
    release_dying();
    bool drift = false;
    for (int y = 0; y < grid_.height && !drift; ++y) {
        for (int x = 0; x < grid_.width; ++x) {
            if (kinds_[y * grid_.width + x] != board_->get_cell(x, y) || !pieces_[y * grid_.width + x]) {
                drift = true;
                break;
            }
        }
    }
    if (drift && OS::get_singleton()->is_debug_build() && !redealing_) {
        WARN_PRINT("BoardView3D: view drifted from the board after an animation; resyncing.");
    }
    redealing_ = false;
    sync_to_board(false);
}

void BoardView3D::skip_animation() {
    if (!busy_) return;
    if (tween_.is_valid()) tween_->kill();
    finish_animation();
}

void BoardView3D::set_animation_speed(double v) {
    animation_speed_ = Math::clamp(v, 0.1, 10.0);
    if (tween_.is_valid()) tween_->set_speed_scale(static_cast<float>(animation_speed_));
}

void BoardView3D::play_illegal(Vector2i a, Vector2i b) {
    ERR_FAIL_COND_MSG(board_.is_null(), "play_illegal() before show_board().");
    ERR_FAIL_COND(!in_bounds(a) || !in_bounds(b));
    Node3D* pa = piece_at(a);
    Node3D* pb = piece_at(b);
    const Vector3 pos_a = cell_position(a);
    const Vector3 pos_b = cell_position(b);
    const real_t reach = r(feel::kIllegalReach);
    Ref<Tween> tween = begin_animation();
    tween->set_parallel(true);
    tween->tween_property(pa, "position", pos_a.lerp(pos_b, reach), feel::kIllegalLegSeconds)
        ->set_ease(Tween::EASE_OUT);
    tween->tween_property(pb, "position", pos_b.lerp(pos_a, reach), feel::kIllegalLegSeconds)
        ->set_ease(Tween::EASE_OUT);
    tween->chain()->tween_property(pa, "position", pos_a, feel::kIllegalLegSeconds)->set_ease(Tween::EASE_IN);
    tween->tween_property(pb, "position", pos_b, feel::kIllegalLegSeconds)->set_ease(Tween::EASE_IN);
    tween->chain()->tween_callback(callable_mp(this, &BoardView3D::finish_animation));
}

Vector2i BoardView3D::cell_at_screen(Vector2 screen_pos) const {
    const Viewport* viewport = get_viewport();
    const Camera3D* camera = viewport ? viewport->get_camera_3d() : nullptr;
    if (!camera) return kNoCell;
    const Vector3 origin = camera->project_ray_origin(screen_pos);
    const Vector3 dir = camera->project_ray_normal(screen_pos);
    // Intersect with the board plane in the view's own space, so a moved or
    // tilted board still picks correctly.
    const Transform3D inv = get_global_transform().affine_inverse();
    const Vector3 o = inv.xform(origin);
    const Vector3 d = inv.basis.xform(dir);
    if (Math::is_zero_approx(d.y)) return kNoCell;
    const real_t t = -o.y / d.y;
    if (t < 0) return kNoCell;
    const Vector3 hit = o + d * t;
    int x = 0;
    int y = 0;
    if (!layout::cell_at(grid_, {hit.x, hit.z}, x, y)) return kNoCell;
    return Vector2i(x, y);
}

void BoardView3D::_unhandled_input(const Ref<InputEvent>& event) {
    if (Ref<InputEventMouseButton> mb = event; mb.is_valid()) {
        // Touches also arrive as emulated mouse events; take the touch only.
        if (mb->get_device() == InputEvent::DEVICE_ID_EMULATION) return;
        if (mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) handle_press(mb->get_position());
    } else if (Ref<InputEventScreenTouch> touch = event; touch.is_valid()) {
        if (touch->is_pressed()) handle_press(touch->get_position());
    }
}

void BoardView3D::handle_press(Vector2 screen_pos) {
    if (busy_ || board_.is_null()) return;
    const Vector2i cell = cell_at_screen(screen_pos);
    if (cell == kNoCell) {
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
    last_request_[0] = from;
    last_request_[1] = cell;
    emit_signal("swap_requested", from, cell);
}

void BoardView3D::select(Vector2i cell) {
    has_selection_ = true;
    selected_ = cell;
    if (Node3D* piece = piece_at(cell)) {
        const real_t s = r(feel::kSelectedScale);
        piece->set_scale(Vector3(s, s, s));
        piece->set_position(cell_position(cell) + Vector3(0, r(grid_.cell * feel::kSelectedLiftCells), 0));
    }
}

void BoardView3D::clear_selection() {
    if (has_selection_) {
        if (Node3D* piece = piece_at(selected_)) {
            piece->set_scale(Vector3(1, 1, 1));
            piece->set_position(cell_position(selected_));
        }
    }
    has_selection_ = false;
}

void BoardView3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("show_board", "board"), &BoardView3D::show_board);
    ClassDB::bind_method(D_METHOD("play", "result"), &BoardView3D::play);
    ClassDB::bind_method(D_METHOD("play_illegal", "a", "b"), &BoardView3D::play_illegal);
    ClassDB::bind_method(D_METHOD("is_busy"), &BoardView3D::is_busy);
    ClassDB::bind_method(D_METHOD("skip_animation"), &BoardView3D::skip_animation);
    ClassDB::bind_method(D_METHOD("set_animation_speed", "scale"), &BoardView3D::set_animation_speed);
    ClassDB::bind_method(D_METHOD("get_animation_speed"), &BoardView3D::get_animation_speed);
    ClassDB::bind_method(D_METHOD("get_board_width"), &BoardView3D::get_board_width);
    ClassDB::bind_method(D_METHOD("get_board_height"), &BoardView3D::get_board_height);
    ClassDB::bind_method(D_METHOD("set_cell_size", "value"), &BoardView3D::set_cell_size);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &BoardView3D::get_cell_size);
    ClassDB::bind_method(D_METHOD("cell_at_screen", "screen_position"), &BoardView3D::cell_at_screen);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.25,4,0.05"), "set_cell_size",
                 "get_cell_size");
    ADD_SIGNAL(MethodInfo("swap_requested", PropertyInfo(Variant::VECTOR2I, "a"), PropertyInfo(Variant::VECTOR2I, "b")));
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_speed", PROPERTY_HINT_RANGE, "0.1,10,0.05"),
                 "set_animation_speed", "get_animation_speed");
    ADD_SIGNAL(MethodInfo("animation_finished"));
    ADD_SIGNAL(MethodInfo("cascade_step", PropertyInfo(Variant::INT, "step")));
    ADD_SIGNAL(MethodInfo("layout_changed"));
}

}  // namespace montezuma::godot_binding
