// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Power Totem tests (rules-totems.md). No framework dependency; run via ctest.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/rng.hpp"
#include "montezuma/totems.hpp"

using namespace montezuma;

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

template <typename E, typename F>
bool throws(F&& f) {
    try {
        f();
    } catch (const E&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// Kind order: red, orange, yellow, green, blue, purple, white.
const std::vector<TotemId> kAllTotems{TotemId::Fire, TotemId::Time, TotemId::Gold, TotemId::Harvest,
                                      TotemId::Ward, TotemId::Gift, TotemId::Paint};

constexpr int kRed = 0;
constexpr int kOrange = 1;
constexpr int kYellow = 2;
constexpr int kGreen = 3;
constexpr int kBlue = 4;
constexpr int kPurple = 5;
constexpr int kWhite = 6;

std::string name(TotemId id) { return std::string(totem_name(id)); }

// Rows of digits, one per board row; '.' is an empty cell.
Board board_from(const std::vector<std::string>& rows) {
    Board b(static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), 7, 1);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x)
            b.set(x, y, rows[y][x] == '.' ? Board::kEmpty : rows[y][x] - '0');
    return b;
}

int occupied(const Board& b) {
    return static_cast<int>(std::count_if(b.cells().begin(), b.cells().end(), [](int k) { return k != Board::kEmpty; }));
}

int kinds_present(const Board& b) {
    std::set<int> kinds;
    for (int k : b.cells())
        if (k != Board::kEmpty) kinds.insert(k);
    return static_cast<int>(kinds.size());
}

std::map<int, int> counts_for(const std::vector<int>& kinds) {
    std::map<int, int> m;
    for (int k : kinds) m[k] = 3;
    return m;
}

CascadeStep step_with_runs(const std::vector<int>& kinds) {
    CascadeStep step;
    int row = 0;
    for (int k : kinds) {
        Run run{{{0, row}, {1, row}, {2, row}}, k};
        step.cleared.insert(step.cleared.end(), run.cells.begin(), run.cells.end());
        step.runs.push_back(run);
        ++row;
    }
    return step;
}

// A swap result whose own match (step 0) had runs of `kinds`, followed by a
// cascade step with runs of `cascade`.
MoveResult swap_move(const std::vector<int>& kinds, const std::vector<int>& cascade = {}) {
    MoveResult m;
    m.swap = std::array<Cell, 2>{Cell{0, 0}, Cell{1, 0}};
    m.steps.push_back(counts_for(kinds));
    m.cascades.push_back(step_with_runs(kinds));
    if (!cascade.empty()) {
        m.steps.push_back(counts_for(cascade));
        m.cascades.push_back(step_with_runs(cascade));
    }
    m.matched_kind = kinds.empty() ? -1 : kinds.front();
    return m;
}

// All seven unlocked at one tier (ward capped at its last tier).
std::vector<TotemUnlock> all_at(int tier) {
    std::vector<TotemUnlock> u;
    for (TotemId id : kAllTotems) u.push_back({id, std::min(tier, kTotemDefs[static_cast<std::size_t>(id)].tier_count)});
    return u;
}

bool cells_valid(const std::vector<Cell>& cells, const Board& b) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const Cell c = cells[i];
        if (!b.in_bounds(c.x, c.y) || b.at(c.x, c.y) == Board::kEmpty) return false;
        if (i > 0) {
            const Cell p = cells[i - 1];
            if (!(p.y < c.y || (p.y == c.y && p.x < c.x))) return false;  // sorted, unique
        }
    }
    return true;
}

bool same_effect(const TotemEffect& a, const TotemEffect& b) {
    return a.totem == b.totem && a.tier == b.tier && a.use == b.use && a.cells_to_clear == b.cells_to_clear &&
           a.time_delta_seconds == b.time_delta_seconds && a.points == b.points &&
           a.start_frenzy == b.start_frenzy && a.powerups_to_place == b.powerups_to_place &&
           a.powerup_board_cap == b.powerup_board_cap && a.collect_all_gems == b.collect_all_gems &&
           a.gems_to_place == b.gems_to_place && a.remove_chains == b.remove_chains &&
           a.cells_to_repaint == b.cells_to_repaint && a.repaint_kind == b.repaint_kind &&
           a.destroy_steel == b.destroy_steel && a.stop_steel == b.stop_steel &&
           a.clear_around_steel == b.clear_around_steel;
}

TotemEffect effect_at(TotemId id, int tier, const Board& b, Rng& rng) {
    TotemSystem s({{id, tier}});
    return s.make_effect(id, b, rng);
}

