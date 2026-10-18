// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Totem and power-up effect tests (WP-29): the effect plan's timing, and
// markers that follow their pieces through real LevelSession moves. No
// Godot, no framework; run via ctest.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "../game/display_move.hpp"
#include "animator.hpp"
#include "effects/effect_plan.hpp"
#include "feel.hpp"

using namespace montezuma::godot_binding;
using anim::Cell;

namespace {

int checks = 0;
int failures = 0;

void check(bool cond, const std::string& label) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    }
}

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

// What the view tracks per cell: a piece's kind and its markers.
struct Slot {
    int kind = -1;
    int marks = fx::marks::kNone;
    bool operator==(const Slot&) const = default;
};

// Walks a display move as the board view does, markers included.
struct SlotHooks {
    fx::MarkTracker tracker;
    const montezuma::LevelSession* session = nullptr;
    std::vector<Slot>* grid = nullptr;
    int width = 0;
    int repaints = 0;

    void on_move(const anim::Phase&, std::size_t, const Slot&, Cell, Cell) {}
    void on_clear(const anim::Phase&, const Slot&, Cell) {}
    Slot make_spawn(const anim::Phase& p, std::size_t, const anim::Spawn& sp) {
        return {sp.kind, tracker.spawn_marks(p.step, sp.to)};
    }
    void on_redeal(const anim::Phase&);
    Slot make_repaint(const anim::Phase&, const Slot& old, const anim::Repaint& rp) {
        ++repaints;
        return {rp.kind, old.marks};
    }
    void set(Cell c, int change, bool on) {
        Slot& s = (*grid)[static_cast<std::size_t>(c.y * width + c.x)];
        s.marks = fx::MarkTracker::apply(s.marks, change, on);
    }
    void before_phase(const anim::Phase& p, std::vector<Slot>&) {
        if (p.kind == anim::Phase::Kind::Clear)
            tracker.before_clear(p.step, [this](Cell c, int change, bool on) { set(c, change, on); });
    }
    void after_phase(const anim::Phase& p, std::vector<Slot>&) {
        if (p.kind == anim::Phase::Kind::Fall)
            tracker.after_fall(p.step, [this](Cell c, int change, bool on) { set(c, change, on); });
    }
};

std::vector<Slot> slots_of(const montezuma::LevelSession& s) {
    const std::vector<int> kinds = s.board().cells();
    const std::vector<int> marks = session_marks(s);
    std::vector<Slot> out(kinds.size());
    for (std::size_t i = 0; i < kinds.size(); ++i) out[i] = {kinds[i], marks[i]};
    return out;
}

void SlotHooks::on_redeal(const anim::Phase&) { *grid = slots_of(*session); }

// ---- Plan ----

void test_lead_comes_before_the_clear() {
    anim::Events ev;
    anim::Step s;
    s.lead = feel::kTotemLeadSeconds;
    s.repaints = {{{1, 0}, 4}};
    s.cleared = {{0, 0}};
    s.spawns = {{{0, 0}, 2, 1}};
    ev.steps.push_back(s);
    const anim::Plan plan = anim::build_plan(ev);
    check(plan.phases.size() == 3, "effect, clear, fall");
    check(plan.phases[0].kind == anim::Phase::Kind::Effect && plan.phases[0].step == 0, "the lead is step 0's");
    check(near(plan.phases[1].start, feel::kTotemLeadSeconds), "the clear waits for the lead");

    std::vector<Slot> grid{{1, 0}, {3, fx::marks::kGem}};
    SlotHooks hooks;
    hooks.grid = &grid;
    hooks.width = 2;
    anim::walk(plan, grid, 2, hooks);
    check(hooks.repaints == 1, "the repaint is walked");
    check(grid[1] == Slot{4, fx::marks::kGem}, "a repainted piece keeps its gem");
    check(grid[0].kind == 2, "the clear and refill still play");
}

