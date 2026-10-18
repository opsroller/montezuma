// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "board_view_3d.hpp"

#include <algorithm>
#include <utility>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/visual_instance3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "animator.hpp"
#include "feel.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

real_t r(double v) { return static_cast<real_t>(v); }

const Vector2i kNoCell(-1, -1);

// Bounds of every mesh under `node`, in `node`'s parent space.
void grow_bounds(const Node3D* node, const Transform3D& to_parent, AABB& box, bool& any) {
    const Transform3D xf = to_parent * node->get_transform();
    if (const auto* vi = Object::cast_to<VisualInstance3D>(node)) {
        const AABB local = xf.xform(vi->get_aabb());
        box = any ? box.merge(local) : local;
        any = true;
    }
    for (int i = 0; i < node->get_child_count(); ++i) {
        if (const auto* child = Object::cast_to<Node3D>(node->get_child(i))) grow_bounds(child, xf, box, any);
    }
}

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

    void on_clear(const anim::Phase& p, const Slot& slot, anim::Cell cell) {
        tween->tween_callback(callable_mp(view, &BoardView3D::spawn_match_fx).bind(view->cell_position(cell.x, cell.y)))
            ->set_delay(p.start);
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
    if (board_.is_valid() && is_inside_tree()) {
        stop_animation();
        rebuild();
    }
}

void BoardView3D::set_theme(const String& v) {
    if (v == theme_) return;
    theme_ = v;
    if (environment_) {
        environment_->queue_free();
        environment_ = nullptr;
    }
    if (board_.is_valid() && is_inside_tree()) {
        stop_animation();
        rebuild();
    }
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
    stop_animation();
    rebuild();
}

void BoardView3D::stop_animation() {
    // Kills every running tween and frees what it was animating away, so a
    // rebuild never leaves shrunken pieces behind or tweens moving pooled nodes.
    if (tween_.is_valid()) tween_->kill();
    tween_.unref();
    finish_grow();
    if (pieces_root_) release_dying();
    redealing_ = false;
    busy_ = false;
}