// Expected effect with only the data-only fields set; cells left empty.
TotemEffect expected(TotemId id, int tier) {
    TotemEffect e;
    e.totem = id;
    e.tier = tier;
    return e;
}

// Rng `seed` advanced by `draws` calls.
std::uint64_t next_after(std::uint64_t seed, int draws) {
    Rng r(seed);
    for (int i = 0; i < draws; ++i) r.next();
    return r.next();
}

// Fires `id` once by feeding two moves of its colour.
std::vector<TotemId> fire_once(TotemSystem& s, TotemId id) {
    s.record_move(swap_move({totem_kind(id)}));
    return s.record_move(swap_move({totem_kind(id)}));
}

// rule: TOT ids table, contracts.md kind order
void test_ids_and_colours() {
    const std::vector<std::string_view> names{"fire", "time", "gold", "harvest", "ward", "gift", "paint"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        check(totem_name(kAllTotems[i]) == names[i], "totem name " + std::string(names[i]));
        auto id = totem_from_name(names[i]);
        check(id && *id == kAllTotems[i], "totem id round-trips " + std::string(names[i]));
        check(totem_kind(kAllTotems[i]) == static_cast<int>(i), "kind index of " + std::string(names[i]));
        auto back = totem_for_kind(static_cast<int>(i));
        check(back && *back == kAllTotems[i], "kind " + std::to_string(i) + " maps back");
        check(kTotemDefs[i].id == kAllTotems[i], "default table row " + std::to_string(i) + " in kind order");
    }
    check(totem_kind(TotemId::Fire) == kRed && totem_kind(TotemId::Time) == kOrange &&
              totem_kind(TotemId::Gold) == kYellow && totem_kind(TotemId::Harvest) == kGreen &&
              totem_kind(TotemId::Ward) == kBlue && totem_kind(TotemId::Gift) == kPurple &&
              totem_kind(TotemId::Paint) == kWhite,
          "red fire, orange time, yellow gold, green harvest, blue ward, purple gift, white paint");
    check(!totem_for_kind(-1) && !totem_for_kind(7), "kinds without a totem");
    check(!totem_from_name("row_clear"), "old id rejected");
}

void test_invalid_tables_and_unlocks_rejected() {
    auto bad = [](auto edit) {
        TotemTable t = kTotemDefs;
        edit(t);
        return throws<std::invalid_argument>([&] { TotemSystem s({}, t); });
    };
    const auto fire = static_cast<std::size_t>(TotemId::Fire);
    const auto time = static_cast<std::size_t>(TotemId::Time);
    const auto gold = static_cast<std::size_t>(TotemId::Gold);
    const auto harvest = static_cast<std::size_t>(TotemId::Harvest);
    const auto ward = static_cast<std::size_t>(TotemId::Ward);
    const auto gift = static_cast<std::size_t>(TotemId::Gift);
    const auto paint = static_cast<std::size_t>(TotemId::Paint);
    check(!bad([](TotemTable&) {}), "default table accepted");
    check(bad([](TotemTable& t) { std::swap(t[0], t[1]); }), "out-of-order table rejected");
    check(bad([&](TotemTable& t) { t[fire].charges_to_fire = 0; }), "zero charges_to_fire rejected");
    check(bad([&](TotemTable& t) { t[fire].tier_count = 0; }), "zero tier_count rejected");
    check(bad([&](TotemTable& t) { t[fire].tier_count = 5; }), "tier_count over 4 rejected");
    check(bad([&](TotemTable& t) { t[time].tiers[2].uses_per_level = 0; }), "zero uses rejected");
    check(bad([&](TotemTable& t) { t[time].tiers[2].uses_per_level = 3; }), "more than two uses rejected");
    check(bad([&](TotemTable& t) { t[fire].tiers[0].fireballs[0].size = -1; }), "negative fireball size rejected");
    check(bad([&](TotemTable& t) { t[fire].tiers[3].fireballs[1].count = -1; }), "negative fireball count rejected");
    check(bad([&](TotemTable& t) { t[time].tiers[0].time_seconds = -1.0; }), "negative time rejected");
    check(bad([&](TotemTable& t) { t[time].tiers[0].time_seconds = std::numeric_limits<double>::quiet_NaN(); }),
          "NaN time rejected");
    check(bad([&](TotemTable& t) { t[gold].tiers[1].points[0] = -5; }), "negative first-use points rejected");
    check(bad([&](TotemTable& t) { t[gold].tiers[2].points[1] = -5; }), "negative second-use points rejected");
    check(bad([&](TotemTable& t) { t[gift].tiers[0].powerups_to_place = -1; }), "negative power-ups rejected");
    check(bad([&](TotemTable& t) { t[harvest].tiers[1].gems_to_place = -1; }), "negative gems rejected");
    check(bad([&](TotemTable& t) { t[paint].tiers[0].repaint_count = -1; }), "negative repaint rejected");
    check(!bad([&](TotemTable& t) { t[ward].tiers[3].uses_per_level = 0; }), "unused ward tier 4 ignored");

    check(throws<std::invalid_argument>([] { TotemSystem s({{TotemId::Ward, 4}}); }), "ward has no tier 4");
    check(throws<std::invalid_argument>([] { TotemSystem s({{TotemId::Fire, 0}}); }), "tier 0 rejected");
    check(throws<std::invalid_argument>([] { TotemSystem s({{TotemId::Fire, 5}}); }), "tier 5 rejected");
    check(!throws<std::invalid_argument>([] { TotemSystem s({{TotemId::Fire, 4}, {TotemId::Ward, 3}}); }),
          "top tiers accepted");
    check(throws<std::invalid_argument>([] { TotemSystem::from_names({"time", "hammer"}); }),
          "unknown unlocked name rejected");
    auto s = TotemSystem::from_names({"time", "paint"});
    check(s.unlocked(TotemId::Time) && s.unlocked(TotemId::Paint) && !s.unlocked(TotemId::Fire),
          "from_names unlocks the named totems only");
    check(s.tier(TotemId::Time) == 1 && s.uses_left(TotemId::Time) == 1, "from_names unlocks at tier 1");
    s.unlock(TotemId::Time, 3);
    check(s.tier(TotemId::Time) == 3 && s.uses_left(TotemId::Time) == 2, "tier set at runtime from progress");
}

