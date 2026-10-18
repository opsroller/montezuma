// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Bonus tests (WP-07). No framework dependency; run via ctest.
#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/bonuses.hpp"
#include "montezuma/rng.hpp"

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

constexpr BonusId kAll[] = {BonusId::Hammer,  BonusId::Lightning, BonusId::ExtraTime,
                            BonusId::Shuffle, BonusId::Dynamite,  BonusId::Freeze};

Board board_from(const std::vector<std::string>& rows) {
    Board b(static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), 7, 1);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) b.set(x, y, rows[y][x] - '0');
    return b;
}

// One cascade step per map: kind -> pieces cleared.
MoveResult move_with(std::vector<std::map<int, int>> steps) {
    MoveResult r;
    r.steps = std::move(steps);
    return r;
}

bool contains(const std::vector<Cell>& cells, Cell c) {
    return std::find(cells.begin(), cells.end(), c) != cells.end();
}

bool all_in_bounds_unique_row_major(const std::vector<Cell>& cells, const Board& b) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (!b.in_bounds(cells[i].x, cells[i].y)) return false;
        if (i > 0) {
            const Cell p = cells[i - 1];
            const Cell c = cells[i];
            if (!(p.y < c.y || (p.y == c.y && p.x < c.x))) return false;
        }
    }
    return true;
}

int cap_at_level(const BonusInventory& inv, BonusId id, int level) {
    BonusInventory copy = inv;
    while (copy.level_of(id) < level) copy.upgrade(id);
    return copy.cap(id);
}

void test_names_match_contract() {
    const char* expected[] = {"hammer", "lightning", "extra_time", "shuffle", "dynamite", "freeze"};
    for (std::size_t i = 0; i < kBonusCount; ++i) {
        check(bonus_name(kAll[i]) == expected[i], std::string("name of ") + expected[i]);
        check(bonus_from_name(expected[i]) == kAll[i], std::string("id from ") + expected[i]);
    }
    check(!bonus_from_name("Hammer").has_value(), "names are case-sensitive");
    check(!bonus_from_name("").has_value(), "empty name rejected");
}

void test_table_is_sane() {
    const auto defs = bonus_defs();
    for (std::size_t i = 0; i < kBonusCount; ++i) {
        const BonusDef& d = defs[i];
        const std::string n(bonus_name(d.id));
        check(d.id == kAll[i], n + " table row is in id order");
        check(d.earn_min_cleared >= kBonusEarnThreshold, n + " never earned below the spec's 4");
        check(d.earn_weight > 0, n + " has a positive earn weight");
        check(&bonus_def(d.id) == &d, n + " bonus_def finds its row");
    }
    check(bonus_def(BonusId::Hammer).targeted(), "hammer is targeted");
    check(bonus_def(BonusId::Lightning).targeted(), "lightning is targeted");
    check(bonus_def(BonusId::Dynamite).targeted(), "dynamite is targeted");
    check(!bonus_def(BonusId::ExtraTime).targeted(), "extra_time is untargeted");
    check(!bonus_def(BonusId::Shuffle).targeted(), "shuffle is untargeted");
    check(!bonus_def(BonusId::Freeze).targeted(), "freeze is untargeted");
}

void test_new_inventory_is_empty_at_level_one() {
    BonusInventory inv;
    for (BonusId id : kAll) {
        const std::string n(bonus_name(id));
        check(inv.count(id) == 0, n + " starts with no stock");
        check(inv.level_of(id) == kMinBonusLevel, n + " starts at level 1");
        check(inv.cap(id) == bonus_def(id).levels[0].max_stock, n + " cap is level 1's");
    }
}

void test_earning_needs_four_of_a_kind_in_one_step() {
    BonusInventory inv;
    Rng rng(1);
    check(inv.earn_from(move_with({}), rng).empty(), "empty move earns nothing");
    check(inv.earn_from(move_with({{{0, 3}}}), rng).empty(), "match of 3 earns nothing");
    check(inv.earn_from(move_with({{{0, 3}, {1, 3}}}), rng).empty(), "two kinds of 3 earn nothing");
    check(inv.earn_from(move_with({{{0, 3}}, {{0, 3}}}), rng).empty(), "3 then 3 of a kind across steps earn nothing");
    check(inv == BonusInventory{}, "nothing earned leaves inventory unchanged");

    const auto awarded = inv.earn_from(move_with({{{2, 4}}}), rng);
    check(awarded.size() == 1, "match of 4 earns one pickup");
    int total = 0;
    for (BonusId id : kAll) total += inv.count(id);
    check(total == 1, "inventory holds exactly the awarded pickup");
    check(!awarded.empty() && inv.count(awarded[0]) == 1, "awarded id is the one stored");
}

