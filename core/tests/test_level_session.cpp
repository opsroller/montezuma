// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// LevelSession tests (WP-22): one level by the full classic rules
// (rules-spec.md, rules-power-ups.md, rules-totems.md, progression.md).
// No framework dependency; run via ctest.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "montezuma/level_session.hpp"

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

constexpr int kRed = 0;
constexpr int kOrange = 1;
constexpr int kYellow = 2;
constexpr int kGreen = 3;
constexpr int kBlue = 4;
constexpr int kPurple = 5;
constexpr int kWhite = 6;

// A level that does not end by accident: goal 50 gems, nothing unlocked.
LevelData base_level() {
    LevelData l;
    l.id = "e1-l03";
    l.episode = 1;
    l.index = 3;
    l.gem_goal = GemGoal{50, 75};
    l.seed = 7;
    return l;
}

// Background with no run and no legal move: kind (x + 2y) mod 7.
int bg(int x, int y) { return (x + 2 * y) % 7; }

const std::vector<std::string> kBlank(8, "........");

// One blue run in row 7 at x 0..2: swap (2,6) with (2,7).
const std::vector<std::string> kSimple{"........", "........", "........", "........",
                                       "........", "........", "56412345", "44234560"};
const Swap kSimpleSwap{Cell{2, 6}, Cell{2, 7}};
// Blue run at row 7 x 0..2, then the fall makes an orange run at x 1..3.
const std::vector<std::string> kCascade{"........", "........", "........", "........",
                                        "........", "........", "51102345", "44144560"};
const Swap kCascadeSwap{Cell{2, 7}, Cell{3, 7}};
// Blue run of 4 / of 5 in row 7: swap (2,6) with (2,7).
const std::vector<std::string> kRun4{"........", "........", "........", "........",
                                     "........", "........", "56412345", "44241560"};
const std::vector<std::string> kRun5{"........", "........", "........", "........",
                                     "........", "........", "56412345", "44244560"};
// An L: blue row 7 x 0..2 and column 2 y 5..7; swap (2,7) with (3,7).
const std::vector<std::string> kEll{"........", "........", "........", "........",
                                    "........", "34460123", "56412345", "44244560"};
const Swap kEllSwap{Cell{2, 7}, Cell{3, 7}};

// Paints `rows` ('.' = background) on a fresh session and removes every gem,
// power-up, steel piece and chain, so a test starts from a known board.
void paint(LevelSession& s, const std::vector<std::string>& rows) {
    for (int y = 0; y < s.board().height(); ++y)
        for (int x = 0; x < s.board().width(); ++x) {
            const Cell c{x, y};
            s.set_gem(c, false);
            s.set_steel(c, false);
            s.set_chain(c, false);
            s.set_power_up(c, std::nullopt);
            const char ch = (y < static_cast<int>(rows.size()) && x < static_cast<int>(rows[y].size())) ? rows[y][x] : '.';
            s.set_piece(c, ch == '.' ? bg(x, y) : ch - '0');
        }
}

LevelSession scene(const std::vector<std::string>& rows, const LevelData& level = base_level(),
                   const Upgrades& up = {}, std::uint64_t seed = 1, Difficulty d = Difficulty::Normal) {
    LevelSession s(level, d, up, seed);
    paint(s, rows);
    check(s.runs().empty(), "scene is still: " + rows[6] + "/" + rows[7]);
    return s;
}


bool has_cell(const std::vector<Cell>& cells, Cell c) { return std::find(cells.begin(), cells.end(), c) != cells.end(); }

// Everything a report should let the view rebuild.
struct Snapshot {
    std::vector<int> kinds;
    std::vector<bool> gems, steel, chains;
    PowerUpLayer power_ups;
    double time_left = 0;
    int score = 0, gems_collected = 0, frenzy_bar = 0, moves = 0;
    bool operator==(const Snapshot&) const = default;
};

Snapshot snap(const LevelSession& s) {
    return {s.board().cells(), s.gem_marks(), s.steel_marks(), s.chain_marks(), s.power_ups(), s.time_left(),
            s.score(),         s.gems_collected(), s.frenzy_bar(), s.moves()};
}

// Replays a report on a copy of the previous state's pieces and marks.
struct Model {
    int w = 8;
    std::vector<int> kinds;
    std::vector<bool> gems, steel, chains;
    PowerUpLayer pu;

    std::size_t i(Cell c) const { return static_cast<std::size_t>(c.y * w + c.x); }

    void step(const StepReport& st) {
        for (Cell c : st.chains_broken) chains[i(c)] = false;  // OBS-05: the piece stays
        for (Cell c : st.cleared) {
            kinds[i(c)] = Board::kEmpty;
            gems[i(c)] = steel[i(c)] = chains[i(c)] = false;
        }
        CascadeStep cs;
        cs.cleared = st.cleared;
        cs.falls = st.falls;
        pu.apply_step(cs);
        struct M {
            Cell to;
            int k;
            bool g, s, ch;
        };
        std::vector<M> moving;
        for (const Fall& f : st.falls) {
            moving.push_back({f.to, kinds[i(f.from)], gems[i(f.from)], steel[i(f.from)], chains[i(f.from)]});
            kinds[i(f.from)] = Board::kEmpty;
            gems[i(f.from)] = steel[i(f.from)] = chains[i(f.from)] = false;
        }
        for (const M& m : moving) {
            kinds[i(m.to)] = m.k;
            gems[i(m.to)] = m.g;
            steel[i(m.to)] = m.s;
            chains[i(m.to)] = m.ch;
        }
        for (const Spawn& sp : st.spawns) kinds[i(sp.to)] = sp.kind;
        for (Cell c : st.steel_spawned) steel[i(c)] = true;
        for (Cell c : st.gems_placed) gems[i(c)] = true;
        for (const PowerUpPlacement& p : st.power_ups_placed) pu.set(p.cell, p.power_up);
    }

    // False when the report cannot be replayed (a redeal).
    bool apply(const MoveReport& r) {
        const Cell a = r.swap[0], b = r.swap[1];
        std::swap(kinds[i(a)], kinds[i(b)]);
        bool g = gems[i(a)];
        gems[i(a)] = gems[i(b)];
        gems[i(b)] = g;
        pu.apply_swap(a, b);
        for (const StepReport& st : r.steps) step(st);
        for (const TotemFiring& f : r.totems_fired) {
            for (Cell c : f.effect.cells_to_repaint) kinds[i(c)] = f.effect.repaint_kind;
            for (Cell c : f.gems_harvested) gems[i(c)] = false;
            for (Cell c : f.gems_placed) gems[i(c)] = true;
            for (Cell c : f.chains_removed) chains[i(c)] = false;
            for (const PowerUpPlacement& p : f.power_ups_placed) pu.set(p.cell, p.power_up);
            for (const StepReport& st : f.steps) step(st);
        }
        if (r.auto_shuffle && r.auto_shuffle->shuffled) {
            if (r.auto_shuffle->redealt) return false;
            const auto k0 = kinds;
            const auto g0 = gems;
            for (const auto& [from, to] : r.auto_shuffle->shuffle_map) {
                kinds[i(to)] = k0[i(from)];
                gems[i(to)] = g0[i(from)];
            }
            MoveResult m;
            m.shuffled = true;
            m.shuffle_map = r.auto_shuffle->shuffle_map;
            Board dummy(8, 8, 7, 0);
            Rng rng(0);
            pu.apply_shuffle(m, dummy, rng);
        }
        return true;
    }
};

Model model_of(const LevelSession& s) {
    return {s.board().width(), s.board().cells(), s.gem_marks(), s.steel_marks(), s.chain_marks(), s.power_ups()};
}

bool model_matches(const Model& m, const LevelSession& s) {
    return m.kinds == s.board().cells() && m.gems == s.gem_marks() && m.steel == s.steel_marks() &&
           m.chains == s.chain_marks() && m.pu.cells == s.power_ups().cells;
}

Upgrades with_totem(TotemId id, int tier) {
    Upgrades u;
    u.totem_tiers[static_cast<std::size_t>(id)] = tier;
    return u;
}

