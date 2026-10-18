// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Power Totem tests. No framework dependency; run via ctest.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
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

const std::vector<TotemId> kAllTotems{TotemId::Fire,    TotemId::Time,  TotemId::Gold, TotemId::Gift,
                                      TotemId::Harvest, TotemId::Paint, TotemId::Ward};

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

// A swap result whose own match (step 0) cleared `kinds`, followed by a
// cascade step that cleared `cascade`.
MoveResult swap_move(const std::vector<int>& kinds, const std::vector<int>& cascade = {}) {
    MoveResult m;
    std::map<int, int> step0;
    for (int k : kinds) step0[k] = 3;
    m.steps.push_back(step0);
    if (!cascade.empty()) {
        std::map<int, int> step1;
        for (int k : cascade) step1[k] = 3;
        m.steps.push_back(step1);
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
    return a.totem == b.totem && a.tier == b.tier && a.cells_to_clear == b.cells_to_clear &&
           a.time_delta_seconds == b.time_delta_seconds && a.points == b.points &&
           a.start_frenzy == b.start_frenzy && a.powerups_to_place == b.powerups_to_place &&
           a.collect_all_gems == b.collect_all_gems && a.gems_to_place == b.gems_to_place &&
           a.remove_chains == b.remove_chains && a.cells_to_repaint == b.cells_to_repaint &&
           a.repaint_kind == b.repaint_kind && a.destroy_steel == b.destroy_steel && a.stop_steel == b.stop_steel &&
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

void test_ids_and_colours() {
    const std::vector<std::string_view> names{"fire", "time", "gold", "gift", "harvest", "paint", "ward"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        check(totem_name(kAllTotems[i]) == names[i], "totem name " + std::string(names[i]));
        auto id = totem_from_name(names[i]);
        check(id && *id == kAllTotems[i], "totem id round-trips " + std::string(names[i]));
        check(totem_kind(kAllTotems[i]) == static_cast<int>(i), "kind index of " + std::string(names[i]));
        auto back = totem_for_kind(static_cast<int>(i));
        check(back && *back == kAllTotems[i], "kind " + std::to_string(i) + " maps back");
        check(kTotemDefs[i].id == kAllTotems[i], "default table row " + std::to_string(i) + " in kind order");
    }
    check(totem_kind(TotemId::Fire) == 0 && totem_kind(TotemId::Time) == 1 && totem_kind(TotemId::Gold) == 2 &&
              totem_kind(TotemId::Gift) == 3 && totem_kind(TotemId::Harvest) == 4 &&
              totem_kind(TotemId::Paint) == 5 && totem_kind(TotemId::Ward) == 6,
          "red fire, orange time, yellow gold, purple gift, green harvest, white paint, blue ward");
    check(!totem_for_kind(-1) && !totem_for_kind(7), "kinds without a totem");
    check(!totem_from_name("row_clear"), "old id rejected");
}

void test_invalid_tables_and_unlocks_rejected() {
    auto bad = [](auto edit) {
        TotemTable t = kTotemDefs;
        edit(t);
        return throws<std::invalid_argument>([&] { TotemSystem s({}, t); });
    };
    check(!bad([](TotemTable&) {}), "default table accepted");
    check(bad([](TotemTable& t) { std::swap(t[0], t[1]); }), "out-of-order table rejected");
    check(bad([](TotemTable& t) { t[0].charges_to_fire = 0; }), "zero charges_to_fire rejected");
    check(bad([](TotemTable& t) { t[0].tier_count = 0; }), "zero tier_count rejected");
    check(bad([](TotemTable& t) { t[0].tier_count = 5; }), "tier_count over 4 rejected");
    check(bad([](TotemTable& t) { t[1].tiers[2].uses_per_level = 0; }), "zero uses rejected");
    check(bad([](TotemTable& t) { t[0].tiers[0].fireballs[0].size = -1; }), "negative fireball size rejected");
    check(bad([](TotemTable& t) { t[0].tiers[3].fireballs[1].count = -1; }), "negative fireball count rejected");
    check(bad([](TotemTable& t) { t[1].tiers[0].time_seconds = -1.0; }), "negative time rejected");
    check(bad([](TotemTable& t) { t[1].tiers[0].time_seconds = std::numeric_limits<double>::quiet_NaN(); }), "NaN time rejected");
    check(bad([](TotemTable& t) { t[2].tiers[1].points = -5; }), "negative points rejected");
    check(bad([](TotemTable& t) { t[3].tiers[0].powerups_to_place = -1; }), "negative power-ups rejected");
    check(bad([](TotemTable& t) { t[4].tiers[1].gems_to_place = -1; }), "negative gems rejected");
    check(bad([](TotemTable& t) { t[5].tiers[0].repaint_count = -1; }), "negative repaint rejected");
    check(!bad([](TotemTable& t) { t[6].tiers[3].uses_per_level = 0; }), "unused ward tier 4 ignored");

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
}

void test_charging_per_colour() {
    TotemSystem s(all_at(1));
    check(s.record_move(swap_move({2})).empty(), "one yellow move does not fire");
    check(s.charge_count(TotemId::Gold) == 1 && s.charge(TotemId::Gold) == 0.5, "gold charged to 1 of 2");
    for (TotemId id : kAllTotems)
        if (id != TotemId::Gold) check(s.charge_count(id) == 0, name(id) + " not charged by yellow");

    TotemSystem two(all_at(1));
    two.record_move(swap_move({0, 5}));
    check(two.charge_count(TotemId::Fire) == 1 && two.charge_count(TotemId::Paint) == 1,
          "a two-colour match charges both totems");
    check(two.charge_count(TotemId::Time) == 0, "other colours stay empty");
    auto fired = two.record_move(swap_move({5, 0}));
    check(fired == std::vector<TotemId>{TotemId::Fire, TotemId::Paint}, "both fire, in kind order");

    TotemSystem cascade(all_at(1));
    cascade.record_move(swap_move({1}, {3}));
    check(cascade.charge_count(TotemId::Time) == 1, "the swap's own colour charges");
    check(cascade.charge_count(TotemId::Gift) == 0, "a cascade colour does not charge");
    check(cascade.record_move(swap_move({4}, {1})).empty() && cascade.charge_count(TotemId::Time) == 0,
          "a cascade of the colour does not keep the charge");

    TotemSystem other(all_at(1));
    other.record_move(swap_move({7, 8}));
    for (TotemId id : kAllTotems) check(other.charge_count(id) == 0, "kinds 7+ charge no " + name(id));
}

void test_reset_on_miss() {
    TotemSystem s({{TotemId::Ward, 1}, {TotemId::Gift, 1}});
    s.record_move(swap_move({6}));
    check(s.charge_count(TotemId::Ward) == 1, "ward charged");
    check(s.record_move(swap_move({3})).empty(), "a miss does not fire");
    check(s.charge_count(TotemId::Ward) == 0 && s.charge(TotemId::Ward) == 0.0, "a move without blue resets ward");
    check(s.charge_count(TotemId::Gift) == 1, "the move's colour charges gift");
    s.record_move(swap_move({6}));
    auto fired = s.record_move(swap_move({6}));
    check(fired == std::vector<TotemId>{TotemId::Ward}, "ward fires after two fresh blue moves");
    check(s.charge_count(TotemId::Gift) == 0, "gift reset by the blue moves");
}

void test_fires_at_two_and_resets() {
    TotemSystem s({{TotemId::Harvest, 3}});  // tier 3: two uses
    check(s.record_move(swap_move({4})).empty(), "move 1 no fire");
    auto f2 = s.record_move(swap_move({4}));
    check(f2 == std::vector<TotemId>{TotemId::Harvest}, "fires on the second green move");
    check(s.charge_count(TotemId::Harvest) == 0 && s.charge(TotemId::Harvest) == 0.0, "resets to 0 after firing");
    check(s.uses_left(TotemId::Harvest) == 1, "a use is spent");
    check(s.record_move(swap_move({4})).empty(), "third green move only charges");
    check(s.charge(TotemId::Harvest) == 0.5, "HUD fraction is charge / 2");
    check(s.record_move(swap_move({4})) == std::vector<TotemId>{TotemId::Harvest}, "fires again on the fourth");

    TotemSystem all(all_at(1));
    std::vector<int> every{0, 1, 2, 3, 4, 5, 6};
    all.record_move(swap_move(every));
    check(all.record_move(swap_move(every)) == kAllTotems, "simultaneous fires reported in kind order");
}

void test_locked_totems() {
    TotemSystem s({{TotemId::Time, 4}});
    std::vector<TotemId> fired_all;
    for (int i = 0; i < 20; ++i)
        for (TotemId id : s.record_move(swap_move({0, 1, 2, 3, 4, 5, 6}))) fired_all.push_back(id);
    for (TotemId id : kAllTotems) {
        if (id == TotemId::Time) continue;
        check(!s.unlocked(id) && s.charge_count(id) == 0 && s.charge(id) == 0.0 && s.uses_left(id) == 0,
              "locked " + name(id) + " stays empty");
    }
    check(fired_all == std::vector<TotemId>{TotemId::Time, TotemId::Time}, "only the unlocked totem fired");

    TotemSystem late;
    late.record_move(swap_move({5}));
    late.unlock(TotemId::Paint, 2);
    check(late.charge_count(TotemId::Paint) == 0 && late.uses_left(TotemId::Paint) == 1,
          "unlocking does not count earlier moves");
    late.record_move(swap_move({5}));
    check(late.charge_count(TotemId::Paint) == 1, "charges from the next move");
}

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
                  name(id) + " L" + std::to_string(t) + " spent totem stops charging");
            s.start_level();
            check(s.uses_left(id) == uses && s.charge_count(id) == 0,
                  name(id) + " L" + std::to_string(t) + " start_level refills uses");
        }
    }
    TotemSystem s({{TotemId::Fire, 1}});
    s.record_move(swap_move({0}));
    s.record_move(swap_move({0}));
    s.unlock(TotemId::Fire, 3);
    check(s.tier(TotemId::Fire) == 3 && s.uses_left(TotemId::Fire) == 2, "re-tiering refills uses for the new tier");
}