// rule: TOT-01, TOT-02, TOT-04
void test_charging_per_colour() {
    TotemSystem s(all_at(1));
    check(s.record_move(swap_move({kYellow})).empty(), "one yellow move does not fire");
    check(s.charge_count(TotemId::Gold) == 1 && s.charge(TotemId::Gold) == 0.5, "gold charged to 1 of 2");
    for (TotemId id : kAllTotems)
        if (id != TotemId::Gold) check(s.charge_count(id) == 0, name(id) + " not charged by yellow");

    TotemSystem two(all_at(1));
    two.record_move(swap_move({kRed, kPurple}));
    check(two.charge_count(TotemId::Fire) == 1 && two.charge_count(TotemId::Gift) == 1,
          "TOT-04 a two-colour match charges both totems");
    check(two.charge_count(TotemId::Time) == 0, "other colours stay empty");
    auto fired = two.record_move(swap_move({kPurple, kRed}));
    check(fired == std::vector<TotemId>{TotemId::Fire, TotemId::Gift}, "both fire, in kind order");

    // TOT-02 acceptance: charge 1, step 0 blue and a cascade red -> 0.
    TotemSystem cascade(all_at(1));
    cascade.record_move(swap_move({kRed}));
    check(cascade.record_move(swap_move({kBlue}, {kRed})).empty() && cascade.charge_count(TotemId::Fire) == 0,
          "TOT-02 a cascade of the colour does not keep the charge");
    check(cascade.charge_count(TotemId::Ward) == 1, "the swap's own colour charges");

    TotemSystem cascade2(all_at(1));
    cascade2.record_move(swap_move({kOrange}, {kGreen}));
    check(cascade2.charge_count(TotemId::Harvest) == 0, "a cascade colour does not charge");

    // Only runs count: a kind cleared in step 0 without a run (a future
    // power-up's area) does not charge.
    TotemSystem runs_only(all_at(1));
    MoveResult m = swap_move({kYellow});
    m.steps[0][kGreen] = 1;
    m.cascades[0].cleared.push_back({5, 5});
    runs_only.record_move(m);
    check(runs_only.charge_count(TotemId::Gold) == 1 && runs_only.charge_count(TotemId::Harvest) == 0,
          "TOT-02 kinds come from step 0's runs");

    TotemSystem other(all_at(1));
    other.record_move(swap_move({7, 8}));
    for (TotemId id : kAllTotems) check(other.charge_count(id) == 0, "kinds 7+ charge no " + name(id));
}

// rule: TOT-02 with a real Board
void test_charging_from_real_swaps() {
    int swaps = 0;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        Board b(8, 8, 7, seed);
        TotemSystem s(all_at(1));
        for (int y = 0; y < 8 && swaps < 1000; ++y)
            for (int x = 0; x + 1 < 8; ++x) {
                Board copy = b;
                auto r = copy.try_swap({x, y}, {x + 1, y});
                if (!r) continue;
                TotemSystem t = s;
                t.record_move(*r);
                std::set<int> kinds;
                for (const Run& run : r->cascades.front().runs) kinds.insert(run.kind);
                bool ok = !kinds.empty();
                for (TotemId id : kAllTotems)
                    ok = ok && t.charge_count(id) == (kinds.count(totem_kind(id)) ? 1 : 0);
                check(ok, "seed " + std::to_string(seed) + " real swap charges its step-0 run kinds");
                ++swaps;
            }
    }
    check(swaps > 0, "found real legal swaps");
}