LevelData level_with_totems(std::vector<std::string> totems, std::vector<std::string> powerups = {}) {
    LevelData l = base_level();
    l.totems_unlocked = std::move(totems);
    l.powerups_unlocked = std::move(powerups);
    return l;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void test_construction() {
    LevelData l = base_level();
    l.board = {7, 6, 5};
    LevelSession s(l, Difficulty::Normal, {}, 3);
    check(s.board().width() == 7 && s.board().height() == 6 && s.board().kinds() == 5, "BOARD-02 size and kinds");
    bool kinds_ok = std::all_of(s.board().cells().begin(), s.board().cells().end(), [](int k) { return k >= 0 && k < 5; });
    check(kinds_ok, "BOARD-02 pieces from the level's kinds");

    // Determinism.
    LevelSession a(base_level(), Difficulty::Normal, {}, 42);
    LevelSession b(base_level(), Difficulty::Normal, {}, 42);
    check(snap(a) == snap(b), "same seed, same board and marks");
    for (int i = 0; i < 30; ++i) {
        a.tick(1.5);
        b.tick(1.5);
        auto ma = a.legal_moves();
        auto mb = b.legal_moves();
        if (ma.empty()) break;
        auto ra = a.swap(ma[static_cast<std::size_t>(i) % ma.size()][0], ma[static_cast<std::size_t>(i) % ma.size()][1]);
        auto rb = b.swap(mb[static_cast<std::size_t>(i) % mb.size()][0], mb[static_cast<std::size_t>(i) % mb.size()][1]);
        check(ra.has_value() && rb.has_value(), "determinism: moves legal");
        check(ra->points == rb->points && ra->steps.size() == rb->steps.size(), "determinism: same report");
    }
    check(snap(a) == snap(b), "determinism: same state after 30 moves");

    bool differs = false;
    for (std::uint64_t seed = 1; seed < 6 && !differs; ++seed)
        differs = LevelSession(base_level(), Difficulty::Normal, {}, seed).board().cells() != a.board().cells();
    check(differs, "different seed, different opening board");

    // BOARD-03, GEM-02, BOARD-04
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        LevelData lc = base_level();
        lc.chained_cells = {{0, 0}, {3, 3}, {4, 4}, {7, 7}, {2, 5}};
        LevelSession n(lc, Difficulty::Normal, {}, seed);
        check(n.runs().empty(), "BOARD-03 no run");
        check(!n.legal_moves().empty(), "BOARD-03 a legal move without chains");
        check(n.gems_on_board() == 5, "GEM-02 gems_max gems");
        check(n.steel_on_board() == 0 && n.power_ups().count() == 0, "BOARD-04 no steel, no power-ups");
        for (Cell c : lc.chained_cells) check(n.chained({c.x, c.y}), "OBS-05 chained cells from the level");
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                if (n.gem({x, y})) check(!n.steel({x, y}) && !n.power_ups().has({x, y}), "GEM-02 on plain pieces");
    }
    // Chains everywhere but a dead corner: the session reshuffles the movable pieces.
    {
        LevelData lc = base_level();
        lc.board = {4, 4, 4};
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                if (x >= 2 || y >= 2) lc.chained_cells.push_back({x, y});
        bool ok = true;
        for (std::uint64_t seed = 0; seed < 10; ++seed) {
            try {
                LevelSession n(lc, Difficulty::Normal, {}, seed);
                ok = ok && !n.legal_moves().empty() && n.runs().empty() && n.time_left() == 270.0;
            } catch (const std::runtime_error&) {
                // No arrangement of the movable pieces works: reported, not looped.
            }
        }
        check(ok, "BOARD-03 chains: reshuffled with no time penalty");
    }

    // Gems tier.
    for (int tier = 0; tier <= 3; ++tier) {
        Upgrades u;
        u.gems_tier = tier;
        LevelSession g(base_level(), Difficulty::Normal, u, 5);
        check(g.gems_max() == 5 + tier && g.gems_on_board() == 5 + tier, "gems_max = 5 + gems tier");
    }
    for (int bad : {-1, 4}) {
        Upgrades u;
        u.gems_tier = bad;
        check(throws<std::invalid_argument>([&] { LevelSession(base_level(), Difficulty::Normal, u, 1); }),
              "gems tier out of range throws");
    }

    // Gem goal by difficulty (DIFF-03).
    check(LevelSession(base_level(), Difficulty::Normal, {}, 1).gem_goal() == 50, "gem goal normal");
    check(LevelSession(base_level(), Difficulty::Hard, {}, 1).gem_goal() == 75, "gem goal hard");
    check(throws<std::invalid_argument>([] { LevelSession(base_level(), static_cast<Difficulty>(7), {}, 1); }),
          "DIFF-01 only normal and hard");

    // Timer (GOAL-03, DIFF-03).
    const double times[] = {270, 300, 330, 360, 390};
    for (int tier = 0; tier <= 4; ++tier) {
        Upgrades u;
        u.timer_tier = tier;
        check(LevelSession(base_level(), Difficulty::Normal, u, 1).time_left() == times[tier], "timer tier seconds");
        check(LevelSession(base_level(), Difficulty::Hard, u, 1).time_left() == times[tier], "timer same on hard");
    }
    {
        LevelData lt = base_level();
        lt.base_clock = BaseClock{200, 150};
        Upgrades u;
        check(LevelSession(lt, Difficulty::Normal, u, 1).time_left() == 200, "base_clock normal at tier 0");
        check(LevelSession(lt, Difficulty::Hard, u, 1).time_left() == 150, "base_clock hard at tier 0");
        u.timer_tier = 3;
        check(LevelSession(lt, Difficulty::Normal, u, 1).time_left() == 290, "timer tier adds to base_clock normal");
        check(LevelSession(lt, Difficulty::Hard, u, 1).time_left() == 240, "timer tier adds to base_clock hard");
    }
    for (int bad : {-1, 5}) {
        Upgrades u;
        u.timer_tier = bad;
        check(throws<std::invalid_argument>([&] { LevelSession(base_level(), Difficulty::Normal, u, 1); }),
              "timer tier out of range throws");
    }

    // Frenzy tier (FRZ-01, FRZ-02).
    for (int bad : {-1, 5}) {
        Upgrades u;
        u.frenzy_tier = bad;
        check(throws<std::invalid_argument>([&] { LevelSession(base_level(), Difficulty::Normal, u, 1); }),
              "frenzy tier out of range throws");
    }
    check(kFrenzySeconds[1] == 15 && kFrenzySeconds[2] == 20 && kFrenzySeconds[3] == 25 && kFrenzySeconds[4] == 30,
          "FRZ-02 durations");

    // Totem and power-up tiers (PROG-03).
    {
        LevelData lt = level_with_totems({"fire", "gold"}, {"tnt", "clock"});
        Upgrades u;
        u.totem_tiers[static_cast<std::size_t>(TotemId::Fire)] = 3;
        u.totem_tiers[static_cast<std::size_t>(TotemId::Time)] = 4;  // not listed
        u.power_up_tiers[power_up_index(PowerUpId::Tnt)] = 2;
        u.power_up_tiers[power_up_index(PowerUpId::Lightning)] = 3;  // not listed
        LevelSession t(lt, Difficulty::Normal, u, 1);
        check(t.totems().unlocked(TotemId::Fire) && t.totems().tier(TotemId::Fire) == 3, "listed totem at its tier");
        check(t.totems().unlocked(TotemId::Gold) && t.totems().tier(TotemId::Gold) == 1, "listed totem, no tier -> 1");
        check(!t.totems().unlocked(TotemId::Time), "unlisted totem not owned");
        check(t.totems().uses_left(TotemId::Fire) == 2 && t.totems().uses_left(TotemId::Gold) == 1,
              "TOT-06 uses at start");
        check(t.totems().charge_count(TotemId::Fire) == 0, "TOT-06 charges 0");
        const PowerUpTiers& pt = t.power_up_tiers();
        check(pt[power_up_index(PowerUpId::Tnt)] == 2 && pt[power_up_index(PowerUpId::Clock)] == 1 &&
                  pt[power_up_index(PowerUpId::Lightning)] == 0,
              "power-up tiers for listed ids only");
    }
    {
        const auto bad_totem = [](TotemId id, int tier) {
            return throws<std::invalid_argument>(
                [&] { LevelSession(level_with_totems({"ward"}), Difficulty::Normal, with_totem(id, tier), 1); });
        };
        check(bad_totem(TotemId::Ward, 4), "ward tier 4 throws");
        check(bad_totem(TotemId::Fire, 5), "fire tier 5 throws");
        check(!bad_totem(TotemId::Ward, 3), "ward tier 3 fine");
        Upgrades u;
        u.power_up_tiers[0] = 4;
        check(throws<std::invalid_argument>([&] { LevelSession(base_level(), Difficulty::Normal, u, 1); }),
              "power-up tier 4 throws");
    }
    {
        LevelData lb = base_level();
        lb.blocked_cells = {{0, 0}};
        check(throws<std::invalid_argument>([&] { LevelSession(lb, Difficulty::Normal, {}, 1); }),
              "blocked cells not supported yet");
    }

    // Starting state.
    LevelSession st(base_level(), Difficulty::Normal, {}, 9);
    check(st.score() == 0 && st.gems_collected() == 0 && st.frenzy_bar() == 0 && !st.frenzy_on() &&
              st.star_sand() == 0 && st.manual_shuffles_left() == 2 && st.outcome() == Outcome::Playing &&
              st.moves() == 0,
          "starting state");
}

// ---------------------------------------------------------------------------
// Moves and legal moves
// ---------------------------------------------------------------------------

void test_moves() {
    {
        LevelSession s = scene(kSimple);
        check(s.legal_moves().size() >= 1, "simple scene has a move");
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r.has_value() && s.moves() == 1, "MOVE-01 legal swap is a move");
        check(r && !r->steps.empty() && r->steps[0].cleared.size() == 3, "MOVE-01 the run clears");
    }
    {
        LevelSession s = scene(kSimple);
        const Snapshot before = snap(s);
        check(!s.swap({0, 0}, {1, 1}), "diagonal refused");
        check(!s.swap({0, 0}, {2, 0}), "non-adjacent refused");
        check(!s.swap({0, 0}, {0, 0}), "same cell refused");
        check(!s.swap({7, 7}, {8, 7}), "out of bounds refused");
        check(!s.swap({0, 0}, {1, 0}), "MOVE-02 no run refused");
        check(snap(s) == before, "MOVE-02 refused swaps change nothing");
        // The rng did not move: the next legal move resolves as on a fresh copy.
        LevelSession t = scene(kSimple);
        auto r1 = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        auto r2 = t.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r1 && r2 && snap(s) == snap(t), "MOVE-02 refused swap leaves the rng alone");
    }
    {
        LevelSession s = scene(kSimple);
        s.set_steel({2, 6}, true);
        check(!s.swap(kSimpleSwap[0], kSimpleSwap[1]), "MOVE-03 steel cannot be swapped");
        LevelSession c = scene(kSimple);
        c.set_chain({2, 6}, true);
        check(!c.swap(kSimpleSwap[0], kSimpleSwap[1]), "MOVE-03 chained piece cannot be swapped");
        LevelSession m = scene(kSimple);
        m.set_steel({1, 7}, true);
        check(!m.swap(kSimpleSwap[0], kSimpleSwap[1]), "RES-01 a run through steel is no run");
    }
    {
        LevelSession s = scene(kSimple);
        s.set_gems_collected(49);
        s.set_gem({0, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.outcome() == Outcome::Won, "won");
        const Snapshot before = snap(s);
        check(!s.swap({0, 0}, {1, 0}) && !s.tick(1.0), "after Won swap and tick refused");
        check(snap(s) == before, "after Won nothing changes");
        LevelSession l = scene(kSimple);
        l.tick(1000);
        check(l.outcome() == Outcome::Lost && !l.swap(kSimpleSwap[0], kSimpleSwap[1]) && !l.tick(1), "after Lost");
    }

    // legal_moves
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        LevelData l = base_level();
        l.chained_cells = {{1, 1}, {5, 2}};
        LevelSession s(l, Difficulty::Normal, {}, seed);
        s.set_steel({3, 3}, true);
        s.set_steel({6, 6}, true);
        const Snapshot before = snap(s);
        auto moves = s.legal_moves();
        check(snap(s) == before, "legal_moves changes nothing");
        bool ordered = true;
        for (std::size_t i = 0; i < moves.size(); ++i) {
            const Cell a = moves[i][0], b = moves[i][1];
            ordered = ordered && ((b.x == a.x + 1 && b.y == a.y) || (b.x == a.x && b.y == a.y + 1));
            if (i > 0) {
                const Cell p = moves[i - 1][0];
                const bool row_major = p.y < a.y || (p.y == a.y && p.x < a.x) ||
                                       (p == a && moves[i - 1][1].x > p.x && b.y > a.y);
                ordered = ordered && row_major;
            }
            ordered = ordered && !s.steel(a) && !s.steel(b) && !s.chained(a) && !s.chained(b);
        }
        check(ordered, "legal_moves ordered, (a,b) right or below, no steel or chains");
        // Every listed swap is accepted; every unlisted adjacent swap is refused.
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                for (Cell b : {Cell{x + 1, y}, Cell{x, y + 1}}) {
                    if (b.x > 7 || b.y > 7) continue;
                    const bool listed = std::find(moves.begin(), moves.end(), Swap{Cell{x, y}, b}) != moves.end();
                    LevelSession copy = s;
                    const bool accepted = copy.swap({x, y}, b).has_value();
                    check(listed == accepted, "legal_moves agrees with swap()");
                }
    }
    // After every completed move legal_moves is non-empty (SHUF-02).
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 11);
        for (int i = 0; i < 60 && s.outcome() == Outcome::Playing; ++i) {
            auto m = s.legal_moves();
            auto r = s.swap(m.front()[0], m.front()[1]);
            check(r.has_value(), "first legal move accepted");
            if (s.outcome() == Outcome::Playing) check(!s.legal_moves().empty(), "legal move after each move");
        }
    }
}