void test_earning_does_not_draw_rng_without_a_pickup() {
    BonusInventory inv;
    Rng rng(9);
    Rng untouched(9);
    inv.earn_from(move_with({{{0, 3}, {4, 3}}, {{1, 3}}}), rng);
    check(rng.next() == untouched.next(), "no qualifying step draws no random numbers");
}

void test_earning_one_pickup_per_step_and_kind() {
    BonusInventory inv;
    Rng rng(3);
    const auto awarded = inv.earn_from(move_with({{{0, 4}, {1, 4}, {2, 2}}, {{0, 6}}, {{3, 3}}}), rng);
    check(awarded.size() == 3, "two kinds of 4 in step 0 plus 6 in step 1 earn three pickups");
}

void test_big_bonuses_need_five() {
    // Over many seeds a match of 4 never yields dynamite or freeze; a match of
    // 5 yields each at least once.
    bool four_gave_big = false;
    bool five_gave_dynamite = false;
    bool five_gave_freeze = false;
    for (int s = 0; s < 500; ++s) {
        BonusInventory a;
        Rng ra(static_cast<std::uint64_t>(s));
        for (BonusId id : a.earn_from(move_with({{{0, 4}}}), ra))
            four_gave_big = four_gave_big || id == BonusId::Dynamite || id == BonusId::Freeze;
        BonusInventory b;
        Rng rb(static_cast<std::uint64_t>(s));
        for (BonusId id : b.earn_from(move_with({{{0, 5}}}), rb)) {
            five_gave_dynamite = five_gave_dynamite || id == BonusId::Dynamite;
            five_gave_freeze = five_gave_freeze || id == BonusId::Freeze;
        }
    }
    check(!four_gave_big, "match of 4 never earns dynamite or freeze");
    check(five_gave_dynamite, "match of 5 can earn dynamite");
    check(five_gave_freeze, "match of 5 can earn freeze");
}

void test_earning_respects_caps() {
    BonusInventory inv;
    Rng rng(5);
    // Fill everything a match of 4 can earn except shuffle.
    for (BonusId id : {BonusId::Hammer, BonusId::Lightning, BonusId::ExtraTime})
        while (inv.add(id)) {
        }
    for (int i = 0; i < 20; ++i) inv.earn_from(move_with({{{0, 4}}}), rng);
    check(inv.count(BonusId::Shuffle) == inv.cap(BonusId::Shuffle), "pickups go to the bonus below its cap");
    for (BonusId id : kAll)
        check(inv.count(id) <= inv.cap(id), std::string(bonus_name(id)) + " never exceeds its cap");
    check(inv.count(BonusId::Dynamite) == 0 && inv.count(BonusId::Freeze) == 0, "4s never earned 5-only bonuses");

    const BonusInventory full_for_four = inv;
    check(inv.earn_from(move_with({{{0, 4}, {1, 4}}}), rng).empty(), "every eligible bonus full: pickup lost");
    check(inv == full_for_four, "lost pickup leaves inventory unchanged");

    check(!inv.add(BonusId::Hammer), "add at cap fails");
    check(inv.count(BonusId::Hammer) == inv.cap(BonusId::Hammer), "failed add changes nothing");
}

void test_earning_is_deterministic() {
    const std::vector<MoveResult> moves = {
        move_with({{{0, 4}}}), move_with({{{1, 5}, {2, 4}}}), move_with({{{3, 3}}, {{4, 7}}}),
        move_with({{{5, 4}}, {{6, 4}}, {{0, 5}}}), move_with({{{1, 6}}})};
    auto run = [&](std::uint64_t seed) {
        BonusInventory inv;
        Rng rng(seed);
        std::vector<BonusId> all;
        for (const MoveResult& m : moves)
            for (BonusId id : inv.earn_from(m, rng)) all.push_back(id);
        return std::make_pair(all, inv);
    };
    check(run(42) == run(42), "same seed and moves give the same pickups and inventory");
    bool any_differs = false;
    for (std::uint64_t s = 1; s < 20 && !any_differs; ++s) any_differs = run(s).first != run(42).first;
    check(any_differs, "different seeds can give different pickups");
}