void test_reset_on_miss() {
    TotemSystem s({{TotemId::Ward, 1}, {TotemId::Gift, 1}});
    s.record_move(swap_move({kBlue}));
    check(s.charge_count(TotemId::Ward) == 1, "ward charged");
    check(s.record_move(swap_move({kPurple})).empty(), "a miss does not fire");
    check(s.charge_count(TotemId::Ward) == 0 && s.charge(TotemId::Ward) == 0.0, "a move without blue resets ward");
    check(s.charge_count(TotemId::Gift) == 1, "the move's colour charges gift");
    s.record_move(swap_move({kBlue}));
    auto fired = s.record_move(swap_move({kBlue}));
    check(fired == std::vector<TotemId>{TotemId::Ward}, "ward fires after two fresh blue moves");
    check(s.charge_count(TotemId::Gift) == 0, "gift reset by the blue moves");
}

// rule: TOT-03, TOT-05
void test_fires_at_two_and_resets() {
    TotemSystem s({{TotemId::Harvest, 3}});  // tier 3: two uses
    check(s.record_move(swap_move({kGreen})).empty(), "move 1 no fire");
    auto f2 = s.record_move(swap_move({kGreen}));
    check(f2 == std::vector<TotemId>{TotemId::Harvest}, "fires on the second green move");
    check(s.charge_count(TotemId::Harvest) == 0 && s.charge(TotemId::Harvest) == 0.0, "resets to 0 after firing");
    check(s.uses_left(TotemId::Harvest) == 1 && s.uses_spent(TotemId::Harvest) == 1, "a use is spent");
    check(s.record_move(swap_move({kGreen})).empty(), "TOT-03 a third green move only charges");
    check(s.charge(TotemId::Harvest) == 0.5, "HUD fraction is charge / 2");
    check(s.record_move(swap_move({kGreen})) == std::vector<TotemId>{TotemId::Harvest}, "fires again on the fourth");

    TotemSystem all(all_at(1));
    std::vector<int> every{0, 1, 2, 3, 4, 5, 6};
    all.record_move(swap_move(every));
    check(all.record_move(swap_move(every)) == kAllTotems, "TOT-05 simultaneous fires reported in kind order");

    // TOT-04 acceptance: fire and time at charge 1, red and orange at once.
    TotemSystem ft({{TotemId::Fire, 1}, {TotemId::Time, 1}});
    ft.record_move(swap_move({kRed, kOrange}));
    check(ft.apply_move(swap_move({kOrange, kRed})) == std::vector<TotemId>{TotemId::Fire, TotemId::Time},
          "TOT-04 both fire, fire first");
}

void test_locked_totems() {
    TotemSystem s({{TotemId::Time, 4}});
    std::vector<TotemId> fired_all;
    for (int i = 0; i < 20; ++i)
        for (TotemId id : s.record_move(swap_move({0, 1, 2, 3, 4, 5, 6}))) fired_all.push_back(id);
    for (TotemId id : kAllTotems) {
        if (id == TotemId::Time) continue;
        check(!s.unlocked(id) && s.charge_count(id) == 0 && s.charge(id) == 0.0 && s.uses_left(id) == 0,
              "TOT-01 locked " + name(id) + " stays empty");
    }
    check(fired_all == std::vector<TotemId>{TotemId::Time, TotemId::Time}, "only the unlocked totem fired");

    TotemSystem late;
    late.record_move(swap_move({kWhite}));
    late.unlock(TotemId::Paint, 2);
    check(late.charge_count(TotemId::Paint) == 0 && late.uses_left(TotemId::Paint) == 1,
          "unlocking does not count earlier moves");
    late.record_move(swap_move({kWhite}));
    check(late.charge_count(TotemId::Paint) == 1, "charges from the next move");
}