void test_empty_step_still_has_time() {
    // A totem that clears nothing (time, gold, gift) still gets its moment.
    anim::Events ev;
    anim::Step s;
    s.lead = feel::kTotemLeadSeconds;
    ev.steps.push_back(s);
    const anim::Plan plan = anim::build_plan(ev);
    std::vector<fx::StepFx> fx(1);
    fx[0].totem = fx::TotemFire{};
    fx[0].totem->id = fx::kTime;
    fx[0].totem->time_seconds = 60;
    const auto beats = fx::build_beats(plan, fx);
    check(beats.size() == 1 && beats[0].kind == fx::Beat::Kind::Totem, "one totem beat");
    check(beats[0].duration > 0.2, "with time to read");
    check(near(beats[0].value, 60.0), "carrying its seconds");
}

void test_beats_end_inside_their_step() {
    // Readability rule 3: a transient effect covers a cell no longer than
    // the animation step it belongs to.
    anim::Events ev;
    anim::Step a;
    a.cleared = {{0, 7}, {1, 7}, {2, 7}};
    for (int x = 0; x < 3; ++x) a.spawns.push_back({{x, 7}, 1, 1});
    anim::Step b = a;
    b.lead = feel::kTotemLeadSeconds;
    ev.steps = {a, b};
    const anim::Plan plan = anim::build_plan(ev);
    std::vector<fx::StepFx> fx(2);
    fx[0].power_ups.push_back({{0, 7}, fx::kTnt, {{0, 7}, {1, 7}}, 0, 1, 0});
    fx[0].power_ups.push_back({{1, 7}, fx::kClock, {{1, 7}}, 5, 1, 0});
    fx[0].gems_collected = {{2, 7}};
    fx[0].steel_removed = {{1, 7}};
    fx[0].chains_broken = {{0, 6}};
    fx[0].power_ups_placed.push_back({{3, 3}, fx::kLightning});
    fx[1].totem = fx::TotemFire{};
    fx[1].totem->id = fx::kFire;
    fx[1].totem->targets = a.cleared;
    const auto windows = fx::step_windows(plan, 2);
    const auto beats = fx::build_beats(plan, fx);
    int transient = 0;
    bool inside = true;
    bool placed_late = false;
    for (const fx::Beat& beat : beats) {
        const fx::Window& w = windows[static_cast<std::size_t>(beat.step)];
        if (beat.kind == fx::Beat::Kind::MarkPlaced) {
            placed_late = near(beat.start, w.end);
            continue;
        }
        ++transient;
        inside = inside && beat.start >= w.start - 1e-9 && beat.start + beat.duration <= w.end + 1e-9;
    }
    check(transient == 6, "every transient beat planned");
    check(inside, "every transient beat ends inside its step");
    check(placed_late, "a placed badge pops in after its step's refill");
    check(near(windows[1].clear - windows[1].start, feel::kTotemLeadSeconds), "the totem step opens with its lead");
}

// ---- Markers through real sessions ----

montezuma::LevelData busy_level(std::uint64_t seed) {
    montezuma::LevelData l;
    l.id = "e1-l01";
    l.episode = 1;
    l.index = 1;
    l.gem_goal = montezuma::GemGoal{200, 200};
    for (std::string_view t : fx::kTotemNames) l.totems_unlocked.emplace_back(t);
    for (std::string_view p : fx::kPowerUpNames) l.powerups_unlocked.emplace_back(p);
    l.steel_rate = 0.08;
    l.chained_cells = {{0, 0}, {3, 3}, {4, 4}, {7, 7}, {2, 5}};
    l.seed = seed;
    return l;
}

