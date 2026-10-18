// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// On-board power-up tests (WP-07). No framework dependency; run via ctest.
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

constexpr PowerUpId kAll[] = {PowerUpId::Tnt,   PowerUpId::ColorClear, PowerUpId::Lightning,
                              PowerUpId::Clock, PowerUpId::Multiplier, PowerUpId::Hint};
constexpr PowerUpSet kAllUnlocked{true, true, true, true, true, true};
constexpr PowerUpTiers kTier1{1, 1, 1, 1, 1, 1};

std::string str(Cell c) { return "(" + std::to_string(c.x) + "," + std::to_string(c.y) + ")"; }

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

// A swap result whose step 0 holds the given runs.
MoveResult swap_with_runs(std::vector<Run> runs) {
    MoveResult r;
    r.swap = std::array<Cell, 2>{Cell{0, 7}, Cell{1, 7}};
    CascadeStep step;
    step.runs = std::move(runs);
    r.cascades.push_back(step);
    r.steps.push_back({});
    return r;
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

// Layer invariant: every power-up sits on an occupied cell whose piece has
// the colour recorded at placement.
bool layer_matches_board(const PowerUpLayer& layer, const Board& b) {
    for (const auto& e : layer.entries()) {
        const int kind = b.at(e.cell.x, e.cell.y);
        if (kind == Board::kEmpty || kind != e.power_up.kind) return false;
    }
    return true;
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
    check(!power_up_from_name("hammer").has_value(), "removed ids are gone");
    check(!power_up_from_name("").has_value(), "empty name rejected");
}

void test_table_validation() {
    bool threw = false;
    try {
        validate_power_up_defs(power_up_defs());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(!threw, "built-in table validates");

    auto expect_bad = [](std::array<PowerUpDef, kPowerUpCount> defs, const std::string& label) {
        bool bad = false;
        try {
            validate_power_up_defs(defs);
        } catch (const std::invalid_argument&) {
            bad = true;
        }
        check(bad, "bad table throws: " + label);
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
    t[0].tiers[0].colour_steel = true;
    expect_bad(t, "steel without colour");
    t = base;
    t[0].tiers[0].area_chebyshev = -2;
    expect_bad(t, "bad area");

    bool short_bad = false;
    try {
        validate_power_up_defs(std::span<const PowerUpDef>(base.data(), 3));
    } catch (const std::invalid_argument&) {
        short_bad = true;
    }
    check(short_bad, "bad table throws: too few rows");

    for (int bad_tier : {0, 4, -1}) {
        PowerUpTiers tiers = kTier1;
        tiers[2] = bad_tier;
        bool bad = false;
        try {
            validate_power_up_tiers(tiers);
        } catch (const std::invalid_argument&) {
            bad = true;
        }
        check(bad, "tier " + std::to_string(bad_tier) + " rejected");
    }

    bool layer_bad = false;
    try {
        PowerUpLayer l(0, 5);
    } catch (const std::invalid_argument&) {
        layer_bad = true;
    }
    check(layer_bad, "0-wide layer rejected");
}

// --- placement ---------------------------------------------------------------

void test_placement_counts() {
    const Board b = plain_board();
    struct Case {
        std::vector<Run> runs;
        int expected;
        const char* label;
    };
    const Case cases[] = {
        {{run_of(3)}, 0, "run of 3 places nothing"},
        {{run_of(3), run_of(3, 1, 2)}, 0, "two runs of 3 place nothing"},
        {{run_of(4)}, 1, "run of 4 places one"},
        {{run_of(5)}, 2, "run of 5 places two"},
        {{run_of(6)}, 2, "run of 6 places two"},
        {{run_of(4), run_of(3, 1, 2)}, 1, "4 and 3 place one"},
        {{run_of(4), run_of(5, 1, 2)}, 3, "4 and 5 place three"},
    };
    for (const Case& c : cases) {
        PowerUpLayer layer(b);
        Rng rng(7);
        const MoveResult r = swap_with_runs(c.runs);
        check(PowerUpLayer::earned_by(r) == c.expected, std::string(c.label) + " (earned_by)");
        const auto placed = layer.place_from_move(r, b, rng, kAllUnlocked, kTier1);
        check(static_cast<int>(placed.size()) == c.expected, c.label);
        check(layer.count() == c.expected, std::string(c.label) + " (layer count)");
    }

    // A run of 3 draws nothing from the rng.
    {
        PowerUpLayer layer(b);
        Rng rng(11);
        Rng twin(11);
        layer.place_from_move(swap_with_runs({run_of(3)}), b, rng, kAllUnlocked, kTier1);
        check(rng.next() == twin.next(), "run of 3 draws no random numbers");
    }

    // Every cascade step counts. [verify]
    {
        MoveResult r = swap_with_runs({run_of(3)});
        CascadeStep later;
        later.runs = {run_of(4, 2, 5)};
        r.cascades.push_back(later);
        r.steps.push_back({});
        check(PowerUpLayer::earned_by(r) == 1, "a 4-run in a later cascade step counts");
    }

    // A clear_cells result (no swap) earns nothing, even with runs. [verify]
    {
        MoveResult r = swap_with_runs({run_of(5)});
        r.swap.reset();
        PowerUpLayer layer(b);
        Rng rng(3);
        check(layer.place_from_move(r, b, rng, kAllUnlocked, kTier1).empty(), "clear_cells result earns nothing");
    }

    // Nothing unlocked: nothing placed, no draws.
    {
        PowerUpLayer layer(b);
        Rng rng(5);
        Rng twin(5);
        const PowerUpSet none{};
        check(layer.place_from_move(swap_with_runs({run_of(5)}), b, rng, none, kTier1).empty(),
              "nothing unlocked places nothing");
        check(rng.next() == twin.next(), "nothing unlocked draws nothing");
    }

    // Only unlocked ids are placed; tier comes from the lookup; kind from the board.
    {
        PowerUpSet only{};
        only[power_up_index(PowerUpId::Clock)] = true;
        only[power_up_index(PowerUpId::Hint)] = true;
        PowerUpTiers tiers{1, 2, 3, 2, 1, 3};
        bool ok = true;
        std::set<int> seen;
        for (std::uint64_t seed = 0; seed < 200; ++seed) {
            PowerUpLayer layer(b);
            Rng rng(seed);
            for (const auto& p : layer.place_from_move(swap_with_runs({run_of(5)}), b, rng, only, tiers)) {
                seen.insert(static_cast<int>(p.power_up.id));
                ok = ok && (p.power_up.id == PowerUpId::Clock || p.power_up.id == PowerUpId::Hint);
                ok = ok && p.power_up.tier == tiers[power_up_index(p.power_up.id)];
                ok = ok && p.power_up.kind == b.at(p.cell.x, p.cell.y);
                ok = ok && layer.at(p.cell) == p.power_up;
            }
        }
        check(ok, "placed ids are unlocked, with the looked-up tier and the piece's kind");
        check(seen.size() == 2, "both unlocked ids get picked");
    }

    // Bad inputs throw.
    {
        PowerUpLayer layer(b);
        Rng rng(1);
        PowerUpTiers bad = kTier1;
        bad[0] = 9;
        bool threw = false;
        try {
            layer.place_from_move(swap_with_runs({run_of(4)}), b, rng, kAllUnlocked, bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "bad tier lookup throws");
        PowerUpLayer small(5, 5);
        threw = false;
        try {
            small.place_from_move(swap_with_runs({run_of(4)}), b, rng, kAllUnlocked, kTier1);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "layer of the wrong size throws");
    }
}

void test_placement_only_on_plain_pieces() {
    const Board b = plain_board();
    // Fill all but three cells; three 5-runs earn six, so exactly the three
    // plain cells get one each and placement stops.
    const std::vector<Cell> free_cells{{2, 1}, {5, 4}, {7, 7}};
    PowerUpLayer layer(b);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (std::find(free_cells.begin(), free_cells.end(), Cell{x, y}) == free_cells.end())
                layer.set({x, y}, {PowerUpId::Clock, 1, b.at(x, y)});
    const PowerUpLayer before = layer;
    Rng rng(42);
    const auto placed =
        layer.place_from_move(swap_with_runs({run_of(5), run_of(5, 1, 3), run_of(5, 2, 6)}), b, rng, kAllUnlocked, kTier1);
    check(placed.size() == 3, "placement stops when no plain piece is left");
    std::set<std::pair<int, int>> got;
    for (const auto& p : placed) got.insert({p.cell.x, p.cell.y});
    check(got == std::set<std::pair<int, int>>{{2, 1}, {5, 4}, {7, 7}}, "only plain pieces get power-ups");
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (before.has({x, y})) check(layer.at({x, y}) == before.at({x, y}), "existing power-up kept " + str({x, y}));

    // Empty cells are never chosen.
    Board holes = plain_board();
    for (int x = 0; x < 8; ++x)
        for (int y = 0; y < 7; ++y) holes.set(x, y, Board::kEmpty);
    bool ok = true;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        PowerUpLayer l(holes);
        Rng r(seed);
        for (const auto& p : l.place_from_move(swap_with_runs({run_of(5)}), holes, r, kAllUnlocked, kTier1))
            ok = ok && p.cell.y == 7;
    }
    check(ok, "empty cells are never chosen");
}

void test_placement_draw_order_and_determinism() {
    const Board b = plain_board();
    PowerUpLayer layer(b);
    Rng rng(99);
    const auto placed = layer.place_from_move(swap_with_runs({run_of(4), run_of(5, 1, 2)}), b, rng, kAllUnlocked, kTier1);

    // Reproduce with the documented draw order: id, then cell, per placement.
    Rng twin(99);
    std::vector<Cell> plain = cells_where(b, [](int, int) { return true; });
    std::vector<PowerUpPlacement> expected;
    for (int i = 0; i < 3; ++i) {
        const PowerUpId id = kAll[twin.below(6)];
        const std::size_t k = static_cast<std::size_t>(twin.below(static_cast<int>(plain.size())));
        const Cell c = plain[k];
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(k));
        expected.push_back({c, {id, 1, b.at(c.x, c.y)}});
    }
    check(placed == expected, "placement follows the documented draw order");
    check(rng.next() == twin.next(), "rng left in the same state");

    PowerUpLayer again(b);
    Rng rng2(99);
    check(again.place_from_move(swap_with_runs({run_of(4), run_of(5, 1, 2)}), b, rng2, kAllUnlocked, kTier1) == placed,
          "same seed, same placements");
    check(again == layer, "same seed, same layer");
}

// --- remap and triggering ------------------------------------------------------

void test_remap_falls_and_swap() {
    const PowerUp a{PowerUpId::Tnt, 2, 3};
    const PowerUp c{PowerUpId::Hint, 1, 4};
    const PowerUp d{PowerUpId::Clock, 3, 5};
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
    check(fired.size() == 2, "two power-ups trigger");
    if (fired.size() == 2) {
        check(fired[0] == TriggeredPowerUp{{0, 4}, d, 0}, "step-0 trigger at its own cell");
        check(fired[1] == TriggeredPowerUp{{1, 3}, a, 1}, "fallen power-up triggers where it landed, in step 1");
    }
    const PowerUpLayer before = layer;
    layer.remap(r);
    check(layer.count() == 1, "triggered power-ups are removed");
    check(layer.at({3, 2}) == c, "power-up follows the swap, then the fall");
    check(!layer.has({2, 0}) && !layer.has({3, 0}), "nothing left behind at the swap cells");
    check(before.triggered_by(r) == fired, "triggered_by does not change the layer");
}

void test_remap_shuffle() {
    const PowerUp a{PowerUpId::Lightning, 1, 0};
    const PowerUp b{PowerUpId::Multiplier, 2, 1};
    PowerUpLayer layer(3, 3);
    layer.set({0, 0}, a);
    layer.set({1, 1}, b);

    MoveResult r;
    r.cascades = {CascadeStep{}};
    r.steps = {{}};
    r.shuffled = true;
    // A 3-cycle (0,0) -> (2,2) -> (1,1) -> (0,0), ordered by `to`.
    r.shuffle_map = {{{1, 1}, {0, 0}}, {{2, 2}, {1, 1}}, {{0, 0}, {2, 2}}};
    layer.remap(r);
    check(layer.at({2, 2}) == a, "shuffle moves a power-up with its piece");
    check(layer.at({0, 0}) == b, "shuffle reads from the layer before the shuffle");
    check(!layer.has({1, 1}), "a plain piece shuffled onto a power-up's old cell leaves it plain");
    check(layer.count() == 2, "a shuffle keeps every power-up");

    PowerUpLayer two(3, 3);
    two.set({0, 0}, a);
    two.set({1, 1}, b);
    MoveResult swap_pair = r;
    swap_pair.shuffle_map = {{{1, 1}, {0, 0}}, {{0, 0}, {1, 1}}};
    two.remap(swap_pair);
    check(two.at({0, 0}) == b && two.at({1, 1}) == a, "shuffle swaps two power-ups");

    PowerUpLayer redeal(3, 3);
    redeal.set({0, 0}, a);
    MoveResult rd = r;
    rd.shuffle_map.clear();
    rd.redealt = true;
    redeal.remap(rd);
    check(redeal.count() == 0, "a redeal drops every power-up [verify]");
    check(redeal.triggered_by(rd).empty(), "a redeal triggers nothing");

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
    safe.remap(odd);
    check(safe.count() == 0, "a fall off the layer drops the power-up");
}

void test_trigger_on_real_match() {
    // Swapping (2,1) and (2,2) completes a horizontal 3-run of 0 on row 1.
    Board b = board_from({
        "12345612",
        "00134512",
        "34056134",
        "56123456",
        "12345612",
        "34561234",
        "56123456",
        "12345612",
    });
    PowerUpLayer layer(b);
    const PowerUp tnt{PowerUpId::Tnt, 1, 0};
    const PowerUp clock{PowerUpId::Clock, 1, b.at(4, 4)};
    layer.set({0, 1}, tnt);
    layer.set({4, 4}, clock);
    auto r = b.try_swap({2, 2}, {2, 1});
    if (!check(r.has_value(), "fixture swap is legal")) return;
    const auto fired = layer.triggered_by(*r);
    bool tnt_fired = false;
    for (const auto& f : fired)
        if (f.power_up == tnt) tnt_fired = f.cell == Cell{0, 1} && f.step == 0;
    check(tnt_fired, "a matched piece triggers its power-up at its cell");
    layer.remap(*r);
    check(!layer.has({0, 1}) || layer.at({0, 1}) != tnt, "triggered power-up is gone after remap");
    check(layer_matches_board(layer, b), "layer matches the board after a real move");

    // An effect: clear_cells triggers what it clears.
    const auto now = layer.entries();
    if (check(!now.empty(), "a power-up survives to test clear_cells")) {
        const Cell target = now.front().cell;
        auto e = b.clear_cells({target});
        if (check(e.has_value(), "clear_cells result")) {
            const auto f2 = layer.triggered_by(*e);
            check(!f2.empty() && f2.front().cell == target && f2.front().step == 0, "clear_cells triggers the power-up");
            check(PowerUpLayer::earned_by(*e) == 0, "clear_cells earns nothing");
            layer.remap(*e);
            check(layer_matches_board(layer, b), "layer matches the board after clear_cells");
        }
    }
}

// --- effects -------------------------------------------------------------------

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
            const std::string label = std::string("tnt L") + std::to_string(tier) + " at " + c.where;
            check(static_cast<int>(e.cells_to_clear.size()) == c.sizes[static_cast<std::size_t>(tier - 1)], label + " size");
            check(e.cells_to_clear == reference_area(b, c.at, shape[static_cast<std::size_t>(tier - 1)].first,
                                                     shape[static_cast<std::size_t>(tier - 1)].second),
                  label + " cells");
            check(row_major_unique_occupied(e.cells_to_clear, b), label + " sorted");
            check(e.time_delta_seconds == 0.0 && e.score_multiplier == 1 && e.hint_seconds == 0.0 &&
                      !e.clear_colour_steel,
                  label + " no side effects");
            check(e.id == PowerUpId::Tnt && e.tier == tier, label + " id and tier");
        }
    }
    // Exact shapes at the centre of a 9x9 board.
    const Board big(9, 9, 7, 3);
    const auto l3 = power_up_effect(PowerUpId::Tnt, 3, big, {4, 4});
    for (const Cell corner : {Cell{2, 2}, Cell{6, 2}, Cell{2, 6}, Cell{6, 6}})
        check(std::find(l3.cells_to_clear.begin(), l3.cells_to_clear.end(), corner) == l3.cells_to_clear.end(),
              "tnt L3 skips corner " + str(corner));
    const auto l2 = power_up_effect(PowerUpId::Tnt, 2, big, {4, 4});
    check(std::find(l2.cells_to_clear.begin(), l2.cells_to_clear.end(), Cell{4, 2}) != l2.cells_to_clear.end() &&
              std::find(l2.cells_to_clear.begin(), l2.cells_to_clear.end(), Cell{5, 3}) != l2.cells_to_clear.end() &&
              std::find(l2.cells_to_clear.begin(), l2.cells_to_clear.end(), Cell{5, 2}) == l2.cells_to_clear.end(),
          "tnt L2 is a diamond");

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
            const std::string label = std::string("lightning L") + std::to_string(tier) + " at " + c.where;
            check(static_cast<int>(e.cells_to_clear.size()) == c.sizes[static_cast<std::size_t>(tier - 1)], label + " size");
            check(e.cells_to_clear == expected(c.at, tier), label + " cells");
            check(row_major_unique_occupied(e.cells_to_clear, b), label + " sorted");
        }
    }
}

