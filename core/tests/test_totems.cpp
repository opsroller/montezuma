// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Power Totem tests. No framework dependency; run via ctest.
#include <cstdint>
#include <cstdio>
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

const std::vector<TotemId> kAllTotems{TotemId::RowClear, TotemId::RandomBlast, TotemId::Time,
                                      TotemId::Lightning, TotemId::ColorBomb,  TotemId::Multiplier};

// Rows of digits, one per board row; '.' is an empty cell.
Board board_from(const std::vector<std::string>& rows) {
    Board b(static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), 7, 1);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x)
            b.set(x, y, rows[y][x] == '.' ? Board::kEmpty : rows[y][x] - '0');
    return b;
}

// A table where every totem fires at `threshold`.
TotemTable table_with_threshold(int threshold) {
    TotemTable t = kTotemDefs;
    for (auto& d : t) d.threshold = threshold;
    return t;
}

bool cells_valid(const TotemEffect& e, const Board& b) {
    for (std::size_t i = 0; i < e.cells_to_clear.size(); ++i) {
        const Cell c = e.cells_to_clear[i];
        if (!b.in_bounds(c.x, c.y) || b.at(c.x, c.y) == Board::kEmpty) return false;
        if (i > 0) {
            const Cell p = e.cells_to_clear[i - 1];
            if (!(p.y < c.y || (p.y == c.y && p.x < c.x))) return false;  // sorted, unique
        }
    }
    return true;
}

bool same_effect(const TotemEffect& a, const TotemEffect& b) {
    return a.totem == b.totem && a.cells_to_clear == b.cells_to_clear &&
           a.time_delta_seconds == b.time_delta_seconds && a.score_multiplier == b.score_multiplier &&
           a.multiplier_moves == b.multiplier_moves;
}

void test_ids_round_trip() {
    const std::vector<std::string_view> names{"row_clear", "random_blast", "time",
                                              "lightning", "color_bomb",   "multiplier"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        check(totem_name(kAllTotems[i]) == names[i], "totem name " + std::string(names[i]));
        auto id = totem_from_name(names[i]);
        check(id && *id == kAllTotems[i], "totem id round-trips " + std::string(names[i]));
    }
    check(!totem_from_name("hammer"), "unknown totem name rejected");
    for (std::size_t i = 0; i < kTotemCount; ++i)
        check(kTotemDefs[i].id == kAllTotems[i], "default table row " + std::to_string(i) + " in id order");
}

void test_invalid_tables_rejected() {
    auto bad = [](auto edit) {
        TotemTable t = kTotemDefs;
        edit(t);
        return throws<std::invalid_argument>([&] { TotemSystem s({}, t); });
    };
    check(!bad([](TotemTable&) {}), "default table accepted");
    check(bad([](TotemTable& t) { t[0].threshold = 0; }), "zero threshold rejected");
    check(bad([](TotemTable& t) { t[1].blast_count = -1; }), "negative blast_count rejected");
    check(bad([](TotemTable& t) { t[5].score_multiplier = 0; }), "zero multiplier rejected");
    check(bad([](TotemTable& t) { t[5].multiplier_moves = -1; }), "negative multiplier_moves rejected");
    check(bad([](TotemTable& t) { std::swap(t[0], t[1]); }), "out-of-order table rejected");
    check(throws<std::invalid_argument>([] { TotemSystem::from_names({"time", "hammer"}); }),
          "unknown unlocked name rejected");
    auto s = TotemSystem::from_names({"time", "color_bomb"});
    check(s.unlocked(TotemId::Time) && s.unlocked(TotemId::ColorBomb) && !s.unlocked(TotemId::RowClear),
          "from_names unlocks the named totems only");
}

void test_charge_accumulates_on_same_kind_streak() {
    TotemSystem s({TotemId::Lightning}, table_with_threshold(5));
    check(s.charge(TotemId::Lightning) == 0.0, "starts empty");
    for (int i = 1; i <= 4; ++i) {
        check(s.record_match(2).empty(), "no fire below threshold, match " + std::to_string(i));
        check(s.charge_count(TotemId::Lightning) == i, "charge counts match " + std::to_string(i));
        check(s.charge(TotemId::Lightning) == i / 5.0, "charge fraction after match " + std::to_string(i));
        check(s.streak() == i && s.streak_kind() == 2, "streak tracked, match " + std::to_string(i));
    }
}

