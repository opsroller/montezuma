// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// On-board power-up tests (WP-07), per docs/reference/rules-power-ups.md.
// No framework dependency; run via ctest.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/bonuses.hpp"
#include "montezuma/rng.hpp"
#include "montezuma/totems.hpp"

using namespace montezuma;

namespace {

int checks = 0;
int failures = 0;

bool check(bool cond, const std::string& label) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    }
    return cond;
}

// Kinds in the spec's order (glossary): 0 red .. 6 white.
enum Kind { kRed, kOrange, kYellow, kGreen, kBlue, kPurple, kWhite };

constexpr PowerUpId kAll[] = {PowerUpId::Tnt,   PowerUpId::ColorClear, PowerUpId::Lightning,
                              PowerUpId::Clock, PowerUpId::Multiplier, PowerUpId::Hint};
constexpr PowerUpTiers kTier1 = kAllPowerUpsTier1;

std::string str(Cell c) { return "(" + std::to_string(c.x) + "," + std::to_string(c.y) + ")"; }

bool contains(const std::vector<Cell>& cells, Cell c) { return std::find(cells.begin(), cells.end(), c) != cells.end(); }

Board board_from(const std::vector<std::string>& rows) {
    Board b(static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), 7, 1);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) b.set(x, y, rows[y][x] - '0');
    return b;
}

// 8x8, no run of 3 anywhere, every kind 0..5 present.
Board plain_board() {
    return board_from({
        "01234501",
        "23450123",
        "45012345",
        "01234501",
        "23450123",
        "45012345",
        "01234501",
        "23450123",
    });
}

Run run_of(int length, int kind = 0, int row = 0) {
    Run r{{}, kind};
    for (int x = 0; x < length; ++x) r.cells.push_back({x, row});
    return r;
}

std::vector<bool> mask_of(const Board& b, const std::vector<Cell>& cells) {
    std::vector<bool> m(static_cast<std::size_t>(b.width() * b.height()), false);
    for (const Cell c : cells) m[static_cast<std::size_t>(c.y * b.width() + c.x)] = true;
    return m;
}

bool row_major_unique_occupied(const std::vector<Cell>& cells, const Board& b) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (!b.in_bounds(cells[i].x, cells[i].y)) return false;
        if (b.at(cells[i].x, cells[i].y) == Board::kEmpty) return false;
        if (i > 0) {
            const Cell p = cells[i - 1];
            const Cell c = cells[i];
            if (!(p.y < c.y || (p.y == c.y && p.x < c.x))) return false;
        }
    }
    return true;
}

std::vector<Cell> cells_where(const Board& b, auto pred) {
    std::vector<Cell> out;
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x)
            if (pred(x, y)) out.push_back({x, y});
    return out;
}

// Layer invariant: every power-up sits on an occupied cell.
bool layer_on_pieces(const PowerUpLayer& layer, const Board& b) {
    for (const auto& e : layer.entries())
        if (b.at(e.cell.x, e.cell.y) == Board::kEmpty) return false;
    return true;
}