// rule: TOT-06
void test_uses_per_level() {
    const int expected_uses[] = {1, 1, 2, 2};
    for (TotemId id : kAllTotems) {
        const int tiers = kTotemDefs[static_cast<std::size_t>(id)].tier_count;
        check(tiers == (id == TotemId::Ward ? 3 : 4), name(id) + " tier count");
        for (int t = 1; t <= tiers; ++t) {
            TotemSystem s({{id, t}});
            const int uses = expected_uses[t - 1];
            check(s.uses_left(id) == uses, name(id) + " L" + std::to_string(t) + " uses per level");
            int fires = 0;
            for (int m = 0; m < 20; ++m) fires += static_cast<int>(s.record_move(swap_move({totem_kind(id)})).size());
            check(fires == uses, name(id) + " L" + std::to_string(t) + " fires only while uses last");
            check(s.uses_left(id) == 0 && s.charge_count(id) == 0 && s.charge(id) == 0.0,
                  name(id) + " L" + std::to_string(t) + " spent totem shows empty and stops charging");
            s.start_level();
            check(s.uses_left(id) == uses && s.uses_spent(id) == 0 && s.charge_count(id) == 0,
                  name(id) + " L" + std::to_string(t) + " start_level refills uses");
        }
    }
    // TOT-06 acceptance: time tier 1 used once, two orange moves: nothing.
    TotemSystem used({{TotemId::Time, 1}});
    fire_once(used, TotemId::Time);
    check(fire_once(used, TotemId::Time).empty() && used.charge_count(TotemId::Time) == 0,
          "TOT-06 a spent totem does not charge or fire");

    TotemSystem s({{TotemId::Fire, 1}});
    fire_once(s, TotemId::Fire);
    s.unlock(TotemId::Fire, 3);
    check(s.tier(TotemId::Fire) == 3 && s.uses_left(TotemId::Fire) == 2, "re-tiering refills uses for the new tier");
}

// rule: TOT-02, TOT-07
void test_effect_results_leave_charges() {
    TotemSystem s(all_at(1));
    s.record_move(swap_move({kRed, kGreen}));
    MoveResult empty;
    check(s.record_move(empty).empty(), "empty result fires nothing");
    MoveResult no_swap = swap_move({kRed, kGreen});
    no_swap.swap.reset();
    check(s.record_move(no_swap).empty(), "a result without a swap fires nothing");
    Board b = board_from({"0123", "1234", "2340"});
    auto cleared = b.clear_cells({{0, 0}, {1, 1}});
    check(cleared.has_value() && s.apply_move(*cleared).empty(), "a clear_cells result fires nothing");
    check(s.charge_count(TotemId::Fire) == 1 && s.charge_count(TotemId::Harvest) == 1 &&
              s.charge_count(TotemId::Time) == 0,
          "effect-only results leave charges unchanged");
    auto fired = s.record_move(swap_move({kRed, kGreen}));
    check(fired == std::vector<TotemId>{TotemId::Fire, TotemId::Harvest}, "the next swap completes the charge");
}

// 6 x 5, 30 pieces of kinds 0..4.
const std::vector<std::string> kBoard{
    "012340", "123401", "234012", "340123", "401234",
};

// rule: TOT-10, TOT-17
void test_fire_effect_every_tier() {
    Board b = board_from(kBoard);
    const int totals[] = {6, 12, 18, 20};  // 2x3, 4x3, 2x4+2x5, 4x5
    for (int t = 1; t <= 4; ++t) {
        const std::string at = "fire L" + std::to_string(t);
        Rng rng(40 + static_cast<std::uint64_t>(t));
        TotemEffect e = effect_at(TotemId::Fire, t, b, rng);
        check(static_cast<int>(e.cells_to_clear.size()) == totals[t - 1], at + " clears its fireballs' pieces");
        check(cells_valid(e.cells_to_clear, b), at + " cells in bounds, occupied, sorted, distinct");
        check(rng.next() == next_after(40 + static_cast<std::uint64_t>(t), totals[t - 1]),
              at + " draws once per piece");
        TotemEffect rest = e;
        rest.cells_to_clear.clear();
        check(same_effect(rest, expected(TotemId::Fire, t)), at + " other fields neutral");
    }
    const auto& f3 = kTotemDefs[0].tiers[2].fireballs;
    check(f3[0].count == 2 && f3[0].size == 4 && f3[1].count == 2 && f3[1].size == 5, "fire L3 is 2x4 + 2x5");

    // TOT-10 acceptance: 6 distinct pieces, same seed same 6.
    Rng a(77);
    Rng c(77);
    TotemEffect e1 = effect_at(TotemId::Fire, 1, b, a);
    TotemEffect e2 = effect_at(TotemId::Fire, 1, b, c);
    check(e1.cells_to_clear.size() == 6 && e1.cells_to_clear == e2.cells_to_clear,
          "TOT-10 tier 1 clears 6 distinct pieces, repeatable by seed");

    Board holes = board_from({"0.1", ".2.", "3.4"});
    Rng rng(9);
    TotemEffect capped = effect_at(TotemId::Fire, 4, holes, rng);
    check(capped.cells_to_clear.size() == 5 && cells_valid(capped.cells_to_clear, holes),
          "fire with fewer pieces than targets clears what there is");
    check(rng.next() == next_after(9, 5), "a capped fire draws once per piece taken");
}

