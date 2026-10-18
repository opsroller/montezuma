// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Cell-level cascade events (MoveResult::cascades, swap, shuffle_map) and
// Board::clear_cells. The main property: replaying the events on a copy of
// the cells from before the move reproduces the board exactly, so a view can
// animate a move without knowing the rules. No framework; run via ctest.
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "montezuma/board.hpp"
#include "montezuma/level_state.hpp"
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

Board board_from(const std::vector<std::string>& rows, int kinds = 7) {
    Board b(static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), kinds, 1);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) b.set(x, y, rows[y][x] - '0');
    return b;
}

// A plain grid the test owns, independent of Board, used to replay events.
struct Grid {
    int width;
    int height;
    std::vector<int> cells;

    int& at(Cell c) { return cells[static_cast<std::size_t>(c.y * width + c.x)]; }
    int at(Cell c) const { return cells[static_cast<std::size_t>(c.y * width + c.x)]; }
    bool in_bounds(Cell c) const { return c.x >= 0 && c.y >= 0 && c.x < width && c.y < height; }
};

int row_major(const Grid& g, Cell c) { return c.y * g.width + c.x; }

// Reference run finder written separately from Board::find_runs.
std::vector<Run> reference_runs(const Grid& g) {
    std::vector<Run> runs;
    auto scan = [&](int lines, int length, bool horizontal) {
        for (int line = 0; line < lines; ++line) {
            int start = 0;
            for (int i = 1; i <= length; ++i) {
                auto cell = [&](int k) { return horizontal ? Cell{k, line} : Cell{line, k}; };
                int kind = g.at(cell(start));
                if (i < length && g.at(cell(i)) == kind) continue;
                if (kind != Board::kEmpty && i - start >= 3) {
                    Run r{{}, kind};
                    for (int k = start; k < i; ++k) r.cells.push_back(cell(k));
                    runs.push_back(r);
                }
                start = i;
            }
        }
    };
    scan(g.height, g.width, true);
    scan(g.width, g.height, false);
    return runs;
}