template <class F>
bool throws_invalid(F&& f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

// --- ids and table ---------------------------------------------------------

void test_names() {
    const char* expected[] = {"tnt", "color_clear", "lightning", "clock", "multiplier", "hint"};
    for (std::size_t i = 0; i < kPowerUpCount; ++i) {
        check(power_up_name(kAll[i]) == expected[i], std::string("name of ") + expected[i]);
        check(power_up_from_name(expected[i]) == kAll[i], std::string("id from ") + expected[i]);
        check(power_up_def(kAll[i]).id == kAll[i], std::string("def row of ") + expected[i]);
    }
    check(!power_up_from_name("TNT").has_value(), "names are case-sensitive");
    for (const char* gone : {"hammer", "shuffle", "dynamite", "freeze", "extra_time"})
        check(!power_up_from_name(gone).has_value(), std::string("dropped placeholder is gone: ") + gone);
    check(!power_up_from_name("").has_value(), "empty name rejected");
}

void test_table_validation() {
    check(!throws_invalid([] { validate_power_up_defs(power_up_defs()); }), "built-in table validates");

    auto expect_bad = [](std::array<PowerUpDef, kPowerUpCount> defs, const std::string& label) {
        check(throws_invalid([&] { validate_power_up_defs(defs); }), "bad table throws: " + label);
    };
    std::array<PowerUpDef, kPowerUpCount> base{};
    std::copy(power_up_defs().begin(), power_up_defs().end(), base.begin());

    auto t = base;
    std::swap(t[0], t[1]);
    expect_bad(t, "ids out of order");
    t = base;
    t[2].name = "tnt";
    expect_bad(t, "duplicate name");
    t = base;
    t[3].name = "";
    expect_bad(t, "empty name");
    t = base;
    t[4].tiers[1].score_multiplier = 0;
    expect_bad(t, "multiplier 0");
    t = base;
    t[3].tiers[0].time_seconds = -1.0;
    expect_bad(t, "negative time");
    t = base;
    t[2].tiers[0].rays = 1u << 7;
    expect_bad(t, "unknown ray bit");
    t = base;
    t[0].tiers[0].kind_steel = true;
    expect_bad(t, "steel without kind_clear");
    t = base;
    t[0].tiers[0].area_chebyshev = -2;
    expect_bad(t, "bad area");
    check(throws_invalid([&] { validate_power_up_defs(std::span<const PowerUpDef>(base.data(), 3)); }),
          "bad table throws: too few rows");

    // Tier 0 means not unlocked; 1..3 are tiers.
    for (int tier : {0, 1, 3}) {
        PowerUpTiers tiers = kTier1;
        tiers[2] = tier;
        check(!throws_invalid([&] { validate_power_up_tiers(tiers); }), "tier " + std::to_string(tier) + " accepted");
    }
    for (int bad_tier : {4, -1}) {
        PowerUpTiers tiers = kTier1;
        tiers[2] = bad_tier;
        check(throws_invalid([&] { validate_power_up_tiers(tiers); }), "tier " + std::to_string(bad_tier) + " rejected");
    }
    check(throws_invalid([] { PowerUpLayer l(0, 5); }), "0-wide layer rejected");
}

// --- earning and placement (PWR-01, PWR-02) --------------------------------

void test_earning_counts() {
    struct Case {
        std::vector<Run> runs;
        int expected;
        const char* label;
    };
    const Case cases[] = {
        {{run_of(3)}, 0, "run of 3 places nothing"},
        {{run_of(3), run_of(3, 1, 2)}, 0, "two runs of 3 (an L) place nothing"},
        {{run_of(4)}, 1, "run of 4 places one"},
        {{run_of(5)}, 2, "run of 5 places two"},
        {{run_of(6)}, 2, "run of 6 places two"},
        {{run_of(4), run_of(3, 1, 2)}, 1, "4 and 3 place one"},
        {{run_of(4), run_of(4, 1, 2)}, 2, "two runs of 4 place two"},
        {{run_of(4), run_of(5, 1, 2)}, 3, "4 and 5 earn three"},
    };
    const Board b = plain_board();
    for (const Case& c : cases) {
        check(power_ups_earned_by(c.runs) == c.expected, std::string(c.label) + " (earned)");
        PowerUpLayer layer(b);
        Rng rng(7);
        const auto placed = layer.place_for_step(c.runs, b, rng, kTier1);
        check(static_cast<int>(placed.size()) == c.expected, c.label);
        check(layer.count() == c.expected, std::string(c.label) + " (layer count)");
    }

    // A run of 3 draws nothing.
    {
        PowerUpLayer layer(b);
        Rng rng(11);
        Rng twin(11);
        const std::vector<Run> runs{run_of(3)};
        layer.place_for_step(runs, b, rng, kTier1);
        check(rng.next() == twin.next(), "run of 3 draws no random numbers");
    }
}

void test_board_cap() {
    // PWR-02 acceptance: 4 power-ups on the board, a run of 4: none added.
    const Board b = plain_board();
    PowerUpLayer layer(b);
    for (int x = 0; x < 4; ++x) layer.set({x, 5}, {PowerUpId::Clock, 1});
    Rng rng(5);
    Rng twin(5);
    const std::vector<Run> four{run_of(4)};
    check(layer.place_for_step(four, b, rng, kTier1).empty(), "PWR-02: at 4 on the board, a run of 4 adds none");
    check(layer.count() == 4 && layer.pending.empty(), "PWR-02: the extra one is dropped, not kept");
    check(rng.next() == twin.next(), "PWR-02: a capped placement draws nothing");

    // Three on the board, a run of 5 earns two: only one fits.
    PowerUpLayer three(b);
    for (int x = 0; x < 3; ++x) three.set({x, 5}, {PowerUpId::Clock, 1});
    const std::vector<Run> five{run_of(5)};
    check(three.place_for_step(five, b, rng, kTier1).size() == 1 && three.count() == 4, "PWR-02: fills up to 4");

    // The gift totem's own placements go up to 8 (TOT-15).
    PowerUpLayer gift(b);
    for (int x = 0; x < 4; ++x) gift.set({x, 5}, {PowerUpId::Clock, 1});
    check(gift.place_random(10, b, rng, kTier1).size() == 4 && gift.count() == kMaxPowerUpsOnBoardGift,
          "gift places up to 8 on the board");
    check(gift.place_for_step(five, b, rng, kTier1).empty(), "above 4, runs place none");
}

void test_placement_ids_and_tiers() {
    const Board b = plain_board();
    // Nothing unlocked: nothing placed, no draws.
    {
        PowerUpLayer layer(b);
        Rng rng(5);
        Rng twin(5);
        const PowerUpTiers none{};
        const std::vector<Run> five{run_of(5)};
        check(layer.place_for_step(five, b, rng, none).empty(), "nothing unlocked places nothing");
        check(rng.next() == twin.next(), "nothing unlocked draws nothing");
    }
    // Only unlocked ids (tier >= 1) are placed, at the player's tier.
    {
        const PowerUpTiers tiers{0, 0, 0, 2, 0, 3};
        bool ok = true;
        std::set<int> seen;
        for (std::uint64_t seed = 0; seed < 200; ++seed) {
            PowerUpLayer layer(b);
            Rng rng(seed);
            const std::vector<Run> five{run_of(5)};
            for (const auto& p : layer.place_for_step(five, b, rng, tiers)) {
                seen.insert(static_cast<int>(p.power_up.id));
                ok = ok && (p.power_up.id == PowerUpId::Clock || p.power_up.id == PowerUpId::Hint);
                ok = ok && p.power_up.tier == tiers[power_up_index(p.power_up.id)];
                ok = ok && layer.at(p.cell) == p.power_up;
            }
        }
        check(ok, "PWR-02: placed ids are unlocked, at the player's tier");
        check(seen.size() == 2, "PWR-02: every unlocked id gets picked");
    }
    // Bad inputs throw.
    {
        PowerUpLayer layer(b);
        Rng rng(1);
        PowerUpTiers bad = kTier1;
        bad[0] = 9;
        const std::vector<Run> four{run_of(4)};
        check(throws_invalid([&] { layer.place_for_step(four, b, rng, bad); }), "bad tier table throws");
        PowerUpLayer small(5, 5);
        check(throws_invalid([&] { small.place_for_step(four, b, rng, kTier1); }), "layer of the wrong size throws");
    }
}

void test_placement_only_on_plain_pieces() {
    Board b = plain_board();
    // Fill all but four cells: two with power-ups, one steel, one gem, three
    // plain. A run of 5 and a run of 4 earn three.
    const std::vector<Cell> free_cells{{2, 1}, {5, 4}, {7, 7}};
    const Cell steel{1, 1};
    const Cell gem{6, 6};
    PowerUpLayer layer(b);
    PieceMarks marks;
    marks.steel = mask_of(b, {steel});
    marks.gem = mask_of(b, {gem});
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const Cell c{x, y};
            if (!contains(free_cells, c) && c != steel && c != gem) b.set(x, y, Board::kEmpty);
        }
    Rng rng(42);
    const std::vector<Run> runs{run_of(5), run_of(4, 1, 3)};
    const auto placed = layer.place_for_step(runs, b, rng, kTier1, marks);
    std::set<std::pair<int, int>> got;
    for (const auto& p : placed) got.insert({p.cell.x, p.cell.y});
    check(got == std::set<std::pair<int, int>>{{2, 1}, {5, 4}, {7, 7}},
          "GEM-01, OBS-04: only plain pieces get power-ups (no steel, gem or empty cell)");

    // No plain piece left: earned power-ups wait for the next refill.
    const std::vector<Run> four{run_of(4)};
    check(layer.place_for_step(four, b, rng, kTier1, marks).empty(), "no plain piece: nothing placed");
    check(layer.pending.size() == 1, "no plain piece: the power-up waits");
    b.set(0, 0, kRed);  // a refill brings a plain piece
    const std::vector<Run> none;
    const auto later = layer.place_for_step(none, b, rng, kTier1, marks);
    check(later.size() == 1 && later[0].cell == Cell{0, 0} && layer.pending.empty(),
          "a waiting power-up is placed after the next refill");
}