// ---------------------------------------------------------------------------
// Resolution order
// ---------------------------------------------------------------------------

void test_resolution() {
    // RES-01 / OBS-01 acceptance: R R S R R is no run.
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 1);
        paint(s, {"........", "........", "........", "........", "........", "........", "........", "00000560"});
        s.set_steel({2, 7}, true);
        check(s.runs().empty(), "OBS-01 steel red between two red pairs: no run");
        s.set_steel({2, 7}, false);
        check(!s.runs().empty(), "without steel it is a run");
    }
    // RES-02: L counted once.
    {
        LevelSession s = scene(kEll);
        auto r = s.swap(kEllSwap[0], kEllSwap[1]);
        check(r && r->steps[0].runs.size() == 2 && r->steps[0].cleared.size() == 5, "RES-02 L cleared once");
        check(r && r->steps[0].points == 50, "RES-02 L scored once per cell");
    }
    // RES-06 step index increments; cascade scene.
    {
        LevelSession s = scene(kCascade);
        auto r = s.swap(kCascadeSwap[0], kCascadeSwap[1]);
        check(r && r->steps.size() >= 2, "RES-06 cascade makes a second step");
        for (std::size_t i = 0; r && i < r->steps.size(); ++i) check(r->steps[i].step == static_cast<int>(i), "step index");
        check(r && r->steps[1].runs.size() >= 1 && r->steps[1].runs[0].kind == kOrange, "cascade orange run");
    }
    // Cascade cap.
    {
        check(Board::kMaxCascadeSteps == 100, "named cascade bound");
        // Board with hooks capping at 1 step: a move that would cascade stops, stuck.
        Board b(8, 8, 7, 1);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) b.set(x, y, bg(x, y));
        const std::vector<std::string>& rows = kCascade;
        for (int y = 6; y < 8; ++y)
            for (int x = 0; x < 8; ++x) b.set(x, y, rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] - '0');
        StepHooks hooks;
        hooks.max_steps = 1;
        hooks.settle_shuffle = false;
        auto r = b.try_swap(kCascadeSwap[0], kCascadeSwap[1], &hooks);
        check(r && r->cascades.size() == 1 && r->stuck, "cascade cap reached reports stuck");
        check(!b.find_runs().empty(), "cap stops resolving with runs left");
    }
    // Board's own end-of-move shuffle does not run inside the swap: the
    // session reshuffles after RES-08 and reports it.
    {
        // A scene with exactly one legal move; after it the dead background
        // likely has none, so the session's auto-reshuffle shows up.
        int auto_shuffles = 0, board_shuffles = 0;
        for (std::uint64_t seed = 0; seed < 20; ++seed) {
            LevelSession s = scene(kSimple, base_level(), {}, seed);
            auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
            if (r && r->result.shuffled) ++board_shuffles;
            if (r && r->auto_shuffle) ++auto_shuffles;
        }
        check(board_shuffles == 0, "board never shuffles inside the move");
        check(auto_shuffles > 0, "session reshuffles after the move (RES-08)");
    }
}

// ---------------------------------------------------------------------------
// Gems
// ---------------------------------------------------------------------------

void test_gems() {
    // GEM-03 by run.
    {
        LevelSession s = scene(kSimple);
        s.set_gem({0, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.gems_collected() == 1 && r->gems_collected == 1, "GEM-03 gem in a run collected");
        check(r && r->steps[0].gems_collected == std::vector<Cell>{{0, 7}}, "GEM-03 reported cell");
        check(r && r->steps[0].points == 30 + 50, "GEM-03 / SCORE-02 +50");
    }
    // GEM-03 by an effect: a gem in tnt's area.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"tnt"}));
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Tnt, 1});
        s.set_gem({0, 6}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && has_cell(r->steps[0].gems_collected, {0, 6}) && s.gems_collected() >= 1,
              "GEM-03 gem cleared by tnt collected");
    }
    // Swapping a gem piece moves the gem with it.
    {
        LevelSession s = scene(kSimple);
        s.set_gem({2, 6}, true);  // the blue piece that moves down into the run
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].gems_collected == std::vector<Cell>{{2, 7}}, "gem moves with its swapped piece");
        LevelSession t = scene(kSimple);
        t.set_gem({2, 7}, true);  // moves up to (2,6), not in the run
        auto r2 = t.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r2 && t.gems_collected() == 0, "gem moving out of the run is not collected");
        // It fell back to (2,7) with the column.
        if (r2 && !r2->auto_shuffle) check(t.gem({2, 7}), "GEM-05 gem falls with its piece");
    }
    // GEM-04 acceptance: 5 max, collect 2 -> 5 on board, both new on spawns.
    {
        LevelSession s = scene(kSimple);
        // Three gems elsewhere plus two in the run.
        s.set_gem({5, 0}, true);
        s.set_gem({6, 1}, true);
        s.set_gem({7, 2}, true);
        s.set_gem({0, 7}, true);
        s.set_gem({1, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.gems_on_board() == 5, "GEM-04 refilled to gems_max");
        std::vector<Cell> placed;
        for (const StepReport& st : r->steps) placed.insert(placed.end(), st.gems_placed.begin(), st.gems_placed.end());
        check(placed.size() == 2, "GEM-04 two new gems");
        for (Cell c : placed) {
            bool on_spawn = false;
            for (const StepReport& st : r->steps)
                for (const Spawn& sp : st.spawns) on_spawn = on_spawn || sp.to == c;
            check(on_spawn, "GEM-04 new gems on spawned pieces");
        }
    }
    // GEM-04 shortfall waits: 4 missing, 3 spawns.
    {
        LevelSession s = scene(kSimple);
        s.set_gem({0, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].gems_placed.size() == 3, "GEM-04 three spawns take three gems");
        check(r && s.gems_on_board() <= 5, "GEM-04 at most gems_max");
        // Step 0 spawned 3, so 5 - 0 left on board + up to 3 placed; later steps fill the rest.
        int placed = 0;
        for (const StepReport& st : r->steps) placed += static_cast<int>(st.gems_placed.size());
        check(placed == s.gems_on_board(), "GEM-04 only placements on new pieces");
    }
    // GEM-04 no gem when at gems_max.
    {
        LevelSession s = scene(kSimple);
        for (int x = 0; x < 6; ++x) s.set_gem({x, 0}, true);  // 6 > 5 (harvest overfill)
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].gems_placed.empty(), "GEM-04 none placed when gems_on_board >= gems_max");
    }
    // GEM-04 placement order: one draw per gem over spawns order.
    {
        LevelSession s = scene(kSimple);
        Rng rng(derive_seed(1, 2));
        (void)rng;  // the session's own stream; covered by the determinism test
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        for (const StepReport& st : r->steps)
            for (Cell c : st.gems_placed) check(!s.steel(c), "GEM-01 placed on plain pieces");
    }
    // GEM-05: shuffles keep gems on their pieces.
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 4);
        std::vector<int> kinds_with_gem;
        const auto before = s.board().cells();
        const auto gems_before = s.gem_marks();
        auto sh = s.shuffle();
        check(sh && sh->shuffled, "manual shuffle");
        if (sh && !sh->redealt) {
            bool follows = true;
            std::vector<bool> expect = gems_before;
            for (const auto& [from, to] : sh->shuffle_map)
                expect[static_cast<std::size_t>(to.y * 8 + to.x)] = gems_before[static_cast<std::size_t>(from.y * 8 + from.x)];
            follows = expect == s.gem_marks();
            check(follows, "GEM-05 gems follow shuffle_map");
        }
        check(s.gems_on_board() == 5, "GEM-05 count kept");
    }
}

// ---------------------------------------------------------------------------
// Goal, timer and difficulty
// ---------------------------------------------------------------------------

void test_goal_and_timer() {
    {
        LevelData l = base_level();
        l.gem_goal = GemGoal{10, 15};
        LevelSession s = scene(kSimple, l);
        s.set_gems_collected(9);
        s.set_gem({1, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->outcome == Outcome::Won && s.outcome() == Outcome::Won, "GOAL-01 acceptance");
        check(r && !r->auto_shuffle, "SHUF-02 no reshuffle when the move won");
    }
    {
        LevelData l = base_level();
        l.gem_goal = GemGoal{10, 15};
        LevelSession s = scene(kSimple, l);
        s.set_gems_collected(9);
        s.set_gem({0, 7}, true);
        s.set_gem({1, 7}, true);
        s.set_time_left(0.0);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.outcome() == Outcome::Won && s.gems_collected() == 11, "GOAL-02 over the goal wins, full count");
        check(r && s.outcome() == Outcome::Won, "GOAL-02 won even at time 0");
    }
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 1);
        auto t = s.tick(0);
        check(t && s.time_left() == 270 && !t->outcome_changed, "GOAL-04 tick(0) changes nothing");
        s.tick(20.5);
        check(s.time_left() == 249.5, "GOAL-04 tick lowers time_left");
        check(throws<std::invalid_argument>([&] { s.tick(-1); }), "GOAL-04 negative dt throws");
        auto t2 = s.tick(249.5);
        check(t2 && t2->outcome_changed && s.outcome() == Outcome::Lost, "GOAL-01 time out -> Lost");
    }
    // GOAL-06 quotas.
    {
        LevelData l = base_level();
        l.gem_goal.reset();
        l.quotas = {{kBlue, 3}};
        LevelSession s = scene(kSimple, l);
        s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(s.outcome() == Outcome::Won, "GOAL-06 quotas only: wins on quotas");
        LevelData both = base_level();
        both.quotas = {{kBlue, 3}};
        LevelSession b = scene(kSimple, both);
        b.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(b.outcome() == Outcome::Playing, "GOAL-06 quotas met, gems not: still playing");
        LevelSession b2 = scene(kSimple, both);
        b2.set_gems_collected(50);
        b2.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(b2.outcome() == Outcome::Won, "GOAL-06 both met: won");
        LevelData gem_only = base_level();
        gem_only.gem_goal = GemGoal{1, 1};
        LevelSession g = scene(kSimple, gem_only);
        g.set_gem({0, 7}, true);
        g.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(g.outcome() == Outcome::Won, "gem goal only: gems alone");
        // A quota counts effect-cleared pieces too.
        LevelData q = level_with_totems({}, {"tnt"});
        q.gem_goal.reset();
        q.quotas = {{kOrange, 1}};
        LevelSession e = scene(kSimple, q);
        e.set_power_up({0, 7}, PowerUp{PowerUpId::Tnt, 1});  // reaches (1,6) orange? (0,6)=5,(1,6)=6
        e.set_piece({1, 6}, kOrange);
        auto r = e.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && e.cleared_by_kind().at(kOrange) >= 1 && e.outcome() == Outcome::Won, "GOAL-06 effect clears count");
    }
    // GOAL-05: time effects add with no cap.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"clock"}));
        s.set_power_up({0, 7}, PowerUp{PowerUpId::Clock, 1});
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.time_left() >= 285.0 - 5.0 && r->time_delta_seconds >= 10.0, "GOAL-05 clock adds time");
        check(r && r->steps[0].power_up_effects.size() == 1, "clock reported");
        LevelSession t = scene(kSimple, level_with_totems({}, {"clock"}));
        t.set_power_up({0, 7}, PowerUp{PowerUpId::Clock, 1});
        auto r2 = t.swap(kSimpleSwap[0], kSimpleSwap[1]);
        if (r2 && !r2->auto_shuffle) check(t.time_left() == 285.0, "GOAL-05 above the start value");
    }
    // DIFF-03: hard changes only the goal.
    {
        LevelData l = base_level();
        l.steel_rate = 0.05;
        l.chained_cells = {{2, 2}};
        LevelSession n(l, Difficulty::Normal, {}, 5);
        LevelSession h(l, Difficulty::Hard, {}, 5);
        check(n.board().cells() == h.board().cells() && n.time_left() == h.time_left() &&
                  n.chain_marks() == h.chain_marks() && n.gem_marks() == h.gem_marks() && n.gem_goal() != h.gem_goal(),
              "DIFF-03 hard changes only the gem goal");
    }
}