void BoardView3D::rebuild() {
    clear_selection();
    clear_pieces();
    if (slab_) {
        slab_->queue_free();
        slab_ = nullptr;
    }
    if (highlight_) {
        highlight_->queue_free();
        highlight_ = nullptr;
    }
    grid_.width = board_->get_width();
    grid_.height = board_->get_height();
    gesture_.reset(grid_.width, grid_.height);
    // Pooled pieces were built for the old cell size or theme.
    library_.clear();
    library_.load(grid_.cell, theme_);

    // The theme's environment brings the sky and every light, so the scene
    // around the view supplies none. It outlives redeals; a theme change drops it.
    if (!environment_) {
        environment_ = library_.make_environment();
        environment_->set_name("Environment");
        add_child(environment_);
    }
    slab_ = library_.make_board(grid_.width, grid_.height, grid_.cell);
    slab_->set_name("Slab");
    add_child(slab_);
    AABB box;
    bool any = false;
    grow_bounds(slab_, Transform3D(), box, any);
    const Vector3 hi = box.position + box.size;
    board_padding_ = any ? layout::board_padding(grid_, box.position.x, box.position.z, hi.x, hi.z) : 0.0;
    highlight_ = library_.make_highlight();
    if (highlight_) {
        highlight_->set_name("CellHighlight");
        highlight_lift_ = highlight_->get_position().y;  // keeps it just above the tiles
        highlight_->set_visible(false);
        add_child(highlight_);
    }
    if (!pieces_root_) {
        pieces_root_ = memnew(Node3D);
        pieces_root_->set_name("Pieces");
        add_child(pieces_root_);
        fx_root_ = memnew(Node3D);
        fx_root_->set_name("Effects");
        add_child(fx_root_);
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

void BoardView3D::finish_grow() {
    // The new deal growing in after a redeal outlives busy_; end it at full size.
    if (grow_tween_.is_null()) return;
    grow_tween_->kill();
    grow_tween_.unref();
    for (Node3D* piece : pieces_) {
        if (piece) piece->set_scale(Vector3(1, 1, 1));
    }
}

Ref<Tween> BoardView3D::begin_animation() {
    if (tween_.is_valid()) tween_->kill();
    finish_grow();
    clear_selection();
    gesture_.reset(grid_.width, grid_.height);
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

void BoardView3D::spawn_match_fx(Vector3 position) {
    // The burst frees itself when its particles are done.
    Node3D* fx = library_.make_match_fx();
    if (!fx) return;
    fx->set_position(position);
    fx_root_->add_child(fx);
}

void BoardView3D::redeal_in() {
    // The old pieces have shrunk away; deal the board's new layout in.
    sync_to_board(false);
    Ref<Tween> grow = create_tween();
    grow_tween_ = grow;
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
    finish_grow();
    finish_animation();
}

void BoardView3D::set_animation_speed(double v) {
    animation_speed_ = Math::clamp(v, 0.1, 10.0);
    if (tween_.is_valid()) tween_->set_speed_scale(static_cast<float>(animation_speed_));
    if (grow_tween_.is_valid()) grow_tween_->set_speed_scale(static_cast<float>(animation_speed_));
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
    if (board_.is_null()) return;
    // Touches also arrive as emulated mouse events; take the touch only.
    if (Ref<InputEventMouse> m = event; m.is_valid() && m->get_device() == InputEvent::DEVICE_ID_EMULATION) return;
    // Mouse is pointer -1; touches use their finger index.
    constexpr int kMouse = -1;
    input::Output out;
    bool handled = false;
    if (Ref<InputEventMouseButton> mb = event; mb.is_valid()) {
        if (mb->get_button_index() != MOUSE_BUTTON_LEFT) return;
        handled = true;
        out = mb->is_pressed() ? press(kMouse, mb->get_position()) : gesture_.release(kMouse);
    } else if (Ref<InputEventMouseMotion> mm = event; mm.is_valid()) {
        if (!gesture_.pressing()) return;
        handled = true;
        out = gesture_.move(kMouse, {mm->get_position().x, mm->get_position().y});
    } else if (Ref<InputEventScreenTouch> touch = event; touch.is_valid()) {
        handled = true;
        if (touch->is_canceled()) {
            gesture_.cancel();
        } else {
            out = touch->is_pressed() ? press(touch->get_index(), touch->get_position())
                                      : gesture_.release(touch->get_index());
        }
    } else if (Ref<InputEventScreenDrag> drag = event; drag.is_valid()) {
        if (!gesture_.pressing()) return;
        handled = true;
        out = gesture_.move(drag->get_index(), {drag->get_position().x, drag->get_position().y});
    }
    if (handled) get_viewport()->set_input_as_handled();
    apply_input(out);
}

void BoardView3D::_notification(int what) {
    // Losing focus ends any press, so nothing stays held (alt-tab, app switch).
    // Window focus events are not used: some X11 setups send one on every
    // click (pointer grab), which would cancel every swipe.
    if (what == NOTIFICATION_APPLICATION_FOCUS_OUT) gesture_.cancel();
}

input::Output BoardView3D::press(int pointer, Vector2 screen_pos) {
    // Taps during an animation are ignored by design: the board is about to
    // change under the finger.
    if (busy_) return {};
    gesture_.set_swipe_threshold(swipe_threshold_pixels());
    const Vector2i c = cell_at_screen(screen_pos);
    const input::Cell cell = c == kNoCell ? input::Cell{} : input::Cell{c.x, c.y};
    return gesture_.press(pointer, {screen_pos.x, screen_pos.y}, cell);
}

double BoardView3D::swipe_threshold_pixels() const {
    // A fraction of one cell as drawn now, but never less than a few
    // millimetres, so a shaky tap is not a swipe on a dense screen.
    double cell_px = 0.0;
    if (const Camera3D* camera = get_viewport()->get_camera_3d()) {
        const Vector2 a = camera->unproject_position(to_global(cell_position(0, 0)));
        const Vector2 b = camera->unproject_position(to_global(cell_position(1, 0)));
        cell_px = a.distance_to(b);
    }
    int dpi = DisplayServer::get_singleton()->screen_get_dpi();
    if (dpi <= 0) dpi = 96;
    const double min_px = feel::kSwipeMinMillimetres * dpi / 25.4;
    return std::max(min_px, cell_px * feel::kSwipeCellFraction);
}

void BoardView3D::apply_input(const input::Output& out) {
    switch (out.action) {
        case input::Action::None:
            return;
        case input::Action::Select:
            clear_selection();
            select(Vector2i(out.a.x, out.a.y));
            return;
        case input::Action::Deselect:
            clear_selection();
            return;
        case input::Action::Swap: {
            clear_selection();
            const Vector2i from(out.a.x, out.a.y);
            const Vector2i to(out.b.x, out.b.y);
            last_request_[0] = from;
            last_request_[1] = to;
            emit_signal("swap_requested", from, to);
            return;
        }
    }
}

void BoardView3D::select(Vector2i cell) {
    has_selection_ = true;
    selected_ = cell;
    if (Node3D* piece = piece_at(cell)) {
        const real_t s = r(feel::kSelectedScale);
        piece->set_scale(Vector3(s, s, s));
        piece->set_position(cell_position(cell) + Vector3(0, r(grid_.cell * feel::kSelectedLiftCells), 0));
    }
    if (highlight_) {
        highlight_->set_position(cell_position(cell) + Vector3(0, highlight_lift_, 0));
        highlight_->set_visible(true);
    }
}

void BoardView3D::clear_selection() {
    if (has_selection_) {
        if (Node3D* piece = piece_at(selected_)) {
            piece->set_scale(Vector3(1, 1, 1));
            piece->set_position(cell_position(selected_));
        }
    }
    if (highlight_) highlight_->set_visible(false);
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
    ClassDB::bind_method(D_METHOD("set_theme", "theme"), &BoardView3D::set_theme);
    ClassDB::bind_method(D_METHOD("get_theme"), &BoardView3D::get_theme);
    ClassDB::bind_method(D_METHOD("get_board_padding"), &BoardView3D::get_board_padding);
    ClassDB::bind_method(D_METHOD("cell_at_screen", "screen_position"), &BoardView3D::cell_at_screen);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.25,4,0.05"), "set_cell_size",
                 "get_cell_size");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "theme"), "set_theme", "get_theme");
    ADD_SIGNAL(MethodInfo("swap_requested", PropertyInfo(Variant::VECTOR2I, "a"), PropertyInfo(Variant::VECTOR2I, "b")));
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_speed", PROPERTY_HINT_RANGE, "0.1,10,0.05"),
                 "set_animation_speed", "get_animation_speed");
    ADD_SIGNAL(MethodInfo("animation_finished"));
    ADD_SIGNAL(MethodInfo("cascade_step", PropertyInfo(Variant::INT, "step")));
    ADD_SIGNAL(MethodInfo("layout_changed"));
}

}  // namespace montezuma::godot_binding