void test_marks_follow_real_moves() {
    montezuma::Upgrades up;
    up.totem_tiers = {3, 3, 3, 3, 3, 3, 3};
    up.power_up_tiers = {3, 3, 3, 3, 3, 3};
    up.frenzy_tier = 1;
    int moves = 0;
    bool kinds_match = true;
    bool marks_match = true;
    std::array<int, 7> totems{};
    std::array<int, 6> power_ups{};
    int placed = 0, gems = 0, steel_in = 0, steel_out = 0, chains = 0, repaints = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
        montezuma::LevelSession s(busy_level(seed), montezuma::Difficulty::Normal, up, seed);
        std::mt19937 rng(static_cast<unsigned>(seed));
        for (int turn = 0; turn < 60 && s.outcome() == montezuma::Outcome::Playing; ++turn) {
            const auto legal = s.legal_moves();
            if (legal.empty()) break;
            const auto& m = legal[rng() % legal.size()];
            std::vector<Slot> grid = slots_of(s);
            const auto report = s.swap(m[0], m[1]);
            if (!report || report->stuck) break;
            const DisplayMove dm = display_move(*report);
            check(dm.fx.size() == dm.events.steps.size(), "one fx entry per display step");
            SlotHooks hooks;
            hooks.tracker = {&dm.fx, s.board().width()};
            hooks.session = &s;
            hooks.grid = &grid;
            hooks.width = s.board().width();
            anim::walk(anim::build_plan(dm.events), grid, hooks.width, hooks);
            repaints += hooks.repaints;
            const std::vector<Slot> want = slots_of(s);
            for (std::size_t i = 0; i < want.size(); ++i) {
                kinds_match = kinds_match && grid[i].kind == want[i].kind;
                marks_match = marks_match && grid[i].marks == want[i].marks;
            }
            ++moves;
            for (const fx::StepFx& f : dm.fx) {
                if (f.totem) ++totems[static_cast<std::size_t>(f.totem->id)];
                for (const auto& p : f.power_ups) ++power_ups[static_cast<std::size_t>(p.id)];
                placed += static_cast<int>(f.power_ups_placed.size());
                gems += static_cast<int>(f.gems_collected.size());
                steel_in += static_cast<int>(f.steel_spawned.size());
                steel_out += static_cast<int>(f.steel_removed.size());
                chains += static_cast<int>(f.chains_broken.size());
            }
        }
    }
    check(moves >= 1000, "played over a thousand real moves");
    check(kinds_match, "every walk ends on the session's board");
    check(marks_match, "every marker ends where the session has it");
    for (std::size_t i = 0; i < totems.size(); ++i)
        check(totems[i] > 0, "totem " + std::string(fx::kTotemNames[i]) + " fired");
    for (std::size_t i = 0; i < power_ups.size(); ++i)
        check(power_ups[i] > 0, "power-up " + std::string(fx::kPowerUpNames[i]) + " went off");
    check(placed > 0 && gems > 0 && steel_in > 0 && steel_out > 0 && chains > 0 && repaints > 0,
          "placements, gems, steel, chains and repaints all played");
    std::printf("markers: %d moves; totems", moves);
    for (int n : totems) std::printf(" %d", n);
    std::printf("; power-ups");
    for (int n : power_ups) std::printf(" %d", n);
    std::printf("; placed %d gems %d steel %d/%d chains %d repaints %d\n", placed, gems, steel_in, steel_out, chains,
                repaints);
}

void test_mark_packing() {
    using namespace fx::marks;
    const int m = with_power_up(kGem | kChain, fx::kHint);
    check(power_up(m) == fx::kHint && (m & kGem) && (m & kChain) && !(m & kSteel), "flags and badge pack together");
    check(power_up(with_power_up(m, -1)) == -1 && (with_power_up(m, -1) & kGem), "a badge comes off alone");
    check(power_up(kNone) == -1, "no badge");
    check(fx::MarkTracker::apply(kNone, with_power_up(0, fx::kTnt), true) == with_power_up(0, fx::kTnt), "badge on");
    check(fx::MarkTracker::apply(kGem | kSteel, kSteel, false) == kGem, "steel off");
    check(fx::totem_from_name("paint") == fx::kPaint && fx::power_up_from_name("color_clear") == fx::kColorClear,
          "contract ids");
}

}  // namespace

int main() {
    test_lead_comes_before_the_clear();
    test_empty_step_still_has_time();
    test_beats_end_inside_their_step();
    test_mark_packing();
    test_marks_follow_real_moves();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