void test_color_clear() {
    const Board b = plain_board();
    for (int kind = 0; kind < 6; ++kind) {
        const auto same = cells_where(b, [&](int x, int y) { return b.at(x, y) == kind; });
        const auto with_adjacent = cells_where(b, [&](int x, int y) {
            for (const Cell s : same)
                if (std::abs(s.x - x) + std::abs(s.y - y) <= 1) return true;
            return false;
        });
        for (int tier = 1; tier <= 3; ++tier) {
            // Trigger at a cell of another colour: the recorded colour wins.
            const Cell at{0, 0};
            const auto e = power_up_effect(PowerUpId::ColorClear, tier, b, at, kind);
            const std::string label =
                "color_clear L" + std::to_string(tier) + " colour " + std::to_string(kind);
            check(e.cells_to_clear == (tier == 3 ? with_adjacent : same), label + " cells");
            check(row_major_unique_occupied(e.cells_to_clear, b), label + " sorted");
            check(e.clear_colour_steel == (tier >= 2), label + " steel flag");
        }
    }
    // Without a recorded colour, the piece at the cell is used.
    const auto e = power_up_effect(PowerUpId::ColorClear, 1, b, {7, 7});
    const int kind = b.at(7, 7);
    check(e.cells_to_clear == cells_where(b, [&](int x, int y) { return b.at(x, y) == kind; }),
          "color_clear falls back to the piece at the cell");
    // Corners and edges: L3 adjacency stays on the board.
    Board one = plain_board();  // kinds 0..5 only, so 6 is a fresh colour
    one.set(0, 0, 6);
    one.set(7, 7, 6);
    one.set(7, 3, 6);
    const auto l3 = power_up_effect(PowerUpId::ColorClear, 3, one, {0, 0}, 6);
    check(l3.cells_to_clear == std::vector<Cell>{{0, 0}, {1, 0}, {0, 1}, {7, 2}, {6, 3}, {7, 3}, {7, 4}, {7, 6}, {6, 7}, {7, 7}},
          "color_clear L3 adjacency is clipped at corners and edges");
    const auto none = power_up_effect(PowerUpId::ColorClear, 3, b, {0, 0}, 6);
    check(none.cells_to_clear.empty(), "a colour not on the board clears nothing");
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
            check(c.time_delta_seconds == clock[i] && c.cells_to_clear.empty() && c.score_multiplier == 1 &&
                      c.hint_seconds == 0.0,
                  "clock L" + std::to_string(tier));
            const auto m = power_up_effect(PowerUpId::Multiplier, tier, b, at);
            check(m.score_multiplier == mult[i] && m.cells_to_clear.empty() && m.time_delta_seconds == 0.0,
                  "multiplier L" + std::to_string(tier));
            const auto h = power_up_effect(PowerUpId::Hint, tier, b, at);
            check(h.hint_seconds == hint[i] && h.cells_to_clear.empty() && h.score_multiplier == 1,
                  "hint L" + std::to_string(tier));
        }
    }
    // Tiers clamp; out-of-bounds cells give no cells but keep values.
    check(power_up_effect(PowerUpId::Clock, 0, b, {0, 0}).time_delta_seconds == 15.0, "tier 0 clamps to 1");
    check(power_up_effect(PowerUpId::Clock, 9, b, {0, 0}).time_delta_seconds == 25.0, "tier 9 clamps to 3");
    check(power_up_effect(PowerUpId::Tnt, 9, b, {3, 4}).cells_to_clear.size() == 21, "tnt tier 9 clamps to 3");
    const auto off = power_up_effect(PowerUpId::Tnt, 1, b, {-1, 3});
    check(off.cells_to_clear.empty(), "off-board cell clears nothing");
    check(power_up_effect(PowerUpId::Multiplier, 2, b, {99, 99}).score_multiplier == 7, "off-board keeps the value");

    const TriggeredPowerUp t{{2, 2}, {PowerUpId::ColorClear, 2, 3}, 0};
    check(power_up_effect(t, b) == power_up_effect(PowerUpId::ColorClear, 2, b, {2, 2}, 3),
          "TriggeredPowerUp overload passes id, tier, cell and colour");
}