void test_placement_draw_order_and_determinism() {
    const Board b = plain_board();
    PowerUpLayer layer(b);
    Rng rng(99);
    const std::vector<Run> runs{run_of(4), run_of(5, 1, 2)};
    const auto placed = layer.place_for_step(runs, b, rng, kTier1);

    // Reproduce with the documented draw order: id, then cell, per placement.
    Rng twin(99);
    std::vector<Cell> plain = cells_where(b, [](int, int) { return true; });
    std::vector<PowerUpPlacement> expected;
    for (int i = 0; i < 3; ++i) {
        const PowerUpId id = kAll[twin.below(6)];
        const std::size_t k = static_cast<std::size_t>(twin.below(static_cast<int>(plain.size())));
        const Cell c = plain[k];
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(k));
        expected.push_back({c, {id, 1}});
    }
    check(placed == expected, "placement follows the documented draw order");
    check(rng.next() == twin.next(), "rng left in the same state");

    PowerUpLayer again(b);
    Rng rng2(99);
    check(again.place_for_step(runs, b, rng2, kTier1) == placed, "same seed, same placements");
    check(again == layer, "same seed, same layer");
}

// --- following pieces (PWR-03) ----------------------------------------------

void test_follow_falls_and_swap() {
    const PowerUp a{PowerUpId::Tnt, 2};
    const PowerUp c{PowerUpId::Hint, 1};
    const PowerUp d{PowerUpId::Clock, 3};
    PowerUpLayer layer(4, 6);
    layer.set({1, 1}, a);  // falls 1 -> 3 in step 0, then cleared at (1,3) in step 1
    layer.set({2, 0}, c);  // swapped to (3,0), then falls to (3,2)
    layer.set({0, 4}, d);  // cleared in step 0

    MoveResult r;
    r.swap = std::array<Cell, 2>{Cell{2, 0}, Cell{3, 0}};
    CascadeStep s0;
    s0.cleared = {{1, 2}, {1, 3}, {0, 4}, {3, 1}, {3, 2}};
    std::sort(s0.cleared.begin(), s0.cleared.end(), [](Cell p, Cell q) { return p.y != q.y ? p.y < q.y : p.x < q.x; });
    s0.falls = {{{0, 3}, {0, 4}, 0}, {{1, 1}, {1, 3}, 3}, {{1, 0}, {1, 2}, 0}, {{3, 0}, {3, 2}, 4}};
    CascadeStep s1;
    s1.cleared = {{1, 3}};
    r.cascades = {s0, s1};
    r.steps = {{}, {}};

    const auto fired = layer.triggered_by(r);
    check(fired.size() == 2, "two power-ups go off");
    if (fired.size() == 2) {
        check(fired[0] == TriggeredPowerUp{{0, 4}, d, 0}, "step-0 trigger at its own cell");
        check(fired[1] == TriggeredPowerUp{{1, 3}, a, 1}, "fallen power-up goes off where it landed, in step 1");
    }
    const PowerUpLayer before = layer;
    layer.apply_swap((*r.swap)[0], (*r.swap)[1]);
    for (const CascadeStep& s : r.cascades) layer.apply_step(s);
    check(layer.count() == 1, "PWR-03: a power-up stays until its piece is cleared");
    check(layer.at({3, 2}) == c, "PWR-03: power-up follows the swap, then the fall");
    check(!layer.has({2, 0}) && !layer.has({3, 0}), "nothing left behind at the swap cells");
    check(before.triggered_by(r) == fired, "triggered_by does not change the layer");

    PowerUpLayer via_remap = before;
    const Board board(4, 6, 5, 1);
    Rng rng(1);
    via_remap.remap(r, board, rng);
    check(via_remap == layer, "remap is swap, steps, shuffle");
}