// Replays `r` on `g` (the cells before the move), checking every invariant
// the view relies on along the way. `effect` is true for clear_cells.
// Returns false (after reporting) on the first broken invariant.
bool replay(Grid& g, const MoveResult& r, bool effect, const std::string& label) {
    if (!check(r.steps.size() == r.cascades.size(), label + ": steps and cascades line up")) return false;
    if (!check(!r.cascades.empty(), label + ": at least one step")) return false;
    if (effect) {
        if (!check(!r.swap.has_value() && r.matched_kind == -1, label + ": effect has no swap or matched kind"))
            return false;
    } else {
        if (!check(r.swap.has_value(), label + ": swap recorded")) return false;
        std::swap(g.at((*r.swap)[0]), g.at((*r.swap)[1]));
    }
    for (std::size_t s = 0; s < r.cascades.size(); ++s) {
        const CascadeStep& step = r.cascades[s];
        const std::string at = label + " step " + std::to_string(s);
        const bool effect_step = effect && s == 0;

        // Runs: exactly the runs present on the replayed grid.
        if (effect_step) {
            if (!check(step.runs.empty(), at + ": effect step has no runs")) return false;
        } else {
            if (!check(step.runs == reference_runs(g), at + ": runs match the replayed board")) return false;
        }

        // Cleared: row-major, unique, in bounds, non-empty, and (for matches)
        // exactly the union of the runs.
        std::set<int> run_cells;
        for (const Run& run : step.runs)
            for (Cell c : run.cells) run_cells.insert(row_major(g, c));
        std::set<int> cleared;
        int last = -1;
        std::map<int, int> counts;
        for (Cell c : step.cleared) {
            if (!check(g.in_bounds(c) && g.at(c) != Board::kEmpty, at + ": cleared cell holds a piece")) return false;
            int idx = row_major(g, c);
            if (!check(idx > last, at + ": cleared is row-major and unique")) return false;
            last = idx;
            cleared.insert(idx);
            ++counts[g.at(c)];
        }
        if (!check(!step.cleared.empty(), at + ": something cleared")) return false;
        if (!effect_step && !check(cleared == run_cells, at + ": cleared cells are exactly the run cells"))
            return false;
        if (!check(counts == r.steps[s], at + ": steps counts equal cleared cells by kind")) return false;
        for (Cell c : step.cleared) g.at(c) = Board::kEmpty;

        // Falls: straight down, ordered, no overlap, kind matches the piece.
        std::set<int> froms;
        std::set<int> tos;
        for (std::size_t i = 0; i < step.falls.size(); ++i) {
            const Fall& f = step.falls[i];
            if (!check(g.in_bounds(f.from) && g.in_bounds(f.to) && f.from.x == f.to.x && f.to.y > f.from.y,
                       at + ": fall moves straight down"))
                return false;
            if (!check(g.at(f.from) == f.kind && f.kind != Board::kEmpty, at + ": fall kind matches the piece"))
                return false;
            if (!check(froms.insert(row_major(g, f.from)).second && tos.insert(row_major(g, f.to)).second,
                       at + ": falls never share a source or target"))
                return false;
            if (i > 0) {
                const Fall& p = step.falls[i - 1];
                if (!check(p.from.x < f.from.x || (p.from.x == f.from.x && p.from.y > f.from.y),
                           at + ": falls ordered by column, bottom to top"))
                    return false;
            }
        }
        std::vector<int> moving(step.falls.size());
        for (std::size_t i = 0; i < step.falls.size(); ++i) {
            moving[i] = g.at(step.falls[i].from);
            g.at(step.falls[i].from) = Board::kEmpty;
        }
        for (std::size_t i = 0; i < step.falls.size(); ++i) {
            if (!check(g.at(step.falls[i].to) == Board::kEmpty, at + ": fall lands on an empty cell")) return false;
            g.at(step.falls[i].to) = moving[i];
        }
        // After falling, a column is a block of gaps on top of solid pieces.
        std::map<int, int> gaps;
        for (int x = 0; x < g.width; ++x) {
            bool seen_piece = false;
            for (int y = 0; y < g.height; ++y) {
                bool empty = g.at({x, y}) == Board::kEmpty;
                if (!check(!(empty && seen_piece), at + ": no gap left under a piece")) return false;
                seen_piece = seen_piece || !empty;
                if (empty) ++gaps[x];
            }
        }

        // Spawns: fill exactly the gaps, with drop counted from the top edge.
        for (std::size_t i = 0; i < step.spawns.size(); ++i) {
            const Spawn& sp = step.spawns[i];
            if (!check(g.in_bounds(sp.to) && g.at(sp.to) == Board::kEmpty, at + ": spawn lands on an empty cell"))
                return false;
            if (!check(sp.drop == gaps[sp.to.x] - sp.to.y && sp.drop >= 1, at + ": spawn drop is its start row above the board"))
                return false;
            if (i > 0) {
                const Spawn& p = step.spawns[i - 1];
                if (!check(p.to.x < sp.to.x || (p.to.x == sp.to.x && p.to.y > sp.to.y),
                           at + ": spawns ordered by column, bottom to top"))
                    return false;
            }
            g.at(sp.to) = sp.kind;
        }
        for (int c : g.cells)
            if (!check(c != Board::kEmpty, at + ": board full after spawns")) return false;
    }
    if (!check(reference_runs(g).empty(), label + ": no runs left after the last step")) return false;

    if (r.shuffled && !r.redealt) {
        std::vector<int> before = g.cells;
        std::set<int> tos;
        int last = -1;
        for (const auto& [from, to] : r.shuffle_map) {
            if (!check(g.in_bounds(from) && g.in_bounds(to) && !(from == to), label + ": shuffle pair moves a piece"))
                return false;
            int idx = row_major(g, to);
            if (!check(idx > last, label + ": shuffle_map ordered by target")) return false;
            last = idx;
            tos.insert(idx);
            g.at(to) = before[static_cast<std::size_t>(row_major(g, from))];
        }
        std::set<int> froms;
        for (const auto& pair : r.shuffle_map) froms.insert(row_major(g, pair.first));
        check(froms == tos, label + ": shuffle_map is a permutation");
    } else {
        if (!check(r.shuffle_map.empty(), label + ": no shuffle_map without a reorder")) return false;
        if (!check(!r.redealt || r.shuffled, label + ": redealt only when shuffled")) return false;
    }
    return true;
}