// ---------------------------------------------------------------------------
// Scoring and Frenzy
// ---------------------------------------------------------------------------

void test_scoring() {
    {
        LevelSession s = scene(kCascade);
        auto r = s.swap(kCascadeSwap[0], kCascadeSwap[1]);
        check(r && r->steps[0].points == 30, "SCORE-01 step 0 piece 10");
        check(r && r->steps[1].points == static_cast<int>(r->steps[1].cleared.size()) * 20, "SCORE-01 step 1 piece 20");
        int sum = 0;
        for (const StepReport& st : r->steps) sum += st.points;
        check(r && r->points == sum && s.score() == sum, "move points are the step sum");
    }
    // SCORE-03 Frenzy doubles pieces and gems.
    {
        Upgrades u;
        u.frenzy_tier = 1;
        LevelSession s = scene(kSimple, base_level(), u);
        s.set_frenzy(true, 10);
        s.set_gem({0, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].points == 2 * (30 + 50) && r->steps[0].frenzy, "SCORE-03 doubled");
        check(r && s.star_sand() == 1 && r->star_sand_gained == 1, "FRZ-04 star sand during Frenzy");
        LevelSession o = scene(kSimple, base_level(), u);
        o.set_gem({0, 7}, true);
        o.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(o.star_sand() == 0, "FRZ-04 no star sand outside Frenzy");
    }
    // SCORE-04 / PWR-10 multiplier after Frenzy.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"multiplier"}));
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Multiplier, 1});
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->multiplier == 5 && r->points == r->base_points * 5 && s.score() == r->points,
              "PWR-10 x5 whole move");
        Upgrades u;
        u.frenzy_tier = 1;
        LevelSession f = scene(kSimple, level_with_totems({}, {"multiplier"}), u);
        f.set_frenzy(true, 10);
        f.set_power_up({1, 7}, PowerUp{PowerUpId::Multiplier, 1});
        auto rf = f.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(rf && rf->steps[0].points == 60 && rf->points == rf->base_points * 5, "SCORE-04 after Frenzy");
        // Two multipliers: x7 only.
        Upgrades u2;
        u2.power_up_tiers[power_up_index(PowerUpId::Multiplier)] = 2;
        LevelSession t = scene(kSimple, level_with_totems({}, {"multiplier"}), u2);
        t.set_power_up({0, 7}, PowerUp{PowerUpId::Multiplier, 1});
        t.set_power_up({1, 7}, PowerUp{PowerUpId::Multiplier, 2});
        auto rt = t.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(rt && rt->multiplier == 7 && rt->points == rt->base_points * 7, "PWR-10 two multipliers: highest");
    }
    // PWR-10 acceptance: a move worth 400 with x5 scores 2000.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"multiplier"}));
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Multiplier, 1});
        for (Cell c : {Cell{0, 7}, Cell{1, 7}, Cell{2, 6}}) s.set_gem(c, true);
        // 3 pieces 30 + 3 gems 150 = 180 in step 0; make it 400 with frenzy-free math:
        // add gems so step 0 is 30 + 50 * n. 400 is not reachable that way, so check
        // the scaling law on whatever the move was worth.
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->points == 5 * r->base_points, "PWR-10 move scaled by 5");
    }
    // SCORE-05 score never affects the outcome.
    {
        LevelSession s = scene(kSimple);
        s.set_score(1000000);
        s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(s.outcome() == Outcome::Playing, "SCORE-05");
    }
    // SHUF-04 hint costs 300.
    {
        LevelSession s = scene(kSimple);
        check(!s.buy_hint() && s.score() == 0, "SHUF-04 below 300 refused");
        s.set_score(450);
        auto h = s.buy_hint();
        check(h && s.score() == 150 && s.hint() == h && s.is_legal((*h)[0], (*h)[1]), "SHUF-04 hint for 300");
        s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(!s.hint(), "SHUF-04 hint cleared by the next move");
    }
}

void test_frenzy() {
    Upgrades u;
    u.frenzy_tier = 1;
    // FRZ-01
    {
        LevelSession s = scene(kSimple, base_level(), u);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        int cleared = 0;
        for (const StepReport& st : r->steps) cleared += static_cast<int>(st.cleared.size());
        check(s.frenzy_bar() == std::min(60, cleared), "FRZ-01 +1 per cleared piece");
        LevelSession z = scene(kSimple);
        z.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(z.frenzy_bar() == 0, "FRZ-01 locked at tier 0");
    }
    // FRZ-02 acceptance.
    {
        LevelSession s = scene(kCascade, base_level(), u);
        s.set_frenzy_bar(58);
        auto r = s.swap(kCascadeSwap[0], kCascadeSwap[1]);
        check(r && r->steps[0].points == 30 && !r->steps[0].frenzy, "FRZ-02 step 0 not doubled");
        check(r && r->steps[0].frenzy_started && r->steps[0].frenzy_bar_after == 60, "FRZ-02 starts at end of step 0");
        check(r && r->steps.size() >= 2 && r->steps[1].frenzy &&
                  r->steps[1].points == static_cast<int>(r->steps[1].cleared.size()) * 20 * 2,
              "FRZ-02 step 1 doubled");
        if (r && r->steps[1].cleared.size() == 3) check(r->steps[1].points == 120, "FRZ-02 acceptance 120");
        check(s.frenzy_on() && s.frenzy_left() == 15 && s.frenzy_bar() == 60, "FRZ-02 on for 15 s, bar clamped");
        check(r && r->frenzy_started, "move reports Frenzy started");
        // FRZ-03 / FRZ-05
        auto t = s.tick(5);
        check(t && s.frenzy_left() == 10 && s.time_left() == 265, "FRZ-05 counts down with the timer");
        LevelSession more = s;
        auto m = more.legal_moves();
        more.swap(m[0][0], m[0][1]);
        check(more.frenzy_bar() == 60, "FRZ-03 bar does not fill during Frenzy");
        auto t2 = s.tick(10);
        check(t2 && t2->frenzy_ended && !s.frenzy_on() && s.frenzy_bar() == 0, "FRZ-03 ends, bar resets");
    }
    for (int tier = 1; tier <= 4; ++tier) {
        Upgrades ut;
        ut.frenzy_tier = tier;
        LevelSession s = scene(kSimple, base_level(), ut);
        s.set_frenzy_bar(59);
        s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(s.frenzy_on() && s.frenzy_left() == kFrenzySeconds[static_cast<std::size_t>(tier)],
              "FRZ-02 duration by tier");
    }
}

// ---------------------------------------------------------------------------
// Obstacles
// ---------------------------------------------------------------------------

void test_obstacles() {
    // OBS-02 steel_rate 1: exactly 8, first 8 in spawns order.
    {
        LevelData l = base_level();
        l.steel_rate = 1.0;
        // Clear 10 pieces: a horizontal run of 3 and TNT tier 1 at (4,7) area.
        LevelData lt = l;
        lt.powerups_unlocked = {"lightning"};
        Upgrades u;
        u.power_up_tiers[power_up_index(PowerUpId::Lightning)] = 3;
        LevelSession s = scene(kSimple, lt, u);
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Lightning, 3});
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].spawns.size() >= 8, "enough spawns");
        if (r) {
            std::vector<Cell> first8;
            for (std::size_t i = 0; i < 8 && i < r->steps[0].spawns.size(); ++i) first8.push_back(r->steps[0].spawns[i].to);
            check(r->steps[0].steel_spawned == first8, "OBS-02 rate 1: first 8 spawns steel");
            check(s.steel_on_board() <= 8, "OBS-02 at most 8 steel");
            check(r->steps.size() == 1 || r->steps[1].steel_spawned.empty() || s.steel_on_board() <= 8, "cap holds");
        }
    }
    // OBS-02 rate 0 draws nothing: same rng sequence as no steel at all.
    {
        LevelData a = base_level();
        LevelData b = base_level();
        b.steel_rate = 0.0;
        LevelSession sa = scene(kSimple, a);
        LevelSession sb = scene(kSimple, b);
        sa.swap(kSimpleSwap[0], kSimpleSwap[1]);
        sb.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(snap(sa) == snap(sb), "OBS-02 rate 0 draws nothing");
    }
    // OBS-03: a run next to steel does not clear it; steel falls (OBS-04).
    {
        LevelSession s = scene(kSimple);
        s.set_steel({3, 7}, true);  // right of the run
        s.set_steel({0, 5}, true);  // above the run, falls one row
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.steel({3, 7}), "OBS-03 run next to steel leaves it");
        check(r && s.steel({0, 6}) && !s.steel({0, 5}), "OBS-04 steel falls with its piece");
    }
    // OBS-03 tnt and lightning clear steel; steel scores and fills the bar.
    {
        Upgrades u;
        u.frenzy_tier = 1;
        LevelSession s = scene(kSimple, level_with_totems({}, {"tnt"}), u);
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Tnt, 1});
        s.set_steel({1, 6}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && has_cell(r->steps[0].cleared, {1, 6}) && has_cell(r->steps[0].steel_removed, {1, 6}),
              "OBS-03 tnt clears steel");
        check(r && r->steps[0].points == static_cast<int>(r->steps[0].cleared.size()) * 10, "OBS-03 steel scores");
        LevelSession l = scene(kSimple, level_with_totems({}, {"lightning"}));
        l.set_power_up({1, 7}, PowerUp{PowerUpId::Lightning, 1});
        l.set_steel({1, 3}, true);
        auto rl = l.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(rl && has_cell(rl->steps[0].steel_removed, {1, 3}), "OBS-03 lightning clears steel");
    }
    // OBS-03 color_clear tier 1 spares steel of its kind; tier 2 clears it.
    for (int tier = 1; tier <= 2; ++tier) {
        Upgrades u;
        u.power_up_tiers[power_up_index(PowerUpId::ColorClear)] = tier;
        LevelSession s = scene(kSimple, level_with_totems({}, {"color_clear"}), u);
        s.set_power_up({0, 7}, PowerUp{PowerUpId::ColorClear, tier});
        // (4,0) is blue in the background.
        s.set_steel({4, 0}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        const bool cleared = r && has_cell(r->steps[0].cleared, {4, 0});
        check(tier == 1 ? !cleared : cleared, "OBS-03 color_clear steel by tier");
    }
    // OBS-05: a run through a chained piece breaks the chain; the piece
    // stays. Chain marks fall with their pieces.
    {
        LevelSession s = scene(kSimple);
        s.set_chain({0, 7}, true);
        s.set_chain({1, 5}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].cleared == std::vector<Cell>{{1, 7}, {2, 7}} &&
                  r->steps[0].chains_broken == std::vector<Cell>{{0, 7}},
              "OBS-05 chained piece holds, chain removed");
        check(r && !s.chained({0, 7}), "OBS-05 held piece is plain now");
        check(r && !s.chained({1, 5}) && s.chained({1, 6}), "OBS-05 chain falls with its piece");
    }
    // OBS-06: shuffles leave steel and chained pieces.
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 3);
        s.set_steel({2, 2}, true);
        s.set_chain({5, 5}, true);
        const int k1 = s.board().at(2, 2), k2 = s.board().at(5, 5);
        for (int i = 0; i < 2; ++i) {
            auto sh = s.shuffle();
            check(sh && sh->shuffled, "manual shuffle done");
            check(s.steel({2, 2}) && s.chained({5, 5}) && s.board().at(2, 2) == k1 && s.board().at(5, 5) == k2,
                  "OBS-06 steel and chains stay");
        }
    }
}