void test_streak_break_resets_charge() {
    TotemSystem s({TotemId::RowClear}, table_with_threshold(4));
    s.record_match(1);
    s.record_match(1);
    s.record_match(1);
    check(s.charge_count(TotemId::RowClear) == 3, "three in a row charged");
    check(s.record_match(4).empty(), "a different kind does not fire");
    check(s.charge_count(TotemId::RowClear) == 1, "different kind starts a new streak of 1");
    check(s.streak_kind() == 4 && s.streak() == 1, "streak now follows the new kind");
    s.record_match(4);
    check(s.charge_count(TotemId::RowClear) == 2, "new streak keeps charging");
    check(s.record_match(-1).empty(), "no-match move does not fire");
    check(s.charge_count(TotemId::RowClear) == 0 && s.streak() == 0 && s.streak_kind() == -1,
          "no-match move clears streak and charge");
}

void test_locked_totems_never_charge() {
    TotemSystem s({TotemId::Time}, table_with_threshold(2));
    std::vector<TotemId> fired_all;
    for (int i = 0; i < 20; ++i)
        for (TotemId id : s.record_match(3)) fired_all.push_back(id);
    for (TotemId id : kAllTotems) {
        if (id == TotemId::Time) continue;
        check(s.charge_count(id) == 0 && s.charge(id) == 0.0, "locked " + std::string(totem_name(id)) + " stays empty");
    }
    check(fired_all.size() == 10, "only the unlocked totem fired (every 2nd match)");
    for (TotemId id : fired_all) check(id == TotemId::Time, "fired totem is the unlocked one");

    TotemSystem none;
    for (int i = 0; i < 20; ++i) check(none.record_match(0).empty(), "nothing unlocked never fires");
}

void test_unlock_mid_streak_charges_from_next_match() {
    TotemSystem s({}, table_with_threshold(3));
    s.record_match(5);
    s.record_match(5);
    s.unlock(TotemId::Multiplier);
    check(s.charge_count(TotemId::Multiplier) == 0, "freshly unlocked totem is empty");
    s.record_match(5);
    check(s.charge_count(TotemId::Multiplier) == 1, "charges from the next match");
}

void test_fires_at_threshold_and_resets() {
    TotemSystem s({TotemId::Time, TotemId::Multiplier});  // default table: time 3, multiplier 4
    check(s.record_match(0).empty(), "match 1 no fire");
    check(s.record_match(0).empty(), "match 2 no fire");
    auto f3 = s.record_match(0);
    check(f3.size() == 1 && f3[0] == TotemId::Time, "time fires at its threshold");
    check(s.charge_count(TotemId::Time) == 0 && s.charge(TotemId::Time) == 0.0, "time resets after firing");
    check(s.charge_count(TotemId::Multiplier) == 3, "other totem keeps its charge");
    auto f4 = s.record_match(0);
    check(f4.size() == 1 && f4[0] == TotemId::Multiplier, "multiplier fires at its threshold");
    check(s.charge_count(TotemId::Time) == 1, "time recharges along the continuing streak");
    s.record_match(0);
    auto f6 = s.record_match(0);
    check(f6.size() == 1 && f6[0] == TotemId::Time, "time fires again three matches after reset");

    TotemSystem all(kAllTotems, table_with_threshold(1));
    auto fired = all.record_match(2);
    check(fired == kAllTotems, "simultaneous fires reported in id order");
}

void test_charge_fraction_stays_in_range() {
    TotemSystem s(kAllTotems);
    Rng kinds(99);
    for (int i = 0; i < 500; ++i) {
        s.record_match(kinds.below(3));
        for (TotemId id : kAllTotems) {
            const double c = s.charge(id);
            check(c >= 0.0 && c < 1.0, "charge in [0,1) for " + std::string(totem_name(id)));
        }
    }
}

const std::vector<std::string> kBoard{
    "0123",
    "1234",
    "2340",
    "3401",
    "4012",
};