// --- random play ---------------------------------------------------------------

std::optional<MoveResult> random_swap(Board& b, Rng& rng) {
    std::vector<std::array<Cell, 2>> legal;
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x) {
            if (x + 1 < b.width() && b.is_legal_swap({x, y}, {x + 1, y})) legal.push_back({Cell{x, y}, Cell{x + 1, y}});
            if (y + 1 < b.height() && b.is_legal_swap({x, y}, {x, y + 1})) legal.push_back({Cell{x, y}, Cell{x, y + 1}});
        }
    if (legal.empty()) return std::nullopt;
    const auto& s = legal[static_cast<std::size_t>(rng.below(static_cast<int>(legal.size())))];
    return b.try_swap(s[0], s[1]);
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

// Plays the documented game-layer loop: trigger, remap, resolve the chain
// with clear_cells, then place. Checks the layer after every remap.
SweepStats sweep(int w, int h, int kinds, std::uint64_t seed, int moves) {
    SweepStats st;
    Board b(w, h, kinds, seed);
    PowerUpLayer layer(b);
    Rng rng(seed * 31 + 7);
    const PowerUpTiers tiers{3, 2, 1, 2, 3, 1};
    for (int m = 0; m < moves; ++m) {
        auto r = random_swap(b, rng);
        if (!r) break;
        ++st.moves;
        std::vector<TriggeredPowerUp> queue = layer.triggered_by(*r);
        layer.remap(*r);
        st.shuffles += r->shuffled ? 1 : 0;
        st.redeals += r->redealt ? 1 : 0;
        if (r->redealt && layer.count() != 0) st.ok = false;
        if (!layer_matches_board(layer, b)) st.ok = false;
        for (std::size_t q = 0; q < queue.size() && q < 200; ++q) {
            ++st.fired;
            const PowerUpEffect e = power_up_effect(queue[q], b);
            if (!row_major_unique_occupied(e.cells_to_clear, b)) st.ok = false;
            if (e.cells_to_clear.empty()) continue;
            auto er = b.clear_cells(e.cells_to_clear);
            if (!er) {
                st.ok = false;
                continue;
            }
            const auto more = layer.triggered_by(*er);
            st.chained += static_cast<int>(more.size());
            queue.insert(queue.end(), more.begin(), more.end());
            layer.remap(*er);
            st.shuffles += er->shuffled ? 1 : 0;
            st.redeals += er->redealt ? 1 : 0;
            if (!layer_matches_board(layer, b)) st.ok = false;
        }
        const auto placed = layer.place_from_move(*r, b, rng, kAllUnlocked, tiers);
        st.placed += static_cast<int>(placed.size());
        st.log.insert(st.log.end(), placed.begin(), placed.end());
        if (!layer_matches_board(layer, b)) st.ok = false;
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
    check(total.fired > 100, "sweep fired power-ups (" + std::to_string(total.fired) + ")");
    check(total.chained > 0, "sweep chained triggers (" + std::to_string(total.chained) + ")");
    check(total.shuffles > 0, "sweep went through shuffles (" + std::to_string(total.shuffles) + ")");
    std::printf("sweep: %d moves, %d placed, %d fired, %d chained, %d shuffles, %d redeals\n", total.moves,
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
    test_placement_counts();
    test_placement_only_on_plain_pieces();
    test_placement_draw_order_and_determinism();
    test_remap_falls_and_swap();
    test_remap_shuffle();
    test_trigger_on_real_match();
    test_tnt();
    test_lightning();
    test_color_clear();
    test_value_effects();
    test_random_play();
    test_determinism();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