void test_earning_from_a_real_swap() {
    Board b = board_from({"00104", "12035", "23456", "34560", "45612"});
    auto r = b.try_swap({2, 0}, {2, 1});
    check(r.has_value(), "swap making four 0s is legal");
    check(r && !r->steps.empty() && r->steps[0].at(0) == 4, "swap clears four of kind 0");
    BonusInventory inv;
    Rng rng(7);
    check(r && !inv.earn_from(*r, rng).empty(), "a real match of 4 earns a pickup");
}

void test_use_with_no_stock_fails_cleanly() {
    Board b(8, 8, 7, 1);
    BonusInventory inv;
    for (BonusId id : kAll) {
        const UseResult u = inv.use(id, b, {{3, 3}});
        check(!u && u.error == UseError::NoStock, std::string(bonus_name(id)) + " with no stock fails");
    }
    check(inv == BonusInventory{}, "failed uses change nothing");
}

void test_use_spends_one() {
    Board b(8, 8, 7, 1);
    BonusInventory inv;
    inv.add(BonusId::Hammer);
    inv.add(BonusId::Hammer);
    const UseResult u = inv.use(BonusId::Hammer, b, {{1, 1}});
    check(static_cast<bool>(u) && u.error == UseError::None, "hammer with stock succeeds");
    check(u.effect && u.effect->bonus == BonusId::Hammer, "effect names its bonus");
    check(inv.count(BonusId::Hammer) == 1, "use spends exactly one");
}

void test_targeted_use_out_of_bounds_keeps_stock() {
    Board b(8, 8, 7, 1);
    BonusInventory inv;
    for (BonusId id : {BonusId::Hammer, BonusId::Lightning, BonusId::Dynamite}) {
        inv.add(id);
        for (Cell c : {Cell{-1, 0}, Cell{0, -1}, Cell{8, 0}, Cell{0, 8}}) {
            const UseResult u = inv.use(id, b, {c});
            check(!u && u.error == UseError::TargetOutOfBounds,
                  std::string(bonus_name(id)) + " aimed off the board fails");
        }
        check(inv.count(id) == 1, std::string(bonus_name(id)) + " keeps its stock after a bad aim");
    }
}

void test_hammer() {
    Board b(8, 8, 7, 1);
    Effect e1 = bonus_effect(BonusId::Hammer, 1, b, {{4, 2}});
    check(e1.cells_to_clear == std::vector<Cell>{{4, 2}}, "hammer L1 clears exactly the target");
    check(e1.time_delta_seconds == 0.0 && e1.freeze_seconds == 0.0 && !e1.reshuffle, "hammer has no other effect");
    Effect e2 = bonus_effect(BonusId::Hammer, 2, b, {{4, 2}});
    check(e2.cells_to_clear.size() == 5, "hammer L2 clears a plus of 5");
    check(contains(e2.cells_to_clear, {4, 1}) && contains(e2.cells_to_clear, {3, 2}) &&
              contains(e2.cells_to_clear, {5, 2}) && contains(e2.cells_to_clear, {4, 3}),
          "hammer L2 includes the four neighbours");
    check(bonus_effect(BonusId::Hammer, 3, b, {{4, 4}}).cells_to_clear.size() == 13, "hammer L3 clears a diamond of 13");
    check(bonus_effect(BonusId::Hammer, 2, b, {{0, 0}}).cells_to_clear.size() == 3, "hammer L2 clipped at a corner");
    check(bonus_effect(BonusId::Hammer, 3, b, {{7, 0}}).cells_to_clear.size() == 6, "hammer L3 clipped at a corner");
}