void test_row_clear_targets_one_full_row() {
    Board b = board_from(kBoard);
    TotemSystem s;
    Rng rng(3);
    Rng probe(3);
    const int row = probe.below(b.height());
    auto e = s.make_effect(TotemId::RowClear, b, rng, 0);
    check(e.totem == TotemId::RowClear, "row_clear effect id");
    check(e.cells_to_clear.size() == 4, "row_clear clears a whole row");
    bool one_row = true;
    for (int x = 0; x < 4; ++x) one_row = one_row && e.cells_to_clear[x] == Cell{x, row};
    check(one_row, "row_clear clears the row the rng picked");
    check(e.time_delta_seconds == 0.0 && e.score_multiplier == 1 && e.multiplier_moves == 0,
          "row_clear has no time or multiplier");
}

void test_random_blast_picks_distinct_cells() {
    Board b = board_from(kBoard);
    TotemSystem s;
    Rng rng(11);
    auto e = s.make_effect(TotemId::RandomBlast, b, rng, 0);
    check(static_cast<int>(e.cells_to_clear.size()) == kTotemDefs[1].blast_count, "random_blast clears blast_count cells");
    check(cells_valid(e, b), "random_blast cells in bounds, sorted and unique");

    Board small = board_from({"012", "120", "201"});
    Rng rng2(11);
    auto capped = s.make_effect(TotemId::RandomBlast, small, rng2, 0);
    check(capped.cells_to_clear.size() == 8, "default blast of 8 on a 3x3 board");
    TotemTable big = kTotemDefs;
    big[1].blast_count = 50;
    TotemSystem sb({}, big);
    auto all = sb.make_effect(TotemId::RandomBlast, small, rng2, 0);
    check(all.cells_to_clear.size() == 9, "blast_count capped at the board's pieces");

    Board holes = board_from({"0.1", ".2.", "3.4"});
    TotemTable five = kTotemDefs;
    five[1].blast_count = 9;
    TotemSystem s5({}, five);
    auto e5 = s5.make_effect(TotemId::RandomBlast, holes, rng2, 0);
    check(e5.cells_to_clear.size() == 5 && cells_valid(e5, holes), "random_blast skips empty cells");
}

void test_time_adds_seconds() {
    Board b = board_from(kBoard);
    TotemSystem s;
    Rng rng(1);
    auto e = s.make_effect(TotemId::Time, b, rng, 0);
    check(e.totem == TotemId::Time && e.cells_to_clear.empty(), "time clears nothing");
    check(e.time_delta_seconds == kTotemDefs[2].time_delta_seconds, "time adds its tuned seconds");
}

void test_lightning_shapes() {
    Board b = board_from(kBoard);  // 4 wide, 5 high
    {
        TotemTable t = kTotemDefs;
        t[3].lightning_shape = LightningShape::Column;
        TotemSystem s({}, t);
        Rng rng(5);
        Rng probe(5);
        const int col = probe.below(b.width());
        auto e = s.make_effect(TotemId::Lightning, b, rng, 0);
        check(e.cells_to_clear.size() == 5, "column lightning clears a whole column");
        bool one_col = true;
        for (int y = 0; y < 5; ++y) one_col = one_col && e.cells_to_clear[y] == Cell{col, y};
        check(one_col, "column lightning hits the rng's column");
    }
    {
        TotemSystem s;  // default: cross
        Rng rng(8);
        Rng probe(8);
        const int x = probe.below(b.width());
        const int y = probe.below(b.height());
        auto e = s.make_effect(TotemId::Lightning, b, rng, 0);
        check(e.cells_to_clear.size() == 4 + 5 - 1, "cross lightning counts the centre once");
        check(cells_valid(e, b), "cross lightning cells in bounds, sorted and unique");
        bool on_cross = true;
        for (Cell c : e.cells_to_clear) on_cross = on_cross && (c.x == x || c.y == y);
        check(on_cross, "cross lightning hits only the rng's row and column");
    }
}