void test_follow_shuffle_and_redeal() {
    const PowerUp a{PowerUpId::Lightning, 1};
    const PowerUp b{PowerUpId::Multiplier, 2};
    const Board board(3, 3, 5, 2);
    Rng rng(3);
    PowerUpLayer layer(3, 3);
    layer.set({0, 0}, a);
    layer.set({1, 1}, b);

    MoveResult r;
    r.cascades = {CascadeStep{}};
    r.steps = {{}};
    r.shuffled = true;
    // A 3-cycle (0,0) -> (2,2) -> (1,1) -> (0,0), ordered by `to`.
    r.shuffle_map = {{{1, 1}, {0, 0}}, {{2, 2}, {1, 1}}, {{0, 0}, {2, 2}}};
    layer.apply_shuffle(r, board, rng);
    check(layer.at({2, 2}) == a, "SHUF-03: shuffle moves a power-up with its piece");
    check(layer.at({0, 0}) == b, "shuffle reads from the layer before the shuffle");
    check(!layer.has({1, 1}), "a plain piece shuffled onto a power-up's old cell leaves it plain");
    check(layer.count() == 2, "SHUF-03: a shuffle keeps every power-up");

    PowerUpLayer two(3, 3);
    two.set({0, 0}, a);
    two.set({1, 1}, b);
    MoveResult swap_pair = r;
    swap_pair.shuffle_map = {{{1, 1}, {0, 0}}, {{0, 0}, {1, 1}}};
    two.apply_shuffle(swap_pair, board, rng);
    check(two.at({0, 0}) == b && two.at({1, 1}) == a, "shuffle swaps two power-ups");

    // A redeal keeps the same power-ups, on random plain pieces of the new
    // board: one draw per power-up, in row-major order of the old layer.
    const Board big = plain_board();
    PowerUpLayer redeal(big);
    redeal.set({0, 0}, a);
    redeal.set({5, 5}, b);
    MoveResult rd = r;
    rd.shuffle_map.clear();
    rd.redealt = true;
    PieceMarks marks;
    marks.steel = mask_of(big, {{0, 0}});
    Rng rr(17);
    Rng twin(17);
    redeal.apply_shuffle(rd, big, rr, marks);
    std::vector<PowerUp> kept;
    for (const auto& e : redeal.entries()) kept.push_back(e.power_up);
    check(redeal.count() == 2 && std::count(kept.begin(), kept.end(), a) == 1 && std::count(kept.begin(), kept.end(), b) == 1,
          "SHUF-03, PWR-03: a redeal keeps the same power-ups");
    std::vector<Cell> plain = cells_where(big, [](int x, int y) { return !(x == 0 && y == 0); });
    const std::size_t i1 = static_cast<std::size_t>(twin.below(static_cast<int>(plain.size())));
    const Cell c1 = plain[i1];
    plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(i1));
    const Cell c2 = plain[static_cast<std::size_t>(twin.below(static_cast<int>(plain.size())))];
    check(redeal.at(c1) == a && redeal.at(c2) == b, "redeal places them with the documented draws, not on steel");
    check(redeal.triggered_by(rd).empty(), "a redeal sets nothing off");

    // Out-of-layer cells in a result are ignored, not undefined behaviour.
    PowerUpLayer safe(3, 3);
    safe.set({0, 0}, a);
    MoveResult odd;
    odd.swap = std::array<Cell, 2>{Cell{0, 0}, Cell{5, 0}};
    CascadeStep s;
    s.cleared = {{-1, 0}, {9, 9}};
    s.falls = {{{0, 0}, {0, 7}, 0}};
    odd.cascades = {s};
    odd.shuffled = true;
    odd.shuffle_map = {{{0, 9}, {0, 0}}};
    safe.remap(odd, board, rng);
    check(safe.count() == 0, "a fall off the layer drops the power-up");
}

// --- effects (PWR-06..11) ---------------------------------------------------

std::vector<Cell> reference_area(const Board& b, Cell at, int cheb, int manh) {
    return cells_where(b, [&](int x, int y) {
        const int dx = std::abs(x - at.x);
        const int dy = std::abs(y - at.y);
        return dx <= cheb && dy <= cheb && dx + dy <= manh;
    });
}

void test_tnt() {
    const Board b = plain_board();
    struct Case {
        Cell at;
        std::array<int, 3> sizes;
        const char* where;
    };
    const Case cases[] = {
        {{3, 4}, {9, 13, 21}, "centre"},
        {{0, 0}, {4, 6, 8}, "top-left corner"},
        {{7, 7}, {4, 6, 8}, "bottom-right corner"},
        {{7, 0}, {4, 6, 8}, "top-right corner"},
        {{0, 4}, {6, 9, 13}, "left edge"},
        {{4, 7}, {6, 9, 13}, "bottom edge"},
        {{1, 1}, {9, 11, 15}, "one in from the corner"},
    };
    const std::array<std::pair<int, int>, 3> shape{{{1, 2}, {2, 2}, {2, 3}}};
    for (const Case& c : cases) {
        for (int tier = 1; tier <= 3; ++tier) {
            const auto e = power_up_effect(PowerUpId::Tnt, tier, b, c.at);
            const std::string label = std::string("PWR-06 tnt tier ") + std::to_string(tier) + " at " + c.where;
            check(static_cast<int>(e.cells_to_clear.size()) == c.sizes[static_cast<std::size_t>(tier - 1)], label + " size");
            check(e.cells_to_clear == reference_area(b, c.at, shape[static_cast<std::size_t>(tier - 1)].first,
                                                     shape[static_cast<std::size_t>(tier - 1)].second),
                  label + " cells");
            check(row_major_unique_occupied(e.cells_to_clear, b), label + " sorted");
            check(e.time_delta_seconds == 0.0 && e.score_multiplier == 1 && e.hint_seconds == 0.0,
                  label + " no side effects");
            check(e.id == PowerUpId::Tnt && e.tier == tier, label + " id and tier");
        }
    }
    // Exact shapes at the centre of a 9x9 board.
    const Board big(9, 9, 7, 3);
    const auto l3 = power_up_effect(PowerUpId::Tnt, 3, big, {4, 4});
    for (const Cell corner : {Cell{2, 2}, Cell{6, 2}, Cell{2, 6}, Cell{6, 6}})
        check(!contains(l3.cells_to_clear, corner), "tnt tier 3 skips corner " + str(corner));
    const auto l2 = power_up_effect(PowerUpId::Tnt, 2, big, {4, 4});
    check(contains(l2.cells_to_clear, {4, 2}) && contains(l2.cells_to_clear, {5, 3}) &&
              !contains(l2.cells_to_clear, {5, 2}),
          "tnt tier 2 is a diamond");

    // Steel is cleared (OBS-03).
    PieceMarks marks;
    marks.steel = mask_of(b, {{3, 3}});
    check(contains(power_up_effect(PowerUpId::Tnt, 1, b, {3, 4}, marks).cells_to_clear, {3, 3}), "OBS-03: tnt clears steel");

    // Empty cells are skipped.
    Board holes = plain_board();
    holes.set(3, 3, Board::kEmpty);
    const auto e = power_up_effect(PowerUpId::Tnt, 1, holes, {3, 4});
    check(e.cells_to_clear.size() == 8 && row_major_unique_occupied(e.cells_to_clear, holes), "tnt skips empty cells");
}

