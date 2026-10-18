// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cmath>
#include <cstdlib>

// Board input: turns one pointer stream (mouse, touch or pen, already
// resolved to screen positions and cells) into the board's two actions,
// "select a cell" and "swap two adjacent cells". No Godot types, so it is
// unit-tested without devices; BoardView3D feeds it engine events.
//
// Gestures, both on every pointer type:
//   tap-tap  tap a piece to select it, then tap a neighbour to swap. Tapping
//            the selected piece again, or off the board, deselects; tapping a
//            piece further away selects that one instead.
//   swipe    press on a piece and drag toward a neighbour past the swipe
//            threshold; the swap fires as soon as the threshold is crossed.
// Only the first pointer down is tracked; other fingers are ignored until it
// lifts. A cancel (system gesture, focus loss) drops the press.

namespace montezuma::godot_binding::input {

struct Cell {
    int x = -1;
    int y = -1;
    bool valid() const { return x >= 0 && y >= 0; }
    bool operator==(const Cell&) const = default;
};

struct Point {
    double x = 0.0;
    double y = 0.0;
};

enum class Action { None, Select, Deselect, Swap };

struct Output {
    Action action = Action::None;
    Cell a;  // Select: the cell. Swap: from.
    Cell b;  // Swap: to.
};

inline bool adjacent(Cell a, Cell b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1; }

class SwapGesture {
public:
    // Board size, for swipes off the edge. Clears any selection.
    void reset(int width, int height) {
        width_ = width;
        height_ = height;
        selected_ = {};
        active_ = false;
        spent_ = false;
    }
    // Screen distance a drag must cover to count as a swipe.
    void set_swipe_threshold(double pixels) { threshold_ = pixels; }
    double swipe_threshold() const { return threshold_; }

    Cell selected() const { return selected_; }
    bool pressing() const { return active_; }

    // A pointer went down at `pos`, over `cell` (invalid when off the board).
    Output press(int pointer, Point pos, Cell cell) {
        if (active_) return {};  // a second finger while one is down
        if (!cell.valid()) {
            const bool had = selected_.valid();
            selected_ = {};
            return had ? Output{Action::Deselect, {}, {}} : Output{};
        }
        if (selected_.valid() && adjacent(selected_, cell)) {
            const Cell from = selected_;
            selected_ = {};
            hold_spent(pointer);
            return {Action::Swap, from, cell};
        }
        active_ = true;
        pointer_ = pointer;
        origin_ = pos;
        pressed_ = cell;
        was_selected_ = selected_ == cell;
        if (was_selected_) return {};  // may become a deselect on release, or a swipe
        selected_ = cell;
        return {Action::Select, cell, {}};
    }

    // The pointer moved while down.
    Output move(int pointer, Point pos) {
        if (!active_ || spent_ || pointer != pointer_) return {};
        const double dx = pos.x - origin_.x;
        const double dy = pos.y - origin_.y;
        if (std::hypot(dx, dy) < threshold_) return {};
        spent_ = true;
        Cell to = pressed_;
        // Screen y grows downward, like board rows.
        if (std::abs(dx) >= std::abs(dy)) {
            to.x += dx > 0 ? 1 : -1;
        } else {
            to.y += dy > 0 ? 1 : -1;
        }
        selected_ = {};
        if (to.x < 0 || to.y < 0 || to.x >= width_ || to.y >= height_) return {Action::Deselect, {}, {}};
        return {Action::Swap, pressed_, to};
    }

    // The pointer lifted.
    Output release(int pointer) {
        if (!active_ || pointer != pointer_) return {};
        active_ = false;
        if (spent_) {
            spent_ = false;
            return {};
        }
        if (was_selected_) {
            selected_ = {};
            return {Action::Deselect, {}, {}};
        }
        return {};
    }

    // The system took the pointer away; forget the press, keep the selection.
    void cancel() {
        active_ = false;
        spent_ = false;
    }

private:
    // A press that already swapped (or left the board) keeps the pointer
    // until it lifts, so a duplicate press for the same tap does nothing.
    void hold_spent(int pointer) {
        active_ = true;
        spent_ = true;
        pointer_ = pointer;
    }

    int width_ = 8;
    int height_ = 8;
    double threshold_ = 24.0;
    Cell selected_;
    bool active_ = false;
    bool spent_ = false;
    int pointer_ = 0;
    Point origin_;
    Cell pressed_;
    bool was_selected_ = false;
};

}  // namespace montezuma::godot_binding::input