void test_lightning() {
    Board b(8, 6, 7, 1);
    Effect row = bonus_effect(BonusId::Lightning, 1, b, {{2, 4}, Axis::Row});
    check(row.cells_to_clear.size() == 8, "lightning L1 row clears the board's width");
    check(std::all_of(row.cells_to_clear.begin(), row.cells_to_clear.end(), [](Cell c) { return c.y == 4; }),
          "lightning L1 row stays on the target row");
    Effect col = bonus_effect(BonusId::Lightning, 1, b, {{2, 4}, Axis::Column});
    check(col.cells_to_clear.size() == 6, "lightning L1 column clears the board's height");
    check(std::all_of(col.cells_to_clear.begin(), col.cells_to_clear.end(), [](Cell c) { return c.x == 2; }),
          "lightning L1 column stays on the target column");
    check(bonus_effect(BonusId::Lightning, 2, b, {{2, 4}}).cells_to_clear.size() == 8 + 6 - 1,
          "lightning L2 clears row and column, target once");
    check(bonus_effect(BonusId::Lightning, 3, b, {{3, 3}}).cells_to_clear.size() == 3 * 8 + 3 * 6 - 9,
          "lightning L3 clears a 3-wide cross");
    check(bonus_effect(BonusId::Lightning, 3, b, {{0, 0}}).cells_to_clear.size() == 2 * 8 + 2 * 6 - 4,
          "lightning L3 clipped at a corner");
}

void test_dynamite() {
    Board b(8, 8, 7, 1);
    Effect e = bonus_effect(BonusId::Dynamite, 1, b, {{3, 3}});
    check(e.cells_to_clear.size() == 9, "dynamite L1 clears 3x3");
    check(contains(e.cells_to_clear, {2, 2}) && contains(e.cells_to_clear, {4, 4}), "dynamite L1 includes diagonals");
    check(bonus_effect(BonusId::Dynamite, 1, b, {{7, 7}}).cells_to_clear.size() == 4, "dynamite L1 clipped at a corner");
    check(bonus_effect(BonusId::Dynamite, 1, b, {{0, 3}}).cells_to_clear.size() == 6, "dynamite L1 clipped at an edge");
    check(bonus_effect(BonusId::Dynamite, 2, b, {{3, 3}}).cells_to_clear.size() == 25, "dynamite L2 clears 5x5");
    Board small(5, 3, 7, 1);
    check(bonus_effect(BonusId::Dynamite, 3, small, {{2, 1}}).cells_to_clear.size() == 15,
          "dynamite L3 on a 5x3 board clears all of it");
}

void test_effect_cells_are_clean() {
    Board b(7, 9, 7, 1);
    for (BonusId id : {BonusId::Hammer, BonusId::Lightning, BonusId::Dynamite})
        for (int level = kMinBonusLevel; level <= kMaxBonusLevel; ++level)
            for (int y = 0; y < b.height(); ++y)
                for (int x = 0; x < b.width(); ++x)
                    for (Axis axis : {Axis::Row, Axis::Column}) {
                        const Effect e = bonus_effect(id, level, b, {{x, y}, axis});
                        if (!all_in_bounds_unique_row_major(e.cells_to_clear, b) ||
                            !contains(e.cells_to_clear, {x, y}))
                            check(false, std::string(bonus_name(id)) + " L" + std::to_string(level) + " at " +
                                             std::to_string(x) + "," + std::to_string(y) +
                                             " yields unique in-bounds row-major cells including the target");
                    }
    check(true, "effect cell sweep ran");
}

void test_untargeted_bonuses() {
    Board b(8, 8, 7, 1);
    const double times[] = {10.0, 15.0, 20.0};
    const double freezes[] = {5.0, 8.0, 12.0};
    for (int level = kMinBonusLevel; level <= kMaxBonusLevel; ++level) {
        const std::string l = " L" + std::to_string(level);
        const Effect t = bonus_effect(BonusId::ExtraTime, level, b, {{99, 99}});
        check(t.time_delta_seconds == times[level - 1] && t.cells_to_clear.empty() && !t.reshuffle,
              "extra_time" + l + " adds its seconds only");
        const Effect f = bonus_effect(BonusId::Freeze, level, b);
        check(f.freeze_seconds == freezes[level - 1] && f.cells_to_clear.empty() && f.time_delta_seconds == 0.0,
              "freeze" + l + " stops the timer only");
        const Effect s = bonus_effect(BonusId::Shuffle, level, b);
        check(s.reshuffle && s.cells_to_clear.empty(), "shuffle" + l + " reshuffles only");
    }
    BonusInventory inv;
    inv.add(BonusId::ExtraTime);
    check(static_cast<bool>(inv.use(BonusId::ExtraTime, b, {{-5, 40}})), "untargeted use ignores the target");
}