void test_lightning() {
    const Board b = plain_board();
    auto expected = [&](Cell at, int tier) {
        return cells_where(b, [&](int x, int y) {
            const bool up = x == at.x && y <= at.y;
            const bool row = y == at.y;
            const bool col = x == at.x;
            if (tier == 1) return up;
            if (tier == 2) return up || row;
            return row || col;
        });
    };
    struct Case {
        Cell at;
        std::array<int, 3> sizes;
        const char* where;
    };
    const Case cases[] = {
        {{3, 5}, {6, 13, 15}, "inside"},
        {{0, 0}, {1, 8, 15}, "top-left corner"},
        {{7, 0}, {1, 8, 15}, "top-right corner"},
        {{7, 7}, {8, 15, 15}, "bottom-right corner"},
        {{0, 7}, {8, 15, 15}, "bottom-left corner"},
        {{0, 3}, {4, 11, 15}, "left edge"},
        {{5, 0}, {1, 8, 15}, "top edge"},
    };
    for (const Case& c : cases) {
        for (int tier = 1; tier <= 3; ++tier) {
            const auto e = power_up_effect(PowerUpId::Lightning, tier, b, c.at);
            const std::string label = std::string("PWR-08 lightning tier ") + std::to_string(tier) + " at " + c.where;
            check(static_cast<int>(e.cells_to_clear.size()) == c.sizes[static_cast<std::size_t>(tier - 1)], label + " size");
            check(e.cells_to_clear == expected(c.at, tier), label + " cells");
            check(row_major_unique_occupied(e.cells_to_clear, b), label + " sorted");
        }
    }
    // PWR-08 acceptance: tier 1 at (3, 5) clears (3, 0) to (3, 4) with (3, 5).
    check(power_up_effect(PowerUpId::Lightning, 1, b, {3, 5}).cells_to_clear ==
              std::vector<Cell>{{3, 0}, {3, 1}, {3, 2}, {3, 3}, {3, 4}, {3, 5}},
          "PWR-08 acceptance: lightning tier 1 at (3,5)");
    PieceMarks marks;
    marks.steel = mask_of(b, {{3, 1}});
    check(contains(power_up_effect(PowerUpId::Lightning, 1, b, {3, 5}, marks).cells_to_clear, {3, 1}),
          "OBS-03: lightning clears steel");
}

void test_color_clear() {
    const Board b = plain_board();
    for (int kind = 0; kind < 6; ++kind) {
        const auto same = cells_where(b, [&](int x, int y) { return b.at(x, y) == kind; });
        const auto with_neighbours = cells_where(b, [&](int x, int y) {
            for (const Cell s : same)
                if (std::abs(s.x - x) + std::abs(s.y - y) <= 1) return true;
            return false;
        });
        const Cell at = same.front();
        for (int tier = 1; tier <= 3; ++tier) {
            const auto e = power_up_effect(PowerUpId::ColorClear, tier, b, at);
            const std::string label = "PWR-07 color_clear tier " + std::to_string(tier) + " kind " + std::to_string(kind);
            check(e.cells_to_clear == (tier == 3 ? with_neighbours : same), label + " cells");
            check(row_major_unique_occupied(e.cells_to_clear, b), label + " sorted");
        }
    }

    // PWR-07 acceptance: tier 1 on a blue piece, 9 blue pieces: all 9 clear.
    Board blue = plain_board();
    const std::vector<Cell> blues{{0, 0}, {7, 0}, {3, 2}, {1, 3}, {6, 4}, {2, 5}, {5, 6}, {0, 7}, {7, 7}};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (blue.at(x, y) == kBlue) blue.set(x, y, kWhite);
    for (const Cell c : blues) blue.set(c.x, c.y, kBlue);
    check(power_up_effect(PowerUpId::ColorClear, 1, blue, {3, 2}).cells_to_clear == blues,
          "PWR-07 acceptance: all 9 blue pieces clear");

    // Steel: tier 1 skips its kind's steel, tier 2 takes it, tier 3's
    // neighbours never take steel of another kind (OBS-03).
    PieceMarks marks;
    marks.steel = mask_of(blue, {{7, 0}, {1, 0}});  // (7,0) blue steel; (1,0) white steel next to blue (0,0)
    blue.set(1, 0, kWhite);
    const auto t1 = power_up_effect(PowerUpId::ColorClear, 1, blue, {3, 2}, marks);
    check(!contains(t1.cells_to_clear, {7, 0}) && t1.cells_to_clear.size() == 8, "PWR-07: tier 1 skips its kind's steel");
    const auto t2 = power_up_effect(PowerUpId::ColorClear, 2, blue, {3, 2}, marks);
    check(contains(t2.cells_to_clear, {7, 0}) && t2.cells_to_clear.size() == 9, "PWR-07: tier 2 also clears its kind's steel");
    const auto t3 = power_up_effect(PowerUpId::ColorClear, 3, blue, {3, 2}, marks);
    check(!contains(t3.cells_to_clear, {1, 0}) && contains(t3.cells_to_clear, {0, 1}) && contains(t3.cells_to_clear, {6, 0}),
          "PWR-07: tier 3 neighbours, without other kinds' steel");

    // Edge case: its kind has only its own piece: just that piece clears.
    Board one = plain_board();  // kinds 0..5 only
    one.set(4, 4, kWhite);
    check(power_up_effect(PowerUpId::ColorClear, 1, one, {4, 4}).cells_to_clear == std::vector<Cell>{{4, 4}},
          "color_clear with only its own piece clears that piece");
    // Corners and edges: tier 3 neighbours stay on the board.
    one.set(0, 0, kWhite);
    one.set(7, 7, kWhite);
    one.set(4, 4, 0);
    one.set(7, 3, kWhite);
    const auto l3 = power_up_effect(PowerUpId::ColorClear, 3, one, {0, 0});
    check(l3.cells_to_clear == std::vector<Cell>{{0, 0}, {1, 0}, {0, 1}, {7, 2}, {6, 3}, {7, 3}, {7, 4}, {7, 6}, {6, 7}, {7, 7}},
          "color_clear tier 3 neighbours are clipped at corners and edges");
}