// rule: TOT-11
void test_time_effect_every_tier() {
    Board b = board_from(kBoard);
    const double seconds[] = {60.0, 120.0, 180.0, 240.0};
    for (int t = 1; t <= 4; ++t) {
        Rng rng(1);
        TotemEffect want = expected(TotemId::Time, t);
        want.time_delta_seconds = seconds[t - 1];
        check(same_effect(effect_at(TotemId::Time, t, b, rng), want), "time L" + std::to_string(t));
        check(rng.next() == next_after(1, 0), "time draws nothing");
    }
}

// rule: TOT-12
void test_gold_effect_every_tier() {
    Board b = board_from(kBoard);
    // Points on the level's first and second use.
    const int points[4][2] = {{1500, 0}, {2000, 0}, {2000, 2500}, {2500, 3000}};
    for (int t = 1; t <= 4; ++t) {
        TotemSystem s({{TotemId::Gold, t}});
        const int uses = t >= 3 ? 2 : 1;
        for (int u = 1; u <= uses; ++u) {
            const std::string at = "gold L" + std::to_string(t) + " use " + std::to_string(u);
            check(fire_once(s, TotemId::Gold) == std::vector<TotemId>{TotemId::Gold}, at + " fires");
            Rng rng(1);
            TotemEffect want = expected(TotemId::Gold, t);
            want.use = u;
            want.points = points[t - 1][u - 1];
            want.start_frenzy = t >= 2;
            check(same_effect(s.make_effect(TotemId::Gold, b, rng), want), at);
            check(rng.next() == next_after(1, 0), at + " draws nothing");
        }
        s.start_level();
        fire_once(s, TotemId::Gold);
        Rng rng(1);
        check(s.make_effect(TotemId::Gold, b, rng).points == points[t - 1][0],
              "gold L" + std::to_string(t) + " a new level starts from the first use");
    }
}

// rule: TOT-13
void test_harvest_effect_every_tier() {
    Board b = board_from(kBoard);
    const int gems[] = {0, 8, 9, 10};
    for (int t = 1; t <= 4; ++t) {
        Rng rng(1);
        TotemEffect want = expected(TotemId::Harvest, t);
        want.collect_all_gems = true;
        want.gems_to_place = gems[t - 1];
        want.remove_chains = t >= 3;
        check(same_effect(effect_at(TotemId::Harvest, t, b, rng), want), "harvest L" + std::to_string(t));
    }
}

// rule: TOT-14
void test_ward_effect_every_tier() {
    Board b = board_from(kBoard);
    for (int t = 1; t <= 3; ++t) {
        Rng rng(1);
        TotemEffect want = expected(TotemId::Ward, t);
        want.destroy_steel = true;
        want.stop_steel = t >= 2;
        want.clear_around_steel = t >= 3;
        check(same_effect(effect_at(TotemId::Ward, t, b, rng), want), "ward L" + std::to_string(t));
    }
}

// rule: TOT-15
void test_gift_effect_every_tier() {
    Board b = board_from(kBoard);
    for (int t = 1; t <= 4; ++t) {
        Rng rng(1);
        TotemEffect want = expected(TotemId::Gift, t);
        want.powerups_to_place = t;
        want.powerup_board_cap = 8;
        check(same_effect(effect_at(TotemId::Gift, t, b, rng), want), "gift L" + std::to_string(t));
    }
}