// Replays and compares with the board's actual final cells.
bool replay_matches(const std::vector<int>& before, const Board& b, const MoveResult& r, bool effect,
                    const std::string& label) {
    Grid g{b.width(), b.height(), before};
    if (!replay(g, r, effect, label)) return false;
    if (r.redealt) return check(true, label + ": redeal reads the board");
    return check(g.cells == b.cells(), label + ": replay reproduces the final board");
}

void test_swap_events_on_a_known_board() {
    Board b = board_from({"0010", "1234", "2345", "3456"});
    auto before = b.cells();
    auto r = b.try_swap({2, 0}, {3, 0});
    if (!check(r.has_value(), "known swap is legal")) return;
    check(r->swap == std::array<Cell, 2>{Cell{2, 0}, Cell{3, 0}}, "swap cells recorded as passed");
    const CascadeStep& s0 = r->cascades.at(0);
    check(s0.runs == std::vector<Run>{{{{0, 0}, {1, 0}, {2, 0}}, 0}}, "step 0 run is the top-row 000");
    check(s0.runs[0].horizontal(), "top-row run is horizontal");
    check(s0.cleared == std::vector<Cell>{{0, 0}, {1, 0}, {2, 0}}, "step 0 clears the three cells");
    check(s0.falls.empty(), "nothing above the top row to fall");
    check(s0.spawns.size() == 3, "three new pieces");
    for (const Spawn& sp : s0.spawns) check(sp.to.y == 0 && sp.drop == 1, "top-row spawns start one row above");
    replay_matches(before, b, *r, false, "known swap");
}

void test_vertical_clear_makes_pieces_fall() {
    // Swapping (1,3)<->(0,3) puts a 0 under the 0s at (0,1),(0,2): a vertical
    // run in column 0 of rows 1..3. The piece at (0,0) falls three rows.
    Board b = board_from({"5123", "0234", "0345", "1056", "2361"});
    auto before = b.cells();
    auto r = b.try_swap({0, 3}, {1, 3});
    if (!check(r.has_value(), "vertical swap is legal")) return;
    const CascadeStep& s0 = r->cascades.at(0);
    check(s0.runs == std::vector<Run>{{{{0, 1}, {0, 2}, {0, 3}}, 0}}, "vertical run found");
    check(!s0.runs[0].horizontal(), "vertical run is not horizontal");
    check(s0.falls == std::vector<Fall>{{{0, 0}, {0, 3}, 5}}, "top piece falls three rows");
    check(s0.spawns.size() == 3 && s0.spawns[0].to == Cell{0, 2} && s0.spawns[0].drop == 1 &&
              s0.spawns[2].to == Cell{0, 0} && s0.spawns[2].drop == 3,
          "spawns fill bottom to top, the highest starting furthest up");
    replay_matches(before, b, *r, false, "vertical swap");
}

void test_t_shape_and_long_runs_are_reported() {
    // Swapping (2,2)<->(3,2) moves a 0 into the centre: row 2 gets 000 on the
    // left and column 2 becomes 00000, a T made of a 3-run and a 5-run.
    Board b = board_from({"12034", "34056", "00102", "56013", "21045"});
    check(b.find_runs().empty(), "fixture starts without runs");
    auto before = b.cells();
    auto r = b.try_swap({2, 2}, {3, 2});
    if (!check(r.has_value(), "T swap is legal")) return;
    const CascadeStep& s0 = r->cascades.at(0);
    check(s0.runs.size() == 2, "two crossing runs");
    check(s0.runs.at(0) == Run{{{0, 2}, {1, 2}, {2, 2}}, 0}, "horizontal runs come first");
    check(s0.runs.at(1) == Run{{{2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 4}}, 0}, "five-run down column 2");
    check(s0.cleared.size() == 7, "shared cell cleared once");
    check(r->steps.at(0).at(0) == 7, "seven pieces of kind 0 counted");
    replay_matches(before, b, *r, false, "T swap");
}

