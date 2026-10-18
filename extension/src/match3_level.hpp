// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <optional>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "montezuma/level_state.hpp"

namespace montezuma::godot_binding {

// Exposes montezuma::LevelState (quotas, timer, score, streak, outcome) to
// Godot. Feed it every Dictionary that Match3Board.try_swap or clear_cells
// returns, and the frame delta while the board is shown.
class Match3Level : public godot::RefCounted {
    GDCLASS(Match3Level, godot::RefCounted)

public:
    enum Outcome { PLAYING = 0, WON = 1, LOST = 2 };

    // quotas: {kind: pieces to clear}. Fails with an engine error, leaving
    // the level unset, on empty quotas or a non-positive count or time.
    void setup(const godot::Dictionary& quotas, double time_limit);
    // A Match3Board move Dictionary; an empty one (illegal swap) is ignored.
    void apply_move(const godot::Dictionary& result);
    void tick(double delta);

    bool is_ready() const { return state_.has_value(); }
    int get_outcome() const;
    int get_score() const;
    double get_time_left() const;
    godot::Dictionary get_quotas() const;
    int get_streak_kind() const;
    int get_streak() const;

protected:
    static void _bind_methods();

private:
    void emit_finished_if_done(int before);

    std::optional<LevelState> state_;
};

}  // namespace montezuma::godot_binding

VARIANT_ENUM_CAST(montezuma::godot_binding::Match3Level::Outcome);