void test_effect_results_leave_charges() {
    TotemSystem s(all_at(1));
    s.record_move(swap_move({0, 3}));
    MoveResult effect_only;  // no steps: an effect's cascade, not a swap
    check(s.record_move(effect_only).empty(), "empty result fires nothing");
    MoveResult no_kind = swap_move({1});
    no_kind.matched_kind = -1;
    check(s.record_move(no_kind).empty(), "matched_kind -1 fires nothing");
    check(s.charge_count(TotemId::Fire) == 1 && s.charge_count(TotemId::Gift) == 1 &&
              s.charge_count(TotemId::Time) == 0,
          "effect-only results leave charges unchanged");
    Board b = board_from({"0123", "1234", "2340"});
    Rng rng(1);
    check(s.apply_move(effect_only, b, rng).empty(), "apply_move on an effect result fires nothing");
    auto fired = s.record_move(swap_move({0, 3}));
    check(fired == std::vector<TotemId>{TotemId::Fire, TotemId::Gift}, "the next swap completes the charge");
}

// 6 x 5, 30 pieces of kinds 0..4.
const std::vector<std::string> kBoard{
    "012340", "123401", "234012", "340123", "401234",
};

void test_fire_effect_every_tier() {
    Board b = board_from(kBoard);
    const int totals[] = {6, 12, 18, 20};  // 2x3, 4x3, 2x4+2x5, 4x5
    for (int t = 1; t <= 4; ++t) {
        const std::string at = "fire L" + std::to_string(t);
        Rng rng(40 + static_cast<std::uint64_t>(t));
        TotemEffect e = effect_at(TotemId::Fire, t, b, rng);
        check(static_cast<int>(e.cells_to_clear.size()) == totals[t - 1], at + " clears its fireballs' pieces");
        check(cells_valid(e.cells_to_clear, b), at + " cells in bounds, occupied, sorted, unique");
        check(rng.next() == next_after(40 + static_cast<std::uint64_t>(t), totals[t - 1]),
              at + " draws once per piece");
        TotemEffect rest = e;
        rest.cells_to_clear.clear();
        check(same_effect(rest, expected(TotemId::Fire, t)), at + " other fields neutral");
    }
    const auto& f3 = kTotemDefs[0].tiers[2].fireballs;
    check(f3[0].count == 2 && f3[0].size == 4 && f3[1].count == 2 && f3[1].size == 5, "fire L3 is 2x4 + 2x5");

    Board holes = board_from({"0.1", ".2.", "3.4"});
    Rng rng(9);
    TotemEffect capped = effect_at(TotemId::Fire, 4, holes, rng);
    check(capped.cells_to_clear.size() == 5 && cells_valid(capped.cells_to_clear, holes),
          "fire caps at the occupied pieces and skips empty cells");
    check(rng.next() == next_after(9, 5), "a capped fire draws once per piece taken");
}

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