// ---------------------------------------------------------------------------
// Shuffles and hints
// ---------------------------------------------------------------------------

void test_shuffles() {
    {
        LevelSession s(level_with_totems({"fire"}), Difficulty::Normal, {}, 2);
        const double t = s.time_left();
        auto a = s.shuffle();
        auto b = s.shuffle();
        auto c = s.shuffle();
        check(a && b && !c, "SHUF-01 twice per level");
        check(s.time_left() == t && s.moves() == 0 && s.manual_shuffles_left() == 0, "SHUF-01 free, not a move");
        check(s.totems().charge_count(TotemId::Fire) == 0, "SHUF-01 no totem charge");
        check(s.runs().empty() && !s.legal_moves().empty(), "SHUF-03 still board with a move");
        // SHUF-03 power-ups move with their pieces.
    }
    {
        LevelSession s(level_with_totems({}, {"tnt"}), Difficulty::Normal, {}, 2);
        s.set_power_up({3, 3}, PowerUp{PowerUpId::Tnt, 1});
        const int kind = s.board().at(3, 3);
        auto sh = s.shuffle();
        check(sh && sh->shuffled && s.power_ups().count() == 1, "SHUF-03 power-up kept");
        if (sh && !sh->redealt) {
            Cell to{3, 3};
            for (const auto& [from, dest] : sh->shuffle_map)
                if (from == Cell{3, 3}) to = dest;
            check(s.power_ups().has(to) && s.board().at(to.x, to.y) == kind, "SHUF-03 power-up on its piece");
        }
    }
    // SHUF-02 acceptance: no legal move after a move -> reshuffle and -5 s.
    {
        bool seen = false;
        for (std::uint64_t seed = 0; seed < 30 && !seen; ++seed) {
            LevelSession s = scene(kSimple, base_level(), {}, seed);
            auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
            if (!r || !r->auto_shuffle) continue;
            seen = true;
            double clocks = 0;
            check(s.time_left() == 270 - 5 + clocks && r->auto_shuffle->time_delta_seconds == -5,
                  "SHUF-02 -5 s");
            check(s.manual_shuffles_left() == 2, "SHUF-02 not a manual shuffle");
            check(!s.legal_moves().empty() && s.runs().empty(), "SHUF-03 after auto-reshuffle");
        }
        check(seen, "SHUF-02 happens");
    }
    // SHUF-02 penalty below 5 s loses.
    {
        bool seen = false;
        for (std::uint64_t seed = 0; seed < 30 && !seen; ++seed) {
            LevelSession s = scene(kSimple, base_level(), {}, seed);
            s.set_time_left(3);
            auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
            if (!r || !r->auto_shuffle) continue;
            seen = true;
            check(s.time_left() == 0 && s.outcome() == Outcome::Lost, "penalty below 5 s loses");
        }
        check(seen, "penalty case seen");
    }
    // SHUF-03 failure: nothing movable can make a move -> stuck, unchanged.
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 1);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) s.set_chain({x, y}, true);
        s.set_chain({0, 0}, false);
        s.set_chain({1, 0}, false);
        const Snapshot before = snap(s);
        auto sh = s.shuffle();
        check(sh && sh->stuck && !sh->shuffled, "SHUF-03 failure reports stuck");
        check(snap(s) == before && s.manual_shuffles_left() == 2, "SHUF-03 failure leaves the board");
    }
    // SHUF-05 free hint on e1-l01 / e1-l02 only.
    for (const char* id : {"e1-l01", "e1-l02", "e1-l03"}) {
        LevelData l = base_level();
        l.id = id;
        l.index = id[5] - '0';
        LevelSession s(l, Difficulty::Normal, {}, 1);
        int hints = 0;
        for (int i = 0; i < 10; ++i) {
            auto t = s.tick(1.0);
            if (t && t->free_hint) ++hints;
        }
        const bool expect = std::string(id) != "e1-l03";
        check(hints == (expect ? 1 : 0), "SHUF-05 free hint once after 8 s on " + std::string(id));
        if (expect) {
            auto m = s.legal_moves();
            s.swap(m[0][0], m[0][1]);
            int again = 0;
            for (int i = 0; i < 9; ++i) {
                auto t = s.tick(1.0);
                if (t && t->free_hint) ++again;
            }
            check(again == (s.outcome() == Outcome::Playing ? 1 : 0), "SHUF-05 again after the next move");
        }
    }
}

// ---------------------------------------------------------------------------
// Power-ups
// ---------------------------------------------------------------------------

int placed_power_ups(const MoveReport& r) {
    int n = 0;
    for (const StepReport& st : r.steps) n += static_cast<int>(st.power_ups_placed.size());
    return n;
}

void test_power_ups() {
    const LevelData tnt = level_with_totems({}, {"tnt"});
    {
        LevelSession s = scene(kRun4, tnt);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].power_ups_placed.size() == 1, "PWR-01 run of 4 earns 1 after its refill");
        LevelSession f = scene(kRun5, tnt);
        auto r5 = f.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r5 && r5->steps[0].power_ups_placed.size() == 2, "PWR-01 run of 5 earns 2");
        LevelSession e = scene(kEll, tnt);
        auto rl = e.swap(kEllSwap[0], kEllSwap[1]);
        check(rl && rl->steps[0].power_ups_placed.empty(), "PWR-01 L of 3+3 earns none");
        for (const PowerUpPlacement& p : r->steps[0].power_ups_placed)
            check(p.power_up.id == PowerUpId::Tnt && !s.gem(p.cell) && !s.steel(p.cell), "PWR-02 on a plain piece");
    }
    {
        LevelSession s = scene(kRun4);  // nothing unlocked
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        LevelSession t = scene(kSimple);
        (void)t;
        check(r && placed_power_ups(*r) == 0, "PWR-02 none unlocked: none placed");
    }
    // PWR-02 acceptance: 4 on board, more dropped.
    {
        LevelSession s = scene(kRun4, tnt);
        for (int x = 4; x < 8; ++x) s.set_power_up({x, 0}, PowerUp{PowerUpId::Tnt, 1});
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].power_ups_placed.empty() && s.power_ups().count() <= 4, "PWR-02 cap 4");
    }
    // PWR-04 acceptance: tnt in the run, clock in its 3x3.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"tnt", "clock"}));
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Tnt, 1});
        s.set_power_up({2, 6}, PowerUp{PowerUpId::Clock, 1});  // swaps down into the run? no: (2,6) is the swapped piece
        s.set_power_up({2, 6}, std::nullopt);
        s.set_power_up({0, 6}, PowerUp{PowerUpId::Clock, 1});
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].power_ups_triggered.size() == 2, "PWR-04 chained reaction in one step");
        check(r && r->time_delta_seconds >= 15.0 - 5.0, "PWR-04 clock +15");
        std::set<std::pair<int, int>> cells;
        for (Cell c : r->steps[0].cleared) cells.insert({c.x, c.y});
        bool square = true;
        for (int y = 6; y <= 7; ++y)
            for (int x = 0; x <= 2; ++x) square = square && cells.count({x, y});
        check(square, "PWR-04 3x3 + run in one step");
        check(r && r->steps[0].power_up_effects[1].time_delta_seconds == 15.0, "clock effect 15 s");
    }
    // PWR-05: power-up cells do not charge totems.
    {
        LevelSession s = scene(kSimple, level_with_totems({"fire"}, {"tnt"}));
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Tnt, 1});
        s.set_piece({1, 6}, kRed);  // cleared by tnt only
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && s.totems().charge_count(TotemId::Fire) == 0, "PWR-05 tnt-cleared red does not charge");
    }
    // PWR-06 sizes.
    for (int tier = 1; tier <= 3; ++tier) {
        const int expect[] = {0, 9, 13, 21};
        Board b(8, 8, 7, 1);
        auto e = power_up_effect(PowerUpId::Tnt, tier, b, {4, 4});
        check(static_cast<int>(e.cells_to_clear.size()) == expect[tier], "PWR-06 tnt size by tier");
    }
    // PWR-07 acceptance: 9 blue, color_clear tier 1 clears all 9.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"color_clear"}));
        s.set_power_up({0, 7}, PowerUp{PowerUpId::ColorClear, 1});
        int blue = 0;
        for (int c : s.board().cells()) blue += c == kBlue;
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].cleared_by_kind.at(kBlue) == blue && blue == 12, "PWR-07 every blue cleared");
    }
    // PWR-07 a kind with only its own piece clears just that piece.
    {
        Board b(3, 3, 3, 1);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) b.set(x, y, (x + y) % 2 + 1);
        b.set(1, 1, 0);
        auto e = power_up_effect(PowerUpId::ColorClear, 1, b, {1, 1});
        check(e.cells_to_clear == std::vector<Cell>{{1, 1}}, "PWR-07 lone kind: just its piece");
    }
    // PWR-08 acceptance: lightning tier 1 at (3,5) clears (3,0)..(3,5).
    {
        Board b(8, 8, 7, 1);
        auto e = power_up_effect(PowerUpId::Lightning, 1, b, {3, 5});
        std::vector<Cell> expect;
        for (int y = 0; y <= 5; ++y) expect.push_back({3, y});
        check(e.cells_to_clear == expect, "PWR-08 acceptance");
    }
    // PWR-09 clocks sum; PWR-11 hint seconds.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"clock", "hint"}));
        s.set_power_up({0, 7}, PowerUp{PowerUpId::Clock, 1});
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Clock, 1});
        s.set_power_up({2, 6}, PowerUp{PowerUpId::Hint, 1});  // moves into the run
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        double clocks = 0;
        for (const StepReport& st : r->steps)
            for (const PowerUpEffect& e : st.power_up_effects) clocks += e.time_delta_seconds;
        check(r && clocks == 30.0, "PWR-09 two clocks sum");
        check(r && r->hint_seconds == 20.0, "PWR-11 hint 20 s");
    }
}