void test_clear_cells_on_known_boards() {
    Board b = board_from({"0123", "1230", "2301", "3012"});
    auto before = b.cells();
    check(!b.clear_cells({}), "empty input is rejected");
    check(!b.clear_cells({{-1, 0}, {4, 0}, {0, 4}}), "all out-of-bounds input is rejected");
    check(b.cells() == before, "rejected clears leave the board alone");

    auto r = b.clear_cells({{1, 2}, {9, 9}, {1, 2}, {1, 1}});
    if (!check(r.has_value(), "valid cells are cleared")) return;
    check(!r->swap.has_value() && r->matched_kind == -1, "effect result has no swap and no matched kind");
    const CascadeStep& s0 = r->cascades.at(0);
    check(s0.cleared == std::vector<Cell>{{1, 1}, {1, 2}}, "duplicates and bad cells dropped, row-major order");
    check(s0.runs.empty(), "effect step has no runs");
    check(s0.falls == std::vector<Fall>{{{1, 0}, {1, 2}, 1}}, "piece above the hole falls two rows");
    check(s0.spawns.size() == 2 && s0.spawns[0].drop == 1 && s0.spawns[1].drop == 2, "two spawns refill column 1");
    check(r->steps.at(0) == std::map<int, int>{{2, 1}, {3, 1}}, "counts by kind of the cleared pieces");
    replay_matches(before, b, *r, true, "clear_cells known");

    // A single cell on the bottom row: the whole column above falls one row.
    Board c = board_from({"1456", "0234", "5023", "0345"});
    before = c.cells();
    auto rc = c.clear_cells({{0, 3}});
    if (!check(rc.has_value(), "single-cell clear works")) return;
    check(rc->cascades.at(0).falls ==
              std::vector<Fall>{{{0, 2}, {0, 3}, 5}, {{0, 1}, {0, 2}, 0}, {{0, 0}, {0, 1}, 1}},
          "column falls one row, listed bottom to top");
    replay_matches(before, c, *rc, true, "clear_cells single");
}

void test_clear_cells_triggers_cascades() {
    // Clearing the blocker at (1,1) drops the 0 at (1,0) into row 1,
    // which completes 000 across row 1.
    Board b = board_from({"1023", "0405", "2345", "3456"});
    auto before = b.cells();
    auto r = b.clear_cells({{1, 1}});
    if (!check(r.has_value(), "blocker clear works")) return;
    check(r->cascades.size() >= 2, "clear brings a match together and cascades");
    if (r->cascades.size() >= 2)
        check(r->cascades[1].runs.at(0) == Run{{{0, 1}, {1, 1}, {2, 1}}, 0}, "cascade run is row 1");
    replay_matches(before, b, *r, true, "clear_cells cascade");
}

void test_seeded_determinism() {
    for (std::uint64_t seed : {3ull, 42ull, 1234567ull}) {
        Board a(8, 8, 7, seed);
        Board b(8, 8, 7, seed);
        for (int move = 0; move < 50; ++move) {
            std::optional<MoveResult> ra;
            std::optional<MoveResult> rb;
            for (int y = 0; y < 8 && !ra; ++y)
                for (int x = 0; x + 1 < 8 && !ra; ++x) {
                    ra = a.try_swap({x, y}, {x + 1, y});
                    rb = b.try_swap({x, y}, {x + 1, y});
                }
            if (!check(ra.has_value() == rb.has_value(), "same legality for the same seed")) return;
            if (!ra) break;
            check(*ra == *rb, "same seed and swap gives identical events, seed " + std::to_string(seed));
            auto ca = a.clear_cells({{move % 8, (move * 3) % 8}});
            auto cb = b.clear_cells({{move % 8, (move * 3) % 8}});
            check(ca.has_value() && cb.has_value() && *ca == *cb, "same seed and clear gives identical events");
        }
        check(a.cells() == b.cells(), "same final board for the same seed");
    }
}

void test_shuffle_reports_a_permutation() {
    // 3x3 with 3 kinds runs out of moves often, so shuffles happen.
    int shuffles = 0;
    int redeals = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        Board b(3, 3, 3, seed);
        Rng pick(seed * 7 + 1);
        for (int move = 0; move < 30; ++move) {
            auto before = b.cells();
            int x = pick.below(3);
            int y = pick.below(3);
            auto r = b.clear_cells({{x, y}});
            if (!check(r.has_value(), "clear on a full board works")) return;
            if (!replay_matches(before, b, *r, true, "3x3 seed " + std::to_string(seed))) return;
            shuffles += r->shuffled && !r->redealt;
            redeals += r->redealt;
        }
    }
    check(shuffles > 0, "small boards exercise shuffle_map (" + std::to_string(shuffles) + " shuffles, " +
                            std::to_string(redeals) + " redeals)");
}

