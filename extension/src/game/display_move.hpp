// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "../board/animator.hpp"
#include "../board/effects/effect_plan.hpp"
#include "../board/feel.hpp"
#include "montezuma/level_session.hpp"

// A LevelSession move as the board view shows it (WP-29): the swap's
// cascades, then each fired totem's (a totem that clears nothing gets an
// empty step of its own, so it has a place on the timeline), then the
// automatic reshuffle; and, step by step, what each step set off. Godot-free
// so the board tests can walk real sessions with it; Match3Game packs it
// into the Dictionary the view plays.

namespace montezuma::godot_binding {

struct DisplayMove {
    anim::Events events;
    std::vector<fx::StepFx> fx;  // parallel to events.steps
    // Display step each firing starts at, parallel to MoveReport::totems_fired.
    std::vector<int> totem_steps;
};

namespace display {

inline anim::Cell cell(Cell c) { return {c.x, c.y}; }

inline std::vector<anim::Cell> cells(const std::vector<Cell>& in) {
    std::vector<anim::Cell> out;
    out.reserve(in.size());
    for (const Cell& c : in) out.push_back(cell(c));
    return out;
}

inline anim::Step step(const CascadeStep& s) {
    anim::Step out;
    out.cleared = cells(s.cleared);
    for (const Fall& f : s.falls) out.falls.push_back({cell(f.from), cell(f.to), f.kind});
    for (const Spawn& sp : s.spawns) out.spawns.push_back({cell(sp.to), sp.kind, sp.drop});
    return out;
}

inline std::vector<fx::Placement> placements(const std::vector<PowerUpPlacement>& in) {
    std::vector<fx::Placement> out;
    for (const PowerUpPlacement& p : in)
        out.push_back({cell(p.cell), static_cast<int>(power_up_index(p.power_up.id))});
    return out;
}

inline fx::StepFx step_fx(const StepReport& r) {
    fx::StepFx out;
    for (std::size_t i = 0; i < r.power_ups_triggered.size(); ++i) {
        const TriggeredPowerUp& t = r.power_ups_triggered[i];
        fx::PowerUpGoOff p;
        p.cell = cell(t.cell);
        p.id = static_cast<int>(power_up_index(t.power_up.id));
        if (i < r.power_up_effects.size()) {
            const PowerUpEffect& e = r.power_up_effects[i];
            p.cells = cells(e.cells_to_clear);
            p.time_seconds = e.time_delta_seconds;
            p.multiplier = e.score_multiplier;
            p.hint_seconds = e.hint_seconds;
        }
        out.power_ups.push_back(std::move(p));
    }
    out.power_ups_placed = placements(r.power_ups_placed);
    out.gems_collected = cells(r.gems_collected);
    out.gems_placed = cells(r.gems_placed);
    out.steel_spawned = cells(r.steel_spawned);
    out.steel_removed = cells(r.steel_removed);
    out.chains_broken = cells(r.chains_broken);
    return out;
}

inline fx::TotemFire totem_fire(const TotemFiring& f) {
    fx::TotemFire t;
    t.id = static_cast<int>(f.effect.totem);
    t.targets = cells(f.effect.cells_to_clear);
    if (f.effect.repaint_kind >= 0)
        for (const Cell& c : f.effect.cells_to_repaint) t.repaints.push_back({cell(c), f.effect.repaint_kind});
    t.time_seconds = f.effect.time_delta_seconds;
    t.points = f.points;
    t.gems_harvested = cells(f.gems_harvested);
    t.gems_placed = cells(f.gems_placed);
    t.chains_removed = cells(f.chains_removed);
    t.power_ups_placed = placements(f.power_ups_placed);
    return t;
}

}  // namespace display

inline DisplayMove display_move(const MoveReport& report) {
    DisplayMove out;
    out.events.swap = std::array<anim::Cell, 2>{display::cell(report.swap[0]), display::cell(report.swap[1])};
    if (report.result.swap)
        out.events.swap = std::array<anim::Cell, 2>{display::cell((*report.result.swap)[0]),
                                                     display::cell((*report.result.swap)[1])};
    for (std::size_t i = 0; i < report.result.cascades.size(); ++i) {
        out.events.steps.push_back(display::step(report.result.cascades[i]));
        out.fx.push_back(i < report.steps.size() ? display::step_fx(report.steps[i]) : fx::StepFx{});
    }
    for (const TotemFiring& firing : report.totems_fired) {
        out.totem_steps.push_back(static_cast<int>(out.events.steps.size()));
        if (!firing.result || firing.steps.empty()) {
            // Clears nothing (time, gold, gift, harvest, a paint that made no
            // run): an empty step carries its lead and repaints.
            out.events.steps.emplace_back();
            out.fx.emplace_back();
        } else {
            for (std::size_t i = 0; i < firing.result->cascades.size(); ++i) {
                out.events.steps.push_back(display::step(firing.result->cascades[i]));
                out.fx.push_back(i < firing.steps.size() ? display::step_fx(firing.steps[i]) : fx::StepFx{});
            }
        }
        const std::size_t first = static_cast<std::size_t>(out.totem_steps.back());
        out.fx[first].totem = display::totem_fire(firing);
        out.events.steps[first].lead = feel::kTotemLeadSeconds;
        out.events.steps[first].repaints = out.fx[first].totem->repaints;
    }
    if (report.auto_shuffle && report.auto_shuffle->shuffled) {
        out.events.shuffled = true;
        out.events.redealt = report.auto_shuffle->redealt;
        for (const auto& [from, to] : report.auto_shuffle->shuffle_map)
            out.events.shuffle_map.push_back({display::cell(from), display::cell(to)});
    }
    return out;
}

// The marks the view draws for every cell, row-major (fx::marks).
inline std::vector<int> session_marks(const LevelSession& s) {
    const Board& b = s.board();
    std::vector<int> out(static_cast<std::size_t>(b.width() * b.height()), fx::marks::kNone);
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            int m = fx::marks::kNone;
            if (s.gem({x, y})) m |= fx::marks::kGem;
            if (s.steel({x, y})) m |= fx::marks::kSteel;
            if (s.chained({x, y})) m |= fx::marks::kChain;
            if (const std::optional<PowerUp> p = s.power_ups().at({x, y}))
                m = fx::marks::with_power_up(m, static_cast<int>(power_up_index(p->id)));
            out[static_cast<std::size_t>(y * b.width() + x)] = m;
        }
    }
    return out;
}

}  // namespace montezuma::godot_binding