// ---------------------------------------------------------------------------
// Totems
// ---------------------------------------------------------------------------

// A run of exactly 3 of `kind` in row 7 x 0..2: swap (2,6) with (2,7).
std::vector<std::string> kind_rows(int k) {
    std::string row6 = "56012345";
    std::string row7 = "00004560";
    row6[2] = static_cast<char>('0' + k);
    row7[0] = row7[1] = static_cast<char>('0' + k);
    row7[2] = static_cast<char>('0' + (k + 2) % 7);
    row7[3] = static_cast<char>('0' + (k + 3) % 7);
    return {"........", "........", "........", "........", "........", "........", row6, row7};
}
// Red run x 0..2 and orange run x 3..5 in row 7: swap (2,7) with (3,7).
const std::vector<std::string> kRedOrange{"........", "........", "........", "........",
                                          "........", "........", "........", "00101160"};
const Swap kRedOrangeSwap{Cell{2, 7}, Cell{3, 7}};

// Repaints the scene and plays its swap; totem charges carry over.
std::optional<MoveReport> play(LevelSession& s, int kind) {
    paint(s, kind_rows(kind));
    return s.swap(kSimpleSwap[0], kSimpleSwap[1]);
}

const TotemFiring* fired(const std::optional<MoveReport>& r, TotemId id) {
    if (!r) return nullptr;
    for (const TotemFiring& f : r->totems_fired)
        if (f.effect.totem == id) return &f;
    return nullptr;
}

void test_totems() {
    // Scenes are sound.
    for (int k = 0; k < 7; ++k) {
        LevelSession s = scene(kind_rows(k));
        check(s.runs().empty(), "kind scene still " + std::to_string(k));
        auto runs = s.runs_for_swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(runs.size() == 1 && runs[0].kind == k && runs[0].cells.size() == 3, "kind scene run " + std::to_string(k));
    }
    // TOT-02 / TOT-03 acceptance.
    {
        LevelSession s = scene(kBlank, level_with_totems({"fire"}), with_totem(TotemId::Fire, 1));
        auto r1 = play(s, kRed);
        check(r1 && s.totems().charge_count(TotemId::Fire) == 1, "TOT-02 red match charges 1");
        check(r1 && r1->totem_charges.size() == 1 && r1->totem_charges[0].charge == 0.5, "charge reported 0.5");
        auto rb = play(s, kBlue);
        check(rb && s.totems().charge_count(TotemId::Fire) == 0, "TOT-02 blue match resets");
        play(s, kRed);
        auto r3 = play(s, kRed);
        const TotemFiring* f = fired(r3, TotemId::Fire);
        check(f != nullptr && s.totems().charge_count(TotemId::Fire) == 0 && s.totems().uses_left(TotemId::Fire) == 0,
              "TOT-03 fires after two, charge 0, use spent");
        check(r3 && r3->totem_charges[0].charge == 1.0, "fired totem reported at 1");
        // TOT-10 fire tier 1: 6 distinct pieces.
        check(f && f->effect.cells_to_clear.size() == 6, "TOT-10 6 pieces");
        check(f && f->steps.size() >= 1 && f->steps[0].cleared.size() >= 6, "TOT-10 cleared as a step");
        check(f && f->steps[0].step == static_cast<int>(r3->steps.size()), "totem steps continue the numbering");
        if (f)
            check(f->steps[0].points == static_cast<int>(f->steps[0].cleared.size()) * 10 * (f->steps[0].step + 1) +
                                            50 * static_cast<int>(f->steps[0].gems_collected.size()),
                  "SCORE-01 totem steps score as further steps");
        // Same seed, same 6.
        LevelSession t = scene(kBlank, level_with_totems({"fire"}), with_totem(TotemId::Fire, 1));
        play(t, kRed);
        play(t, kBlue);
        play(t, kRed);
        auto t3 = play(t, kRed);
        const TotemFiring* g = fired(t3, TotemId::Fire);
        check(f && g && f->effect.cells_to_clear == g->effect.cells_to_clear, "TOT-10 same seed same targets");
        // Spent: a further pair fires nothing (tier 1, one use).
        play(s, kRed);
        auto r6 = play(s, kRed);
        check(r6 && r6->totems_fired.empty(), "TOT-01 spent totem fires no more");
    }
    // TOT-03 third consecutive move charges to 1 again (tier 3, two uses).
    {
        LevelSession s = scene(kBlank, level_with_totems({"fire"}), with_totem(TotemId::Fire, 3));
        play(s, kRed);
        auto r2 = play(s, kRed);
        check(fired(r2, TotemId::Fire) != nullptr && s.totems().uses_left(TotemId::Fire) == 1, "tier 3 fires");
        play(s, kRed);
        check(s.totems().charge_count(TotemId::Fire) == 1, "TOT-03 third move charges 1");
    }
    // TOT-04 acceptance: red and orange in one swap, both fire, fire first.
    {
        Upgrades u = with_totem(TotemId::Fire, 1);
        LevelSession s = scene(kBlank, level_with_totems({"fire", "time"}), u);
        paint(s, kRedOrange);
        auto r1 = s.swap(kRedOrangeSwap[0], kRedOrangeSwap[1]);
        check(r1 && s.totems().charge_count(TotemId::Fire) == 1 && s.totems().charge_count(TotemId::Time) == 1,
              "TOT-04 both charged");
        paint(s, kRedOrange);
        auto r2 = s.swap(kRedOrangeSwap[0], kRedOrangeSwap[1]);
        check(r2 && r2->totems_fired.size() == 2 && r2->totems_fired[0].effect.totem == TotemId::Fire &&
                  r2->totems_fired[1].effect.totem == TotemId::Time,
              "TOT-04 both fire, fire first (RES-07 kind order)");
    }
    // TOT-06 acceptance and TOT-11 time.
    for (int tier = 1; tier <= 4; ++tier) {
        LevelSession s = scene(kBlank, level_with_totems({"time"}), with_totem(TotemId::Time, tier));
        play(s, kOrange);
        const double before = s.time_left();
        auto r = play(s, kOrange);
        const TotemFiring* f = fired(r, TotemId::Time);
        const double expect[] = {0, 60, 120, 180, 240};
        const double shuffle = r && r->auto_shuffle ? 5.0 : 0.0;
        check(f && std::abs(s.time_left() - (before + expect[tier] - shuffle)) < 1e-9, "TOT-11 time seconds by tier");
        if (tier == 1) {
            play(s, kOrange);
            auto r4 = play(s, kOrange);
            check(r4 && r4->totems_fired.empty(), "TOT-06 time tier 1 used once: nothing more");
        }
    }
    // TOT-07: illegal swaps, shuffles and hints change no charge.
    {
        LevelSession s = scene(kBlank, level_with_totems({"fire"}), with_totem(TotemId::Fire, 1));
        play(s, kRed);
        s.swap({0, 0}, {1, 0});
        s.shuffle();
        s.set_score(500);
        s.buy_hint();
        check(s.totems().charge_count(TotemId::Fire) == 1, "TOT-07 no charge change");
    }
    // TOT-12 gold.
    for (int tier = 1; tier <= 4; ++tier) {
        Upgrades u = with_totem(TotemId::Gold, tier);
        u.frenzy_tier = 1;
        LevelSession s = scene(kBlank, level_with_totems({"gold"}), u);
        play(s, kYellow);
        auto r = play(s, kYellow);
        const TotemFiring* f = fired(r, TotemId::Gold);
        const int first[] = {0, 1500, 2000, 2000, 2500};
        const int second[] = {0, 0, 0, 2500, 3000};
        check(f && f->points == first[tier], "TOT-12 gold points use 1");
        check(f && f->frenzy_started == (tier >= 2) && s.frenzy_on() == (tier >= 2 || r->frenzy_started),
              "TOT-12 gold tier 2+ starts Frenzy");
        if (tier >= 3) {
            s.tick(3);
            play(s, kYellow);
            auto r2 = play(s, kYellow);
            const TotemFiring* g = fired(r2, TotemId::Gold);
            check(g && g->effect.use == 2, "gold second use");
            check(g && g->points == second[tier] * 2, "TOT-12 gold use 2, doubled during Frenzy");
            check(g && g->frenzy_restarted && s.frenzy_left() == 15, "FRZ-03 gold restarts Frenzy");
        }
    }
    // TOT-13 harvest acceptance (tier 2) and chains (tier 3).
    for (int tier = 2; tier <= 3; ++tier) {
        LevelSession s = scene(kBlank, level_with_totems({"harvest"}), with_totem(TotemId::Harvest, tier));
        play(s, kGreen);
        paint(s, kind_rows(kGreen));
        for (Cell c : {Cell{5, 0}, Cell{6, 1}, Cell{7, 2}, Cell{5, 3}, Cell{6, 4}}) s.set_gem(c, true);
        s.set_chain({7, 0}, true);
        const int before = s.gems_collected();
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        const TotemFiring* f = fired(r, TotemId::Harvest);
        check(f && f->gems_harvested.size() == 5 && s.gems_collected() == before + 5, "TOT-13 collects every gem");
        check(f && f->points == 5 * 50, "harvest gems score 50 each, no piece points");
        const std::size_t expect = tier == 2 ? 8 : 9;
        check(f && f->gems_placed.size() == expect && s.gems_on_board() >= static_cast<int>(expect),
              "TOT-13 new gems ignore gems_max");
        check(f && f->steps.empty(), "harvest clears no piece");
        check(s.chained({7, 0}) == (tier < 3), "TOT-13 tier 3 removes chains");
    }
    // TOT-14 ward.
    for (int tier = 1; tier <= 3; ++tier) {
        LevelData l = level_with_totems({"ward"});
        LevelSession s = scene(kBlank, l, with_totem(TotemId::Ward, tier));
        play(s, kBlue);
        paint(s, kind_rows(kBlue));
        s.set_steel({5, 2}, true);
        s.set_steel({6, 4}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        const TotemFiring* f = fired(r, TotemId::Ward);
        check(f && !f->steps.empty() && has_cell(f->steps[0].steel_removed, {5, 2}) &&
                  has_cell(f->steps[0].steel_removed, {6, 4}),
              "TOT-14 ward destroys every steel");
        check(f && f->steps[0].points >= 20, "OBS-03 destroyed steel scores");
        check(f && f->steps[0].cleared.size() == (tier == 3 ? 10u : 2u), "TOT-14 tier 3 clears neighbours");
        check(s.steel_stopped() == (tier >= 2), "TOT-14 tier 2+ stops steel");
    }
    {
        LevelData l = level_with_totems({"ward"});
        l.steel_rate = 1.0;
        LevelSession s = scene(kBlank, l, with_totem(TotemId::Ward, 2));
        play(s, kBlue);
        paint(s, kind_rows(kBlue));
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(fired(r, TotemId::Ward) && s.steel_stopped(), "TOT-14 no steel on board still stops steel");
        paint(s, kind_rows(kBlue));
        auto r2 = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        bool any = false;
        for (const StepReport& st : r2->steps) any = any || !st.steel_spawned.empty();
        check(!any && s.steel_on_board() == 0, "OBS-02 no steel after ward tier 2");
    }
    // TOT-15 gift.
    for (int tier = 1; tier <= 4; ++tier) {
        LevelSession s = scene(kBlank, level_with_totems({"gift"}, {"tnt"}), with_totem(TotemId::Gift, tier));
        play(s, kPurple);
        auto r = play(s, kPurple);
        const TotemFiring* f = fired(r, TotemId::Gift);
        check(f && static_cast<int>(f->power_ups_placed.size()) == tier, "TOT-15 gift places n power-ups");
    }
    // TOT-16 paint.
    {
        LevelSession s = scene(kBlank, level_with_totems({"paint"}), with_totem(TotemId::Paint, 1));
        play(s, kWhite);
        paint(s, kind_rows(kWhite));
        s.set_steel({4, 1}, true);
        s.set_gem({7, 3}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        const TotemFiring* f = fired(r, TotemId::Paint);
        check(f && f->effect.cells_to_repaint.size() == 4 && !has_cell(f->effect.cells_to_repaint, {4, 1}),
              "TOT-16 repaints 4 non-steel pieces");
        // The paint's own runs resolve as steps (or there were none).
        check(f && s.runs().empty(), "TOT-16 runs resolve");
        // One kind on the board: nothing repainted, the use is still spent.
        TotemSystem ts({{TotemId::Paint, 1}});
        Board one(3, 3, 3, 1);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) one.set(x, y, 0);
        Rng rng(1);
        auto e = ts.make_effect(TotemId::Paint, one, rng);
        check(e.cells_to_repaint.empty() && e.repaint_kind == -1, "TOT-16 one kind repaints nothing");
    }
    // Gems, power-ups and chains stay on repainted pieces; the replay agrees.
    {
        LevelSession s = scene(kBlank, level_with_totems({"paint"}, {"tnt"}), with_totem(TotemId::Paint, 4));
        play(s, kWhite);
        paint(s, kind_rows(kWhite));
        for (int y = 0; y < 5; ++y) s.set_gem({y, y}, true);
        for (int y = 0; y < 4; ++y) s.set_chain({7, y}, true);
        Model m = model_of(s);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(fired(r, TotemId::Paint) != nullptr, "paint fired");
        if (r && m.apply(*r)) check(model_matches(m, s), "replay through paint reproduces the state");
    }
    // A totem clear that wins still resolves, and later totems still fire.
    {
        LevelData l = level_with_totems({"fire", "time"});
        l.gem_goal = GemGoal{1, 1};
        // Seed 2: the charging move's refill collects no gem, so the goal is
        // still open when the totems fire.
        LevelSession s = scene(kBlank, l, {}, 2);
        paint(s, kRedOrange);
        s.swap(kRedOrangeSwap[0], kRedOrangeSwap[1]);
        check(s.outcome() == Outcome::Playing, "GOAL-02 setup: the charging move does not win");
        paint(s, kRedOrange);
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 8; ++x) s.set_gem({x, y}, true);  // fire will hit one
        auto r = s.swap(kRedOrangeSwap[0], kRedOrangeSwap[1]);
        check(r && r->totems_fired.size() == 2 && s.outcome() == Outcome::Won, "GOAL-02 totem clear wins, time still fires");
    }
}