void test_color_bomb_targets_kind() {
    Board b = board_from(kBoard);
    TotemSystem s;
    Rng rng(2);
    Rng untouched(2);
    auto e = s.make_effect(TotemId::ColorBomb, b, rng, 0);
    const std::vector<Cell> zeros{{0, 0}, {3, 2}, {2, 3}, {1, 4}};
    check(e.cells_to_clear == zeros, "color_bomb clears every piece of the streak kind, row-major");
    check(rng.next() == untouched.next(), "color_bomb on a present kind draws no randomness");

    Board no_six = board_from({"012", "120", "201"});
    Rng r1(4);
    Rng r2(4);
    auto missing = s.make_effect(TotemId::ColorBomb, no_six, r1, 6);
    const int expected_kind = r2.below(3);  // kinds present: 0, 1, 2
    check(missing.cells_to_clear.size() == 3, "absent kind falls back to a present kind");
    bool all_kind = true;
    for (Cell c : missing.cells_to_clear) all_kind = all_kind && no_six.at(c.x, c.y) == expected_kind;
    check(all_kind, "fallback kind is the rng's pick among present kinds");
}

void test_multiplier_effect() {
    Board b = board_from(kBoard);
    TotemSystem s;
    Rng rng(1);
    auto e = s.make_effect(TotemId::Multiplier, b, rng, 0);
    check(e.cells_to_clear.empty(), "multiplier clears nothing");
    check(e.score_multiplier == kTotemDefs[5].score_multiplier && e.multiplier_moves == kTotemDefs[5].multiplier_moves,
          "multiplier carries its tuned factor and duration");
}

void test_apply_move_builds_effects() {
    Board b = board_from(kBoard);
    TotemSystem s({TotemId::ColorBomb, TotemId::Time}, table_with_threshold(2));
    Rng rng(1);
    MoveResult move;
    move.matched_kind = 0;
    check(s.apply_move(move, b, rng).empty(), "first match fires nothing");
    auto effects = s.apply_move(move, b, rng);
    check(effects.size() == 2, "both unlocked totems fire on the second match");
    check(effects.size() == 2 && effects[0].totem == TotemId::Time && effects[1].totem == TotemId::ColorBomb,
          "effects in id order");
    check(effects.size() == 2 && effects[1].cells_to_clear.size() == 4, "color_bomb aims at the matched kind");
    check(effects.size() == 2 && effects[0].time_delta_seconds > 0.0, "time effect adds time");
}

void test_same_seed_same_targets() {
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        Board b(8, 8, 7, seed);
        TotemSystem s;
        Rng r1(seed * 7 + 1);
        Rng r2(seed * 7 + 1);
        for (TotemId id : kAllTotems) {
            auto a = s.make_effect(id, b, r1, static_cast<int>(seed % 7));
            auto c = s.make_effect(id, b, r2, static_cast<int>(seed % 7));
            check(same_effect(a, c), "seed " + std::to_string(seed) + " repeats " + std::string(totem_name(id)));
        }
    }
}

void test_targets_always_in_bounds() {
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        const int w = 3 + static_cast<int>(seed % 6);
        const int h = 3 + static_cast<int>((seed / 6) % 6);
        Board b(w, h, 5, seed);
        if (seed % 3 == 0) b.set(static_cast<int>(seed) % w, 0, Board::kEmpty);
        TotemSystem s;
        Rng rng(seed);
        for (TotemId id : kAllTotems) {
            auto e = s.make_effect(id, b, rng, static_cast<int>(seed % 9) - 1);
            check(cells_valid(e, b), "seed " + std::to_string(seed) + " " + std::string(totem_name(id)) +
                                         " cells in bounds, occupied, sorted, unique");
        }
    }
}

}  // namespace

int main() {
    test_ids_round_trip();
    test_invalid_tables_rejected();
    test_charge_accumulates_on_same_kind_streak();
    test_streak_break_resets_charge();
    test_locked_totems_never_charge();
    test_unlock_mid_streak_charges_from_next_match();
    test_fires_at_threshold_and_resets();
    test_charge_fraction_stays_in_range();
    test_row_clear_targets_one_full_row();
    test_random_blast_picks_distinct_cells();
    test_time_adds_seconds();
    test_lightning_shapes();
    test_color_bomb_targets_kind();
    test_multiplier_effect();
    test_apply_move_builds_effects();
    test_same_seed_same_targets();
    test_targets_always_in_bounds();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