// rule: TOT-16, TOT-17
void test_paint_effect_every_tier() {
    Board b = board_from(kBoard);
    const int counts[] = {4, 6, 8, 10};
    for (int t = 1; t <= 4; ++t) {
        const std::string at = "paint L" + std::to_string(t);
        const std::uint64_t seed = 70 + static_cast<std::uint64_t>(t);
        Rng rng(seed);
        Rng probe(seed);
        const int kind = probe.below(5);  // kinds present: 0..4, ascending
        TotemEffect e = effect_at(TotemId::Paint, t, b, rng);
        check(e.repaint_kind == kind, at + " target kind is the first draw among kinds present");
        check(static_cast<int>(e.cells_to_repaint.size()) == counts[t - 1], at + " repaints its count");
        check(cells_valid(e.cells_to_repaint, b), at + " cells in bounds, occupied, sorted, unique");
        bool none_already = true;
        for (Cell c : e.cells_to_repaint) none_already = none_already && b.at(c.x, c.y) != kind;
        check(none_already, at + " only pieces of other kinds");
        check(rng.next() == next_after(seed, 1 + counts[t - 1]), at + " draws kind then one per cell");
        TotemEffect rest = e;
        rest.cells_to_repaint.clear();
        rest.repaint_kind = -1;
        check(same_effect(rest, expected(TotemId::Paint, t)), at + " other fields neutral");
    }

    // The target kind is uniform among the kinds present: every one shows up.
    Board two_kinds = board_from({"001", "000", "100"});
    std::set<int> targets;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        Rng rng(seed);
        TotemEffect e = effect_at(TotemId::Paint, 4, two_kinds, rng);
        targets.insert(e.repaint_kind);
        const std::size_t others = e.repaint_kind == 0 ? 2u : 7u;
        check(e.cells_to_repaint.size() == others, "paint caps at the pieces of other kinds");
    }
    check(targets == std::set<int>{0, 1}, "both kinds present get picked");

    Board mono = board_from({"333", "333", "333"});
    TotemSystem s({{TotemId::Paint, 2}});
    Rng rng(5);
    check(fire_once(s, TotemId::Paint).size() == 1, "paint fires on a one-kind board");
    TotemEffect e = s.make_effect(TotemId::Paint, mono, rng);
    check(e.repaint_kind == -1 && e.cells_to_repaint.empty(), "one-kind board: paint does nothing");
    check(s.uses_left(TotemId::Paint) == 0, "it still spends its use");
    check(rng.next() == next_after(5, 0), "and draws nothing");

    // TOT-16 acceptance: 4 pieces now have the target kind; runs resolve.
    for (std::uint64_t seed = 0; seed < 30; ++seed) {
        Board real(8, 8, 7, seed);
        Rng r(seed);
        TotemEffect p = effect_at(TotemId::Paint, 1, real, r);
        Board painted = real;
        for (Cell c : p.cells_to_repaint) painted.set(c.x, c.y, p.repaint_kind);
        int now = 0;
        for (Cell c : p.cells_to_repaint) now += painted.at(c.x, c.y) == p.repaint_kind ? 1 : 0;
        check(now == 4 && p.cells_to_repaint.size() == 4, "TOT-16 four pieces take the target kind");
        const bool made_runs = !painted.find_runs().empty();
        auto res = apply_board_effect(p, real);
        check(res.has_value() == made_runs, "TOT-16 a repaint that makes runs resolves them as steps");
        check(real.find_runs().empty(), "TOT-16 no run is left after paint resolves");
    }
}

// rule: TOT-05, RES-07: one at a time, each on the board the previous left.
void test_firing_loop_with_real_board() {
    int boards_changed = 0;
    for (std::uint64_t seed = 0; seed < 40; ++seed) {
        Board board(8, 8, 7, seed);
        TotemSystem totems({{TotemId::Fire, 2}, {TotemId::Gold, 3}, {TotemId::Paint, 4}});
        Rng rng(seed + 100);
        totems.apply_move(swap_move({kRed, kYellow, kWhite}));
        const auto fired = totems.apply_move(swap_move({kWhite, kRed, kYellow}));
        check(fired == std::vector<TotemId>{TotemId::Fire, TotemId::Gold, TotemId::Paint},
              "seed " + std::to_string(seed) + " three totems fire in kind order");

        // The loop the game layer runs. Each effect is built on the board the
        // previous one left; each result is an effect, so it charges nothing.
        Board shadow = board;
        Rng shadow_rng = rng;
        std::vector<TotemEffect> effects;
        for (TotemId id : fired) {
            const std::vector<int> before = board.cells();
            TotemEffect e = totems.make_effect(id, board, rng);
            check(cells_valid(e.cells_to_clear, board) && cells_valid(e.cells_to_repaint, board),
                  name(id) + " targets valid on the current board");
            auto resolved = apply_board_effect(e, board);
            if (resolved) {
                check(!resolved->swap && totems.record_move(*resolved).empty(),
                      name(id) + " resolution is an effect: charges nothing");
                check(!resolved->cascades.empty() && resolved->cascades.front().cleared.size() >=
                                                         e.cells_to_clear.size(),
                      name(id) + " clears its targets in step 0");
            }
            check(board.find_runs().empty(), name(id) + " fully resolved before the next totem");
            if (board.cells() != before) ++boards_changed;
            effects.push_back(e);
        }
        check(effects.size() == 3 && effects[0].cells_to_clear.size() == 12 && effects[1].points == 2000 &&
                  effects[1].start_frenzy && effects[2].cells_to_repaint.size() == 10,
              "seed " + std::to_string(seed) + " effects at their tiers");

        // Paint built on the pre-move board (the old "one board" model) is
        // not what the loop produced once fire changed the board.
        Rng fire_rng = shadow_rng;
        TotemEffect fire = totems.make_effect(TotemId::Fire, shadow, fire_rng);
        check(fire.cells_to_clear == effects[0].cells_to_clear, "fire built first, on the move's board");
        apply_board_effect(fire, shadow);
        TotemEffect gold = totems.make_effect(TotemId::Gold, shadow, fire_rng);
        TotemEffect paint = totems.make_effect(TotemId::Paint, shadow, fire_rng);
        check(same_effect(paint, effects[2]) && same_effect(gold, effects[1]),
              "paint is chosen on the board fire left, from the same rng");
    }
    check(boards_changed > 0, "effects changed real boards");
}