// ---------------------------------------------------------------------------
// Reports and replay
// ---------------------------------------------------------------------------

void test_reports_and_replay() {
    LevelData l = level_with_totems({"fire", "time", "gold", "harvest", "ward", "gift", "paint"},
                                    {"tnt", "color_clear", "lightning", "clock", "multiplier", "hint"});
    l.steel_rate = 0.05;
    l.chained_cells = {{1, 1}, {4, 4}, {6, 2}};
    Upgrades u;
    u.frenzy_tier = 2;
    for (int& t : u.totem_tiers) t = 3;
    LevelSession s(l, Difficulty::Normal, u, 99);
    int replayed = 0, fired_moves = 0;
    for (int i = 0; i < 150 && s.outcome() == Outcome::Playing; ++i) {
        auto moves = s.legal_moves();
        check(!moves.empty(), "a move is available");
        if (moves.empty()) break;
        Model m = model_of(s);
        const Swap pick = moves[static_cast<std::size_t>(i * 7) % moves.size()];
        auto r = s.swap(pick[0], pick[1]);
        check(r.has_value(), "listed move accepted");
        if (!r) break;
        if (!r->totems_fired.empty()) ++fired_moves;
        if (m.apply(*r)) {
            ++replayed;
            check(model_matches(m, s), "replay reproduces board and marks, move " + std::to_string(i));
        }
        check(s.runs().empty(), "board still after a move");
        check(s.steel_on_board() <= kMaxSteelOnBoard || i == 0, "steel cap");
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                if (s.steel({x, y})) check(!s.gem({x, y}) && !s.power_ups().has({x, y}), "OBS-04 steel carries nothing");
        int sum = 0;
        for (const StepReport& st : r->steps) sum += st.points;
        for (const TotemFiring& f : r->totems_fired) {
            sum += f.points;
            for (const StepReport& st : f.steps) sum += st.points;
        }
        check(sum == r->base_points && r->points == r->base_points * r->multiplier, "move totals add up");
        s.tick(1.5);
    }
    check(replayed > 20 && fired_moves > 0, "replay exercised with totems");

    // Tick report.
    LevelSession t(base_level(), Difficulty::Normal, {}, 1);
    auto tr = t.tick(2);
    check(tr && tr->time_left == 268 && tr->frenzy_left == 0 && !tr->frenzy_ended && !tr->free_hint &&
              tr->outcome == Outcome::Playing && !tr->outcome_changed,
          "tick report");
}

// OBS-05: chains that hold. Any clear that reaches a chained piece breaks
// its chain and leaves the piece in place; the next clear takes it.
void test_chains_hold() {
    // A run of 3 through one chained piece: two clear, the third stays,
    // unscored, uncounted and with its gem.
    {
        LevelSession s = scene(kSimple);
        s.set_chain({0, 7}, true);
        s.set_gem({0, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].cleared.size() == 2 && !has_cell(r->steps[0].cleared, {0, 7}),
              "OBS-05 held piece not cleared");
        check(r && r->steps[0].points == 2 * kPointsPerPiece, "OBS-05 held piece scores nothing");
        check(r && r->steps[0].cleared_by_kind.at(kBlue) == 2, "OBS-05 held piece not counted (GOAL-06)");
        check(r && r->steps[0].gems_collected.empty() && s.gems_collected() == 0, "OBS-05 gem stays on the held piece");
        bool stayed = true;
        if (r)
            for (const Fall& f : r->steps[0].falls) stayed = stayed && f.from != Cell{0, 7};
        check(r && stayed, "OBS-05 held piece keeps its cell");
    }
    // Vertical run with the chained piece in the middle: the ends clear and
    // the unchained piece falls one row.
    {
        const std::vector<std::string> rows{"........", "........", "........", "........",
                                            "........", "3.......", "3.......", "53......"};
        LevelSession s = scene(rows);
        s.set_chain({0, 6}, true);
        auto r = s.swap({0, 7}, {1, 7});
        check(r && r->steps[0].cleared == std::vector<Cell>{{0, 5}, {0, 7}} &&
                  r->steps[0].chains_broken == std::vector<Cell>{{0, 6}},
              "OBS-05 vertical run holds its middle");
        check(r && !r->steps[0].falls.empty() && r->steps[0].falls[0] == Fall{{0, 6}, {0, 7}, 3} &&
                  !s.chained({0, 7}),
              "OBS-05 unchained piece falls into the gap");
    }
    // A run made only of chained pieces: the chains break in one step, the
    // now plain pieces match in the next.
    {
        const std::vector<std::string> rows{"........", "........", "........", "........",
                                            "........", "........", "..142...", "11225..."};
        LevelSession s = scene(rows);
        s.set_chain({0, 7}, true);
        s.set_chain({1, 7}, true);
        s.set_chain({2, 6}, true);
        auto r = s.swap({4, 6}, {4, 7});
        const std::vector<Cell> line{{0, 7}, {1, 7}, {2, 7}};
        check(r && r->steps.size() >= 3, "OBS-05 all-chained run takes two steps");
        if (r && r->steps.size() >= 3) {
            check(r->steps[1].chains_broken == line && r->steps[1].cleared.empty() && r->steps[1].points == 0,
                  "OBS-05 all-chained run only breaks chains");
            bool all = true;
            for (Cell c : line) all = all && has_cell(r->steps[2].cleared, c);
            check(all, "OBS-05 unchained pieces clear in the next step");
        }
    }
    // An all-chained run of 4 clears nothing, so it earns no power-up
    // (PWR-01); the same pieces earn when they clear in the next step.
    {
        const std::vector<std::string> rows{"........", "........", "........", "........",
                                            "........", "........", "..112...", "11225..."};
        LevelSession s = scene(rows, level_with_totems({}, {"tnt"}));
        for (Cell c : {Cell{0, 7}, Cell{1, 7}, Cell{2, 6}, Cell{3, 6}}) s.set_chain(c, true);
        auto r = s.swap({4, 6}, {4, 7});
        check(r && r->steps.size() >= 3 && r->steps[1].cleared.empty() && r->steps[1].power_ups_placed.empty() &&
                  r->steps[2].power_ups_placed.size() == 1,
              "OBS-05 an all-chained run of 4 earns once, when it clears");
    }
    // An effect that reaches a chained piece breaks the chain (tnt).
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"tnt"}));
        s.set_power_up({1, 7}, PowerUp{PowerUpId::Tnt, 1});
        s.set_chain({0, 6}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        check(r && r->steps[0].power_ups_triggered.size() == 1 && has_cell(r->steps[0].chains_broken, {0, 6}) &&
                  !has_cell(r->steps[0].cleared, {0, 6}),
              "OBS-05 tnt breaks a chain, the piece stays");
    }
    // A power-up on a chained piece does not go off when the chain breaks;
    // it stays with the piece and goes off when the piece is cleared.
    {
        LevelSession s = scene(kSimple, level_with_totems({}, {"tnt"}));
        s.set_power_up({0, 7}, PowerUp{PowerUpId::Tnt, 1});
        s.set_chain({0, 7}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        // The scene has no move left, so the board reshuffles: count it.
        check(r && r->steps[0].power_ups_triggered.empty() && s.power_ups().count() == 1 && !s.chained({0, 7}),
              "OBS-05 power-up stays on the held piece");
    }
    // A totem's clear (fire) holds chained targets too.
    {
        LevelSession s = scene(kBlank, level_with_totems({"fire"}), with_totem(TotemId::Fire, 1));
        play(s, kRed);
        // The next red match fires: chain everything above the scene's rows.
        paint(s, kind_rows(kRed));
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 8; ++x) s.set_chain({x, y}, true);
        auto r = s.swap(kSimpleSwap[0], kSimpleSwap[1]);
        const TotemFiring* f = fired(r, TotemId::Fire);
        check(f && !f->steps.empty(), "OBS-05 fire fired");
        if (f && !f->steps.empty()) {
            const StepReport& st = f->steps[0];
            bool disjoint = true;
            for (Cell c : st.chains_broken) disjoint = disjoint && !has_cell(st.cleared, c);
            check(!st.chains_broken.empty() && disjoint &&
                      st.cleared.size() + st.chains_broken.size() == f->effect.cells_to_clear.size(),
                  "OBS-05 fire breaks chains and leaves the pieces");
        }
    }
    // Same seed and moves, same game, with chains held.
    {
        LevelData l = base_level();
        for (int y = 4; y < 8; ++y)
            for (int x = 0; x < 8; x += 2) l.chained_cells.push_back({x, y});
        LevelSession a(l, Difficulty::Normal, {}, 11);
        LevelSession b(l, Difficulty::Normal, {}, 11);
        int broken = 0;
        for (int i = 0; i < 40 && a.outcome() == Outcome::Playing; ++i) {
            auto moves = a.legal_moves();
            if (moves.empty()) break;
            const Swap m = moves[static_cast<std::size_t>(i * 5) % moves.size()];
            auto ra = a.swap(m[0], m[1]);
            auto rb = b.swap(m[0], m[1]);
            for (const StepReport& st : ra->steps) broken += static_cast<int>(st.chains_broken.size());
            check(ra && rb && snap(a) == snap(b), "determinism: chains held the same way");
        }
        check(broken > 0, "determinism run broke chains");
    }
}

}  // namespace