void test_value_effects() {
    const Board b = plain_board();
    const double clock[] = {15.0, 20.0, 25.0};
    const int mult[] = {5, 7, 10};
    const double hint[] = {20.0, 25.0, 30.0};
    for (int tier = 1; tier <= 3; ++tier) {
        const std::size_t i = static_cast<std::size_t>(tier - 1);
        for (const Cell at : {Cell{0, 0}, Cell{7, 7}, Cell{3, 4}}) {
            const auto c = power_up_effect(PowerUpId::Clock, tier, b, at);
            check(c.time_delta_seconds == clock[i] && c.cells_to_clear == std::vector<Cell>{at} &&
                      c.score_multiplier == 1 && c.hint_seconds == 0.0,
                  "PWR-09 clock tier " + std::to_string(tier));
            const auto m = power_up_effect(PowerUpId::Multiplier, tier, b, at);
            check(m.score_multiplier == mult[i] && m.cells_to_clear == std::vector<Cell>{at} && m.time_delta_seconds == 0.0,
                  "PWR-10 multiplier tier " + std::to_string(tier));
            const auto h = power_up_effect(PowerUpId::Hint, tier, b, at);
            check(h.hint_seconds == hint[i] && h.cells_to_clear == std::vector<Cell>{at} && h.score_multiplier == 1,
                  "PWR-11 hint tier " + std::to_string(tier));
        }
    }
    // Tiers clamp; out-of-bounds cells give no cells but keep values.
    check(power_up_effect(PowerUpId::Clock, 0, b, {0, 0}).time_delta_seconds == 15.0, "tier 0 clamps to 1");
    check(power_up_effect(PowerUpId::Clock, 9, b, {0, 0}).time_delta_seconds == 25.0, "tier 9 clamps to 3");
    check(power_up_effect(PowerUpId::Tnt, 9, b, {3, 4}).cells_to_clear.size() == 21, "tnt tier 9 clamps to 3");
    check(power_up_effect(PowerUpId::Tnt, 1, b, {-1, 3}).cells_to_clear.empty(), "off-board cell clears nothing");
    check(power_up_effect(PowerUpId::Multiplier, 2, b, {99, 99}).score_multiplier == 7, "off-board keeps the value");

    const TriggeredPowerUp t{{2, 2}, {PowerUpId::ColorClear, 2}, 0};
    check(power_up_effect(t, b) == power_up_effect(PowerUpId::ColorClear, 2, b, {2, 2}),
          "TriggeredPowerUp overload passes id, tier and cell");

    // PWR-10: several multipliers do not stack; the highest applies.
    const std::vector<PowerUpEffect> effects{power_up_effect(PowerUpId::Multiplier, 1, b, {0, 0}),
                                             power_up_effect(PowerUpId::Multiplier, 2, b, {1, 0}),
                                             power_up_effect(PowerUpId::Clock, 1, b, {2, 0})};
    check(move_score_multiplier(effects) == 7, "PWR-10: highest multiplier applies, no stacking");
    check(move_score_multiplier({}) == 1, "no multiplier: x1");
    check(400 * move_score_multiplier(std::vector<PowerUpEffect>{effects[0]}) == 2000,
          "PWR-10 acceptance: tier 1 on a move worth 400 scores 2000");
}

// --- triggering within the step (PWR-04, PWR-05) ----------------------------

void test_expand_clear_chain() {
    const Board b = plain_board();
    PowerUpLayer layer(b);
    // A run on row 0, x 0..2, with a tnt at (1,0). The tnt's 3x3 reaches a
    // lightning at (2,1), whose column above reaches nothing new but whose
    // own row (tier 2) reaches a clock at (7,1). A hint at (6,6) is out of
    // reach.
    layer.set({1, 0}, {PowerUpId::Tnt, 1});
    layer.set({2, 1}, {PowerUpId::Lightning, 2});
    layer.set({7, 1}, {PowerUpId::Clock, 1});
    layer.set({6, 6}, {PowerUpId::Hint, 1});
    const std::vector<Cell> run{{0, 0}, {1, 0}, {2, 0}};
    const PowerUpLayer before = layer;
    const StepClear sc = layer.expand_clear(b, run, 3);

    std::set<std::pair<int, int>> want;
    for (int x = 0; x <= 2; ++x) want.insert({x, 0});
    for (int x = 0; x <= 2; ++x) want.insert({x, 1});  // tnt 3x3 (clipped)
    for (int x = 0; x < 8; ++x) want.insert({x, 1});   // lightning row
    want.insert({2, 0});                               // lightning up (already in)
    std::set<std::pair<int, int>> got;
    for (const Cell c : sc.cells) got.insert({c.x, c.y});
    check(got == want, "PWR-04: chained areas join the same step's clear");
    check(row_major_unique_occupied(sc.cells, b), "step clear is row-major and unique");
    check(sc.triggered.size() == 3 && sc.effects.size() == 3, "PWR-04: three power-ups go off, the far one does not");
    if (sc.triggered.size() == 3) {
        check(sc.triggered[0] == TriggeredPowerUp{{1, 0}, {PowerUpId::Tnt, 1}, 3} &&
                  sc.triggered[1].power_up.id == PowerUpId::Lightning && sc.triggered[2].power_up.id == PowerUpId::Clock,
              "PWR-04: in the order reached, with the step index");
    }
    bool none_from_run = true;
    for (const Cell c : sc.power_up_cells) none_from_run = none_from_run && !contains(run, c);
    check(none_from_run && sc.power_up_cells.size() == sc.cells.size() - run.size(),
          "PWR-05: power_up_cells are the cells only power-ups added");
    check(layer == before, "expand_clear does not change the layer");

    // No power-up on the run: the clear is the run.
    PowerUpLayer empty(b);
    const StepClear plain = empty.expand_clear(b, run, 0);
    check(plain.cells == run && plain.power_up_cells.empty() && plain.triggered.empty(), "no power-up: clear is the run");
}

// Swapping (2,2) and (2,1) makes the run (0,1),(1,1),(2,1) of red; with
// "00104512" on row 1 it is a run of 4 (0..3).
Board fixture_board(bool four) {
    return board_from({
        "12345612",
        four ? "00104512" : "00134512",
        "34056134",
        "56123456",
        "12345612",
        "34561234",
        "56123456",
        "12345612",
    });
}