void test_upgrades() {
    Board b(8, 8, 7, 1);
    BonusInventory inv;
    inv.add(BonusId::Dynamite);
    check(inv.upgrade(BonusId::Dynamite), "upgrade to level 2");
    check(inv.level_of(BonusId::Dynamite) == 2, "level is 2");
    check(inv.count(BonusId::Dynamite) == 1, "upgrade keeps stock");
    const UseResult u = inv.use(BonusId::Dynamite, b, {{3, 3}});
    check(u.effect && u.effect->cells_to_clear.size() == 25, "used dynamite takes the upgraded size");
    check(inv.upgrade(BonusId::Dynamite), "upgrade to level 3");
    check(!inv.upgrade(BonusId::Dynamite), "no upgrade past the max level");
    check(inv.level_of(BonusId::Dynamite) == kMaxBonusLevel, "level stays at max");
    for (BonusId id : kAll) {
        const std::string n(bonus_name(id));
        check(cap_at_level(inv, id, 3) >= cap_at_level(inv, id, 1), n + " cap never shrinks with upgrades");
    }
    BonusInventory s;
    const int cap1 = s.cap(BonusId::Shuffle);
    s.upgrade(BonusId::Shuffle);
    check(s.cap(BonusId::Shuffle) > cap1, "shuffle upgrade raises its cap");
    check(bonus_effect(BonusId::Hammer, 0, b, {{4, 4}}).cells_to_clear.size() == 1, "level below 1 clamps to 1");
    check(bonus_effect(BonusId::Hammer, 9, b, {{4, 4}}).cells_to_clear.size() == 13, "level above max clamps to max");
}

void test_serialisation_round_trip() {
    BonusInventory inv;
    inv.upgrade(BonusId::Lightning);
    inv.upgrade(BonusId::Freeze);
    inv.upgrade(BonusId::Freeze);
    inv.add(BonusId::Hammer);
    inv.add(BonusId::Lightning);
    inv.add(BonusId::Lightning);
    inv.add(BonusId::Freeze);
    const std::string text = inv.to_text();
    check(text.rfind("hammer 1 1\nlightning 2 2\n", 0) == 0, "text form starts with hammer then lightning");
    check(BonusInventory::from_text(text) == inv, "round trip restores the inventory");
    check(BonusInventory::try_from_text(text) == inv, "non-throwing round trip restores the inventory");
    check(BonusInventory::from_text(BonusInventory{}.to_text()) == BonusInventory{}, "empty inventory round-trips");
    check(BonusInventory::try_from_text(
              "freeze 1 3\r\n\nshuffle 0 1\ndynamite 0 1\nextra_time 0 1\nlightning 2 2\nhammer 1 1\n") == inv,
          "lines in any order, CRLF and blank lines accepted");

    const std::string bad[] = {
        "",                                                                                   // all missing
        "hammer 0 1\n",                                                                       // others missing
        "hammer 0 1\nhammer 0 1\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n",  // dup
        "axe 0 1\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n",    // unknown
        "hammer 0 4\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n", // level high
        "hammer 0 0\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n", // level low
        "hammer 9 1\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n", // over cap
        "hammer -1 1\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n",// negative
        "hammer x 1\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n", // malformed
        "hammer 0 1 7\nlightning 0 1\nextra_time 0 1\nshuffle 0 1\ndynamite 0 1\nfreeze 0 1\n",// extra field
    };
    int i = 0;
    for (const std::string& t : bad) {
        bool threw = false;
        try {
            (void)BonusInventory::from_text(t);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "bad inventory text " + std::to_string(i) + " throws invalid_argument");
        check(!BonusInventory::try_from_text(t).has_value(), "bad inventory text " + std::to_string(i) + " gives nullopt");
        ++i;
    }
}

}  // namespace

int main() {
    test_names_match_contract();
    test_table_is_sane();
    test_new_inventory_is_empty_at_level_one();
    test_earning_needs_four_of_a_kind_in_one_step();
    test_earning_does_not_draw_rng_without_a_pickup();
    test_earning_one_pickup_per_step_and_kind();
    test_big_bonuses_need_five();
    test_earning_respects_caps();
    test_earning_is_deterministic();
    test_earning_from_a_real_swap();
    test_use_with_no_stock_fails_cleanly();
    test_use_spends_one();
    test_targeted_use_out_of_bounds_keeps_stock();
    test_hammer();
    test_lightning();
    test_dynamite();
    test_effect_cells_are_clean();
    test_untargeted_bonuses();
    test_upgrades();
    test_serialisation_round_trip();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