void test_det_failed_shuffle_leaves_no_hidden_state() {
    // A manual shuffle that finds nothing playable "changes nothing"
    // (level-session.md); the same moves afterwards must give the same game.
    LevelSession a = scene(kSimple, base_level(), {}, 5);
    LevelSession b = scene(kSimple, base_level(), {}, 5);
    for (LevelSession* s : {&a, &b})
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) s->set_chain({x, y}, !(y == 0 && x < 2));
    auto sh = b.shuffle();
    check(sh && sh->stuck && snap(a) == snap(b), "failed shuffle is reported stuck and leaves the board");
    for (LevelSession* s : {&a, &b})
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) s->set_chain({x, y}, false);
    auto ra = a.swap(kSimpleSwap[0], kSimpleSwap[1]);
    auto rb = b.swap(kSimpleSwap[0], kSimpleSwap[1]);
    check(ra && rb, "determinism: swap legal after failed shuffle");
    check(snap(a) == snap(b), "determinism: a failed shuffle does not change later refills");
}

void test_det_timer_independent_of_tick_size() {
    // 270 s of game time is 270 s whether it arrives in 27 s or 0.1 s ticks.
    LevelSession coarse(base_level(), Difficulty::Normal, {}, 1);
    LevelSession fine(base_level(), Difficulty::Normal, {}, 1);
    for (int i = 0; i < 10; ++i) coarse.tick(27.0);
    for (int i = 0; i < 2700; ++i) fine.tick(0.1);
    check(coarse.outcome() == Outcome::Lost, "GOAL-04 270 s in 27 s ticks loses");
    check(fine.outcome() == coarse.outcome(), "determinism: 270 s in 0.1 s ticks gives the same outcome");
    // SHUF-05: the free hint after 8 s idle, whatever the tick size.
    LevelData l = base_level();
    l.id = "e1-l01";
    l.index = 1;
    LevelSession h(l, Difficulty::Normal, {}, 1);
    int hints = 0;
    for (int i = 0; i < 80; ++i) {
        auto t = h.tick(0.1);
        if (t && t->free_hint) ++hints;
    }
    check(hints == 1, "determinism: free hint after 8 s of 0.1 s ticks");
}

void test_det_streams_do_not_alias() {
    // derive_seed must give independent streams: no other seed's stream 1
    // may equal this seed's stream 2 (bots derive stream 3 the same way).
    constexpr std::uint64_t g = 0x9E3779B97F4A7C15ull;
    const std::uint64_t a = 12345;
    const std::uint64_t b = a ^ (1 * g) ^ (2 * g);
    check(derive_seed(a, 2) != derive_seed(b, 1), "determinism: level stream of a is not board stream of b");
    Rng level_a(derive_seed(a, 2));
    Rng board_b(derive_seed(b, 1));
    check(level_a.next() != board_b.next(), "determinism: streams of different seeds are independent");
}

// BOARD-04 / OBS-05: chained_cells come from the level data. A cell off the
// board is invalid input and must throw std::invalid_argument, not alias
// another cell or write outside the mark vectors.
void test_chained_cells_out_of_bounds() {
    // A chained cell outside the board is rejected, not written out of bounds.
    {
        LevelData l = base_level();
        l.chained_cells = {Cell{0, 8}};
        check(throws<std::invalid_argument>([&] { LevelSession s(l, Difficulty::Normal, {}, 1); }),
              "OBS-05 chained cell outside the board throws");
    }
    LevelData right = base_level();
    right.chained_cells = {{9, 0}};  // x past width: today it silently chains (1, 1)
    check(throws<std::invalid_argument>([&] { LevelSession s(right, Difficulty::Normal, {}, 1); }),
          "OBS-05 chained cell right of the board throws");
    LevelData neg = base_level();
    neg.chained_cells = {{-1, 0}};
    check(throws<std::invalid_argument>([&] { LevelSession s(neg, Difficulty::Normal, {}, 1); }),
          "OBS-05 chained cell left of the board throws");
    LevelData below = base_level();
    below.chained_cells = {{3, 8}};  // y == height: today a heap overflow
    check(throws<std::invalid_argument>([&] { LevelSession s(below, Difficulty::Normal, {}, 1); }),
          "OBS-05 chained cell below the board throws");
}

void test_hint_not_charged_twice() {
    // SHUF-04 a second purchase while the hint is still shown costs nothing more.
    {
        LevelSession s = scene(kSimple);
        s.set_score(900);
        auto h1 = s.buy_hint();
        auto h2 = s.buy_hint();
        check(h1 && h2 && *h1 == *h2 && s.score() == 600, "SHUF-04 a hint already shown is not charged again");
    }
}

void test_setup_bounds_and_shuffle_idle() {
    // Scenario setters reject cells off the board instead of writing past it.
    {
        LevelSession s(base_level(), Difficulty::Normal, {}, 1);
        for (Cell c : {Cell{-1, 0}, Cell{0, -1}, Cell{8, 0}, Cell{0, 8}}) {
            check(throws<std::out_of_range>([&] { s.set_steel(c, true); }), "set_steel off the board throws");
            check(throws<std::out_of_range>([&] { s.set_gem(c, true); }), "set_gem off the board throws");
            check(throws<std::out_of_range>([&] { s.set_chain(c, true); }), "set_chain off the board throws");
        }
        s.set_steel({7, 7}, true);
        s.set_gem({0, 0}, true);
        s.set_chain({3, 4}, true);
        check(s.steel({7, 7}) && s.gem({0, 0}) && s.chained({3, 4}), "setters still work on the board");
    }
    // SHUF-05 a manual shuffle is player activity: it restarts the idle
    // timer and lets the free hint show again.
    {
        LevelData l = base_level();
        l.id = "e1-l01";
        l.index = 1;
        LevelSession s(l, Difficulty::Normal, {}, 1);
        int hints = 0;
        for (int i = 0; i < 7; ++i) {
            auto t = s.tick(1.0);
            if (t && t->free_hint) ++hints;
        }
        check(s.shuffle().has_value(), "manual shuffle works");
        for (int i = 0; i < 7; ++i) {
            auto t = s.tick(1.0);
            if (t && t->free_hint) ++hints;
        }
        check(hints == 0, "SHUF-05 a manual shuffle restarts the idle timer");
        auto t = s.tick(1.0);
        check(t && t->free_hint, "SHUF-05 free hint 8 s after the shuffle");
        check(s.shuffle().has_value(), "second manual shuffle works");
        int again = 0;
        for (int i = 0; i < 8; ++i) {
            auto u = s.tick(1.0);
            if (u && u->free_hint) ++again;
        }
        check(again == 1, "SHUF-05 free hint shows again after a shuffle");
    }
}

void test_report_steps_line_up_with_results() {
    // The game scene animates a move as the board result's cascades, then
    // each fired totem's: every step report must sit on one of those steps.
    LevelData l = base_level();
    l.gem_goal = GemGoal{1000, 1000};
    l.totems_unlocked = {"fire", "time", "gold", "harvest", "ward", "gift", "paint"};
    l.powerups_unlocked = {"tnt", "color_clear", "lightning", "clock", "multiplier", "hint"};
    l.steel_rate = 0.05;
    int moves = 0;
    int fired = 0;
    bool aligned = true;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        LevelSession s(l, Difficulty::Normal, {}, seed);
        for (int i = 0; i < 60 && s.outcome() == Outcome::Playing; ++i) {
            const auto m = s.legal_moves();
            if (m.empty()) break;
            const auto r = s.swap(m[static_cast<std::size_t>(i) % m.size()][0], m[static_cast<std::size_t>(i) % m.size()][1]);
            if (!r) break;
            ++moves;
            aligned = aligned && r->steps.size() == r->result.cascades.size();
            for (const TotemFiring& f : r->totems_fired) {
                ++fired;
                aligned = aligned && f.steps.size() == (f.result ? f.result->cascades.size() : 0u);
            }
        }
    }
    check(moves > 100 && fired > 0, "report alignment covers moves and totem firings");
    check(aligned, "step reports line up with the board result's and each totem's cascades");
}

int main() {
    test_construction();
    test_moves();
    test_resolution();
    test_gems();
    test_goal_and_timer();
    test_scoring();
    test_frenzy();
    test_obstacles();
    test_shuffles();
    test_power_ups();
    test_totems();
    test_reports_and_replay();
    test_chains_hold();
    test_det_failed_shuffle_leaves_no_hidden_state();
    test_det_timer_independent_of_tick_size();
    test_det_streams_do_not_alias();
    test_hint_not_charged_twice();
    test_chained_cells_out_of_bounds();
    test_setup_bounds_and_shuffle_idle();
    test_report_steps_line_up_with_results();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