void test_swap_with_power_ups() {
    // PWR-04 acceptance: tnt tier 1 on a run piece, a clock in its 3x3: the
    // 3x3 and the run clear in one step and time rises 15 s.
    {
        Board b = fixture_board(false);
        PowerUpLayer layer(b);
        layer.set({0, 1}, {PowerUpId::Tnt, 1});
        layer.set({1, 2}, {PowerUpId::Clock, 1});
        Rng rng(1);
        PowerUpMoveReport rep;
        const PowerUpTiers none{};
        const auto r = swap_with_power_ups(b, layer, {2, 2}, {2, 1}, rng, none, rep);
        if (check(r.has_value(), "fixture swap is legal")) {
            const std::vector<Cell> want{{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 1}, {0, 2}, {1, 2}};
            check(r->cascades[0].cleared == want, "PWR-04 acceptance: run and 3x3 clear in step 0");
            check(rep.steps.size() == r->cascades.size() && rep.steps[0].clear.cells == want,
                  "report has one entry per step, matching the board's clear");
            check(rep.time_delta_seconds == 15.0, "PWR-04 acceptance: time rises 15 s");
            check(r->steps[0].size() > 0 && r->cascades[0].runs.size() == 1, "runs stay as found");
            int counted = 0;
            for (const auto& [k, n] : r->steps[0]) counted += n;
            check(counted == 7, "the extra cells are counted in the step");
            check(layer.count() == 0, "both power-ups are gone");
            check(r->matched_kind == kRed, "matched_kind comes from the swap's own match only");
        }
    }
    // PWR-01 acceptance: tnt unlocked, a swap makes a run of 4: one tnt
    // appears on a plain piece after that step's refill.
    {
        Board b = fixture_board(true);
        PowerUpLayer layer(b);
        Rng rng(2);
        PowerUpMoveReport rep;
        const PowerUpTiers only_tnt{1, 0, 0, 0, 0, 0};
        const auto r = swap_with_power_ups(b, layer, {2, 2}, {2, 1}, rng, only_tnt, rep);
        if (check(r.has_value(), "run-of-4 fixture swap is legal")) {
            check(r->cascades[0].runs.size() == 1 && r->cascades[0].runs[0].cells.size() == 4, "fixture makes a run of 4");
            check(rep.steps[0].placed.size() == 1 && rep.steps[0].placed[0].power_up == PowerUp{PowerUpId::Tnt, 1},
                  "PWR-01 acceptance: one tnt placed after step 0's refill");
            check(layer_on_pieces(layer, b), "placed power-up sits on a piece");
        }
    }
    // An illegal swap changes nothing.
    {
        Board b = plain_board();
        const Board copy = b;
        PowerUpLayer layer(b);
        layer.set({0, 0}, {PowerUpId::Tnt, 1});
        const PowerUpLayer before = layer;
        Rng rng(3);
        Rng twin(3);
        PowerUpMoveReport rep;
        rep.score_multiplier = 9;
        check(!swap_with_power_ups(b, layer, {0, 0}, {1, 0}, rng, kTier1, rep).has_value(), "illegal swap is empty");
        check(b.cells() == copy.cells() && layer == before && rng.next() == twin.next() && rep.score_multiplier == 1,
              "illegal swap: board, layer and rng unchanged, report reset");
        PowerUpLayer small(4, 4);
        check(throws_invalid([&] { swap_with_power_ups(b, small, {0, 0}, {1, 0}, rng, kTier1, rep); }),
              "layer of the wrong size throws");
    }
    // A totem's clear (clear_cells) sets off power-ups in step 0.
    {
        Board b = plain_board();
        PowerUpLayer layer(b);
        layer.set({4, 4}, {PowerUpId::Tnt, 1});
        Rng rng(4);
        PowerUpMoveReport rep;
        const auto r = clear_cells_with_power_ups(b, layer, {{4, 4}}, rng, kTier1, rep);
        if (check(r.has_value(), "clear_cells result")) {
            check(r->cascades[0].cleared.size() == 9, "a totem's clear sets off a power-up in the same step");
            check(rep.steps[0].clear.triggered.size() == 1, "reported");
        }
    }
    // A fire totem that hits a TNT piece sets it off (PWR-04).
    {
        Board b = plain_board();
        PowerUpLayer layer(b);
        layer.set({4, 4}, {PowerUpId::Tnt, 1});
        TotemEffect fire;
        fire.totem = TotemId::Fire;
        fire.cells_to_clear = {{4, 4}};
        Rng rng(5);
        PowerUpMoveReport rep;
        const auto r = clear_cells_with_power_ups(b, layer, prepare_board_effect(fire, b), rng, kTier1, rep);
        if (check(r.has_value(), "fire result")) {
            check(r->cascades[0].cleared.size() == 9, "fire on a TNT piece sets off the TNT");
            check(rep.steps[0].clear.triggered.size() == 1 && layer.count() == 0, "TNT reported and gone");
        }
    }
}

// --- board step hooks ----------------------------------------------------------

void test_board_hooks() {
    // Hooks that add nothing leave every result and random draw unchanged.
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        Board plain(8, 8, 6, seed);
        Board hooked(8, 8, 6, seed);
        int calls = 0;
        int refills = 0;
        StepHooks hooks;
        hooks.extend_clear = [&](const Board&, const std::vector<Cell>&, int) {
            ++calls;
            return std::vector<Cell>{};
        };
        hooks.after_refill = [&](const Board&, const CascadeStep&, int) { ++refills; };
        Rng pick(seed);
        bool same = true;
        int steps = 0;
        for (int m = 0; m < 30 && same; ++m) {
            std::vector<std::array<Cell, 2>> legal;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x + 1 < 8; ++x)
                    if (plain.is_legal_swap({x, y}, {x + 1, y})) legal.push_back({Cell{x, y}, Cell{x + 1, y}});
            if (legal.empty()) break;
            const auto s = legal[static_cast<std::size_t>(pick.below(static_cast<int>(legal.size())))];
            const auto a = plain.try_swap(s[0], s[1]);
            const auto b = hooked.try_swap(s[0], s[1], &hooks);
            same = a == b && plain.cells() == hooked.cells();
            if (a) steps += static_cast<int>(a->cascades.size());
        }
        check(same, "empty hooks change nothing (seed " + std::to_string(seed) + ")");
        check(calls == steps && refills == steps, "each hook runs once per step (seed " + std::to_string(seed) + ")");
    }
    // Extra cells join the step's clear and counts.
    Board b = plain_board();
    StepHooks hooks;
    hooks.extend_clear = [](const Board&, const std::vector<Cell>& cleared, int step) {
        std::vector<Cell> extra;
        if (step == 0 && cleared == std::vector<Cell>{{0, 7}}) extra = {{7, 7}, {7, 7}, {-1, 0}, {0, 7}};
        return extra;
    };
    const auto r = b.clear_cells({{0, 7}}, &hooks);
    check(r && r->cascades[0].cleared == std::vector<Cell>{{0, 7}, {7, 7}} && r->steps[0].size() >= 1 &&
              r->cascades[0].runs.empty(),
          "extend_clear cells are cleared once, out-of-bounds ignored");
}