void test_same_seed_same_targets() {
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        Board b(8, 8, 7, seed);
        for (TotemId id : kAllTotems) {
            for (int t = 1; t <= kTotemDefs[static_cast<std::size_t>(id)].tier_count; ++t) {
                Rng r1(seed * 7 + 1);
                Rng r2(seed * 7 + 1);
                check(same_effect(effect_at(id, t, b, r1), effect_at(id, t, b, r2)),
                      "TOT-17 seed " + std::to_string(seed) + " repeats " + name(id) + " L" + std::to_string(t));
            }
        }
    }
    Board b(8, 8, 7, 3);
    Rng r1(1);
    Rng r2(2);
    check(!same_effect(effect_at(TotemId::Fire, 4, b, r1), effect_at(TotemId::Fire, 4, b, r2)),
          "different seeds pick different fire targets");
}

void test_random_board_sweep() {
    Rng layout(2026);
    for (std::uint64_t seed = 0; seed < 300; ++seed) {
        const int w = 3 + layout.below(8);
        const int h = 3 + layout.below(8);
        Board b(w, h, 3 + layout.below(5), seed);
        const int holes = layout.below(w * h + 1);  // up to an empty board
        for (int i = 0; i < holes; ++i) b.set(layout.below(w), layout.below(h), Board::kEmpty);
        const int pieces = occupied(b);
        for (TotemId id : kAllTotems) {
            for (int t = 1; t <= kTotemDefs[static_cast<std::size_t>(id)].tier_count; ++t) {
                const std::string at = "sweep seed " + std::to_string(seed) + " " + name(id) + " L" + std::to_string(t);
                Rng rng(seed);
                TotemEffect e = effect_at(id, t, b, rng);
                check(cells_valid(e.cells_to_clear, b), at + " clear cells valid");
                check(cells_valid(e.cells_to_repaint, b), at + " repaint cells valid");
                if (id == TotemId::Fire) {
                    int total = 0;
                    for (const Fireballs& f : kTotemDefs[0].tiers[static_cast<std::size_t>(t - 1)].fireballs)
                        total += f.count * f.size;
                    check(static_cast<int>(e.cells_to_clear.size()) == std::min(total, pieces), at + " fire count");
                } else {
                    check(e.cells_to_clear.empty(), at + " clears nothing");
                }
                if (id == TotemId::Paint) {
                    check((kinds_present(b) >= 2) == (e.repaint_kind != -1), at + " kind set iff two kinds exist");
                    bool ok = true;
                    for (Cell c : e.cells_to_repaint) ok = ok && b.at(c.x, c.y) != e.repaint_kind;
                    if (e.repaint_kind >= 0) {
                        bool present = false;
                        for (int k : b.cells()) present = present || k == e.repaint_kind;
                        ok = ok && present;
                    }
                    check(ok, at + " repaint kind present, cells not already it");
                } else {
                    check(e.cells_to_repaint.empty() && e.repaint_kind == -1, at + " repaints nothing");
                }
                Board applied = b;
                apply_board_effect(e, applied);
                check(applied.find_runs().empty(), at + " applied effect leaves no run");
            }
        }
    }
}

}  // namespace

int main() {
    test_ids_and_colours();
    test_invalid_tables_and_unlocks_rejected();
    test_charging_per_colour();
    test_charging_from_real_swaps();
    test_reset_on_miss();
    test_fires_at_two_and_resets();
    test_locked_totems();
    test_uses_per_level();
    test_effect_results_leave_charges();
    test_fire_effect_every_tier();
    test_time_effect_every_tier();
    test_gold_effect_every_tier();
    test_harvest_effect_every_tier();
    test_ward_effect_every_tier();
    test_gift_effect_every_tier();
    test_paint_effect_every_tier();
    test_firing_loop_with_real_board();
    test_same_seed_same_targets();
    test_random_board_sweep();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