void test_gold_effect_every_tier() {
    Board b = board_from(kBoard);
    const int points[] = {1500, 2000, 2500, 3000};
    for (int t = 1; t <= 4; ++t) {
        Rng rng(1);
        TotemEffect want = expected(TotemId::Gold, t);
        want.points = points[t - 1];
        want.start_frenzy = t >= 2;
        check(same_effect(effect_at(TotemId::Gold, t, b, rng), want), "gold L" + std::to_string(t));
    }
}

void test_gift_effect_every_tier() {
    Board b = board_from(kBoard);
    for (int t = 1; t <= 4; ++t) {
        Rng rng(1);
        TotemEffect want = expected(TotemId::Gift, t);
        want.powerups_to_place = t;
        check(same_effect(effect_at(TotemId::Gift, t, b, rng), want), "gift L" + std::to_string(t));
    }
}

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
        check(none_already, at + " skips pieces already the target kind");
        check(rng.next() == next_after(seed, 1 + counts[t - 1]), at + " draws kind then one per cell");
        TotemEffect rest = e;
        rest.cells_to_repaint.clear();
        rest.repaint_kind = -1;
        check(same_effect(rest, expected(TotemId::Paint, t)), at + " other fields neutral");
    }

    Board few = board_from({"001", "000", "100"});
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        Rng rng(seed);
        TotemEffect e = effect_at(TotemId::Paint, 4, few, rng);
        const std::size_t others = e.repaint_kind == 0 ? 2u : 7u;
        check(e.cells_to_repaint.size() == others, "paint caps at the pieces not of the target kind");
    }
    Board mono = board_from({"333", "333", "333"});
    Rng rng(5);
    TotemEffect e = effect_at(TotemId::Paint, 2, mono, rng);
    check(e.repaint_kind == 3 && e.cells_to_repaint.empty(), "one-colour board: nothing to repaint");
}

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