// --- random play ---------------------------------------------------------------

std::optional<std::array<Cell, 2>> random_legal(Board& b, Rng& rng) {
    std::vector<std::array<Cell, 2>> legal;
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x) {
            if (x + 1 < b.width() && b.is_legal_swap({x, y}, {x + 1, y})) legal.push_back({Cell{x, y}, Cell{x + 1, y}});
            if (y + 1 < b.height() && b.is_legal_swap({x, y}, {x, y + 1})) legal.push_back({Cell{x, y}, Cell{x, y + 1}});
        }
    if (legal.empty()) return std::nullopt;
    return legal[static_cast<std::size_t>(rng.below(static_cast<int>(legal.size())))];
}

struct SweepStats {
    int moves = 0;
    int placed = 0;
    int fired = 0;
    int chained = 0;
    int shuffles = 0;
    int redeals = 0;
    bool ok = true;
    std::vector<PowerUpPlacement> log;
};

bool report_matches(const MoveResult& r, const PowerUpMoveReport& rep, int before, int after) {
    if (rep.steps.size() != r.cascades.size()) return false;
    int placed = 0;
    int fired = 0;
    for (std::size_t i = 0; i < rep.steps.size(); ++i) {
        if (rep.steps[i].clear.cells != r.cascades[i].cleared) return false;
        placed += static_cast<int>(rep.steps[i].placed.size());
        fired += static_cast<int>(rep.steps[i].clear.triggered.size());
    }
    return after == before + placed - fired || r.redealt;
}

// Plays random moves through swap_with_power_ups, with an occasional
// clear_cells_with_power_ups as a stand-in for a totem. Checks the layer
// after every move.
SweepStats sweep(int w, int h, int kinds, std::uint64_t seed, int moves) {
    SweepStats st;
    Board b(w, h, kinds, seed);
    PowerUpLayer layer(b);
    Rng rng(seed * 31 + 7);
    const PowerUpTiers tiers{3, 2, 1, 2, 3, 1};
    for (int m = 0; m < moves; ++m) {
        const auto s = random_legal(b, rng);
        if (!s) break;
        PowerUpMoveReport rep;
        const int before = layer.count();
        std::optional<MoveResult> r;
        if (m % 7 == 6) {
            r = clear_cells_with_power_ups(b, layer, {(*s)[0]}, rng, tiers, rep);
        } else {
            r = swap_with_power_ups(b, layer, (*s)[0], (*s)[1], rng, tiers, rep);
        }
        if (!r) {
            st.ok = false;
            break;
        }
        ++st.moves;
        st.shuffles += r->shuffled ? 1 : 0;
        st.redeals += r->redealt ? 1 : 0;
        if (!report_matches(*r, rep, before, layer.count())) st.ok = false;
        if (!layer_on_pieces(layer, b)) st.ok = false;
        if (layer.count() > kMaxPowerUpsOnBoard) st.ok = false;
        for (const PowerUpStep& ps : rep.steps) {
            st.fired += static_cast<int>(ps.clear.triggered.size());
            if (ps.clear.triggered.size() > 1) ++st.chained;
            st.placed += static_cast<int>(ps.placed.size());
            st.log.insert(st.log.end(), ps.placed.begin(), ps.placed.end());
            for (const PowerUpEffect& e : ps.clear.effects)
                if (e.cells_to_clear.empty()) st.ok = false;
        }
    }
    return st;
}

void test_random_play() {
    SweepStats total;
    struct Size {
        int w, h, kinds;
    };
    for (const Size s : {Size{8, 8, 7}, Size{8, 8, 5}, Size{5, 5, 6}, Size{4, 4, 5}, Size{6, 9, 6}}) {
        for (std::uint64_t seed = 1; seed <= 40; ++seed) {
            const SweepStats st = sweep(s.w, s.h, s.kinds, seed, 60);
            total.moves += st.moves;
            total.placed += st.placed;
            total.fired += st.fired;
            total.chained += st.chained;
            total.shuffles += st.shuffles;
            total.redeals += st.redeals;
            if (!st.ok)
                check(false, "sweep " + std::to_string(s.w) + "x" + std::to_string(s.h) + " k" +
                                 std::to_string(s.kinds) + " seed " + std::to_string(seed));
        }
    }
    check(total.moves > 5000, "sweep played enough moves (" + std::to_string(total.moves) + ")");
    check(total.placed > 100, "sweep placed power-ups (" + std::to_string(total.placed) + ")");
    check(total.fired > 100, "sweep set off power-ups (" + std::to_string(total.fired) + ")");
    check(total.chained > 0, "sweep chained power-ups in one step (" + std::to_string(total.chained) + ")");
    check(total.shuffles > 0, "sweep went through shuffles (" + std::to_string(total.shuffles) + ")");
    std::printf("sweep: %d moves, %d placed, %d fired, %d chained steps, %d shuffles, %d redeals\n", total.moves,
                total.placed, total.fired, total.chained, total.shuffles, total.redeals);
}

void test_determinism() {
    for (std::uint64_t seed : {3ull, 17ull, 123ull}) {
        const SweepStats a = sweep(8, 8, 6, seed, 80);
        const SweepStats b = sweep(8, 8, 6, seed, 80);
        check(a.log == b.log && a.fired == b.fired && a.moves == b.moves,
              "same seed, same power-ups through play (seed " + std::to_string(seed) + ")");
    }
    check(sweep(8, 8, 6, 3, 80).log != sweep(8, 8, 6, 4, 80).log, "different seeds differ");
}

}  // namespace

int main() {
    test_names();
    test_table_validation();
    test_earning_counts();
    test_board_cap();
    test_placement_ids_and_tiers();
    test_placement_only_on_plain_pieces();
    test_placement_draw_order_and_determinism();
    test_follow_falls_and_swap();
    test_follow_shuffle_and_redeal();
    test_tnt();
    test_lightning();
    test_color_clear();
    test_value_effects();
    test_expand_clear_chain();
    test_swap_with_power_ups();
    test_board_hooks();
    test_random_play();
    test_determinism();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