void test_unshufflable_board_reports_a_redeal() {
    // At most two pieces of any kind: no rearrangement of these can ever make
    // a legal move, so when a clear leaves the board like this the shuffle
    // must fall back to a fresh deal and say so.
    int redeals = 0;
    for (int x = 0; x < 3; ++x)
        for (int y = 0; y < 3; ++y) {
            Board b = board_from({"012", "345", "601"});
            auto before = b.cells();
            auto r = b.clear_cells({{x, y}});
            if (!check(r.has_value(), "clear works")) return;
            replay_matches(before, b, *r, true, "redeal fixture");
            check(!r->stuck, "never stuck");
            if (!r->redealt) continue;
            ++redeals;
            check(r->shuffled && r->shuffle_map.empty(), "a redeal is a shuffle with no shuffle_map");
            check(b.find_matches().empty() && b.has_legal_move(), "board playable after the redeal");
        }
    check(redeals > 0, "fixture produced a redeal (" + std::to_string(redeals) + ")");
}

void test_random_play_replays_exactly() {
    int moves = 0;
    int effects = 0;
    int cascades = 0;
    for (std::uint64_t seed = 0; seed < 40; ++seed) {
        const int w = 5 + static_cast<int>(seed % 4);
        const int h = 5 + static_cast<int>((seed / 4) % 4);
        const int kinds = 4 + static_cast<int>(seed % 4);
        Board b(w, h, kinds, seed);
        Rng pick(seed ^ 0xC0FFEEull);
        for (int i = 0; i < 300; ++i) {
            const std::string label = "seed " + std::to_string(seed) + " move " + std::to_string(i);
            auto before = b.cells();
            std::optional<MoveResult> r;
            bool effect = pick.below(5) == 0;
            if (effect) {
                std::vector<Cell> cells;
                int n = 1 + pick.below(6);
                for (int k = 0; k < n; ++k) cells.push_back({pick.below(w + 2) - 1, pick.below(h + 2) - 1});
                r = b.clear_cells(cells);
                if (!r) {
                    check(b.cells() == before, label + ": rejected clear leaves the board");
                    continue;
                }
            } else {
                std::vector<std::array<Cell, 2>> legal;
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        if (x + 1 < w && b.is_legal_swap({x, y}, {x + 1, y})) legal.push_back({Cell{x, y}, Cell{x + 1, y}});
                        if (y + 1 < h && b.is_legal_swap({x, y}, {x, y + 1})) legal.push_back({Cell{x, y}, Cell{x, y + 1}});
                    }
                if (!check(!legal.empty(), label + ": a legal move exists")) return;
                auto swap = legal[static_cast<std::size_t>(pick.below(static_cast<int>(legal.size())))];
                r = b.try_swap(swap[0], swap[1]);
                if (!check(r.has_value(), label + ": listed legal swap succeeds")) return;
            }
            if (!replay_matches(before, b, *r, effect, label)) return;
            check(!r->stuck, label + ": never stuck");
            moves += !effect;
            effects += effect;
            cascades += static_cast<int>(r->cascades.size()) - 1;
        }
    }
    check(moves > 5000 && effects > 1000 && cascades > 500,
          "random play covered " + std::to_string(moves) + " swaps, " + std::to_string(effects) + " effects, " +
              std::to_string(cascades) + " cascades");
}

}  // namespace

void test_effect_results_keep_the_streak() {
    LevelState state({{0, 100}}, 60.0);
    MoveResult swap;
    swap.matched_kind = 2;
    swap.steps = {{{2, 3}}};
    state.apply_move(swap);
    state.apply_move(swap);
    MoveResult effect;
    effect.steps = {{{0, 4}}};
    state.apply_move(effect);
    check(state.streak_kind() == 2 && state.streak() == 2, "effect result keeps the streak");
    check(state.quotas().at(0) == 96, "effect result still counts toward quotas");
}

int main() {
    test_swap_events_on_a_known_board();
    test_vertical_clear_makes_pieces_fall();
    test_t_shape_and_long_runs_are_reported();
    test_clear_cells_on_known_boards();
    test_clear_cells_triggers_cascades();
    test_seeded_determinism();
    test_shuffle_reports_a_permutation();
    test_unshufflable_board_reports_a_redeal();
    test_random_play_replays_exactly();
    test_effect_results_keep_the_streak();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