void test_apply_move_builds_effects() {
    Board b = board_from(kBoard);
    TotemSystem s({{TotemId::Paint, 1}, {TotemId::Fire, 2}, {TotemId::Time, 3}});
    Rng rng(3);
    check(s.apply_move(swap_move({5, 0, 1}), b, rng).empty(), "first move fires nothing");
    auto effects = s.apply_move(swap_move({1, 5, 0}), b, rng);
    check(effects.size() == 3, "all three fire on the second move");
    if (effects.size() == 3) {
        check(effects[0].totem == TotemId::Fire && effects[1].totem == TotemId::Time &&
                  effects[2].totem == TotemId::Paint,
              "effects in kind order");
        check(effects[0].tier == 2 && effects[0].cells_to_clear.size() == 12, "fire at its tier");
        check(effects[1].time_delta_seconds == 180.0, "time at its tier");
        check(effects[2].cells_to_repaint.size() == 4, "paint at its tier");
    }
    Rng r1(3);
    Rng r2(3);
    TotemSystem a({{TotemId::Fire, 2}, {TotemId::Paint, 1}});
    TotemSystem c = a;
    a.apply_move(swap_move({0, 5}), b, r1);
    c.apply_move(swap_move({0, 5}), b, r2);
    auto ea = a.apply_move(swap_move({0, 5}), b, r1);
    TotemEffect fire = c.make_effect(TotemId::Fire, b, r2);
    TotemEffect paint = c.make_effect(TotemId::Paint, b, r2);
    check(ea.size() == 2 && same_effect(ea[0], fire) && same_effect(ea[1], paint),
          "apply_move draws fire then paint from one rng");
}

void test_same_seed_same_targets() {
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        Board b(8, 8, 7, seed);
        for (TotemId id : kAllTotems) {
            for (int t = 1; t <= kTotemDefs[static_cast<std::size_t>(id)].tier_count; ++t) {
                Rng r1(seed * 7 + 1);
                Rng r2(seed * 7 + 1);
                check(same_effect(effect_at(id, t, b, r1), effect_at(id, t, b, r2)),
                      "seed " + std::to_string(seed) + " repeats " + name(id) + " L" + std::to_string(t));
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
                    check((pieces == 0) == (e.repaint_kind == -1), at + " kind set iff pieces exist");
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
            }
        }
    }
}

}  // namespace

int main() {
    test_ids_and_colours();
    test_invalid_tables_and_unlocks_rejected();
    test_charging_per_colour();
    test_reset_on_miss();
    test_fires_at_two_and_resets();
    test_locked_totems();
    test_uses_per_level();
    test_effect_results_leave_charges();
    test_fire_effect_every_tier();
    test_time_effect_every_tier();
    test_gold_effect_every_tier();
    test_gift_effect_every_tier();
    test_harvest_effect_every_tier();
    test_paint_effect_every_tier();
    test_ward_effect_every_tier();
    test_apply_move_builds_effects();
    test_same_seed_same_targets();
    test_random_board_sweep();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
