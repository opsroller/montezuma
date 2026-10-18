// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Balance simulator tests (WP-22, docs/reference/balance.md). Run via ctest.
#include <cstdio>
#include <string>
#include <vector>

#include "sim.hpp"

using namespace montezuma;
using namespace montezuma::sim;

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

LevelData level(int episode, int index) {
    LevelData l;
    l.episode = episode;
    l.index = index;
    char id[32];
    std::snprintf(id, sizeof id, "e%d-l%02d", episode, index);
    l.id = id;
    l.gem_goal = GemGoal{10, 15};
    l.seed = 5;
    return l;
}

Row row(int episode, int index, Bot bot, Difficulty d, int seeds, int wins, std::vector<double> times = {},
        long long total_moves = 0) {
    Row r;
    r.level = level(episode, index).id;
    r.episode = episode;
    r.index = index;
    r.bot = bot;
    r.difficulty = d;
    r.seeds = seeds;
    r.wins = wins;
    r.win_times = std::move(times);
    r.total_moves = total_moves;
    return r;
}

void test_names_and_seeds() {
    check(bot_from_name("random") == Bot::Random && bot_from_name("greedy") == Bot::Greedy &&
              bot_from_name("planner") == Bot::Planner && !bot_from_name("smart"),
          "bot names");
    check(run_seed(5, 0) == run_seed(5, 0) && run_seed(5, 0) != run_seed(5, 1) && run_seed(5, 1) != run_seed(6, 1),
          "run seed from (level seed, i) only");
}

void test_upgrades() {
    check(campaign_position(1, 1) == 1 && campaign_position(2, 1) == 6 && campaign_position(3, 4) == 15 &&
              campaign_position(5, 12) == 41 && campaign_position(9, 1) == 41,
          "campaign positions");
    LevelData l1 = level(1, 1);
    l1.powerups_unlocked = {"tnt"};
    Upgrades a = auto_upgrades(l1);
    check(a.timer_tier == 0 && a.gems_tier == 0 && a.frenzy_tier == 0 &&
              a.power_up_tiers[power_up_index(PowerUpId::Tnt)] == 1,
          "auto e1-l01");
    LevelSession s1(l1, Difficulty::Normal, a, 1);
    check(s1.time_left() == 270 && s1.gems_max() == 5 && s1.frenzy_tier() == 0, "auto e1-l01 in a session");
    Upgrades b = auto_upgrades(level(3, 4));
    check(b.timer_tier == 2 && b.gems_tier == 1 && b.frenzy_tier == 2 &&
              b.totem_tiers[static_cast<std::size_t>(TotemId::Fire)] == 2 &&
              b.totem_tiers[static_cast<std::size_t>(TotemId::Time)] == 2,
          "auto e3-l04: 330 s, 6 gems, Frenzy 2, fire 2, time 2");
    Upgrades last = auto_upgrades(level(7, 1));
    check(last.timer_tier == 4 && last.gems_tier == 3 && last.frenzy_tier == 4 &&
              last.totem_tiers[static_cast<std::size_t>(TotemId::Ward)] == 3,
          "past the campaign: every tier");
    Upgrades n3 = no_upgrades(level(1, 3));
    Upgrades n4 = no_upgrades(level(1, 4));
    check(n3.timer_tier == 0 && n3.gems_tier == 0 && n3.frenzy_tier == 0 && n4.frenzy_tier == 1,
          "none: base, Frenzy tier 1 from e1-l04");
    check(n4.totem_tiers == std::array<int, kTotemCount>{} && n4.power_up_tiers == PowerUpTiers{},
          "none: listed ids at tier 1 (session default)");
}

void test_targets() {
    // Median.
    check(!row(1, 2, Bot::Greedy, Difficulty::Normal, 4, 0).median_time_left(), "no wins: no median");
    check(row(1, 2, Bot::Greedy, Difficulty::Normal, 4, 3, {10, 30, 20}).median_time_left() == 20.0, "odd median");
    check(row(1, 2, Bot::Greedy, Difficulty::Normal, 4, 4, {10, 40, 20, 30}).median_time_left() == 25.0,
          "even median is the mean of the middle two");
    // e1-l01.
    check(evaluate(row(1, 1, Bot::Greedy, Difficulty::Normal, 200, 200, std::vector<double>(200, 150.0))) ==
              Verdict::Pass,
          "e1-l01 greedy 100% 150 s pass");
    check(evaluate(row(1, 1, Bot::Greedy, Difficulty::Normal, 200, 199, std::vector<double>(199, 200.0))) ==
              Verdict::Fail,
          "e1-l01 greedy 99.5% fails");
    check(evaluate(row(1, 1, Bot::Greedy, Difficulty::Normal, 200, 200, std::vector<double>(200, 149.0))) ==
              Verdict::Fail,
          "e1-l01 median below 150 fails");
    check(evaluate(row(1, 1, Bot::Random, Difficulty::Normal, 200, 190)) == Verdict::Pass, "e1-l01 random 95%");
    check(evaluate(row(1, 1, Bot::Random, Difficulty::Normal, 200, 189)) == Verdict::Fail, "e1-l01 random 94.5%");
    check(evaluate(row(1, 1, Bot::Planner, Difficulty::Hard, 200, 198)) == Verdict::Pass, "e1-l01 planner hard 99%");
    check(evaluate(row(1, 1, Bot::Planner, Difficulty::Hard, 200, 197)) == Verdict::Fail, "planner 98.5% fails");
    // Rows with no target.
    check(evaluate(row(1, 1, Bot::Greedy, Difficulty::Hard, 200, 0)) == Verdict::None, "greedy hard no target");
    check(evaluate(row(1, 1, Bot::Random, Difficulty::Hard, 200, 0)) == Verdict::None, "random hard no target");
    check(evaluate(row(1, 1, Bot::Planner, Difficulty::Normal, 200, 0)) == Verdict::None, "planner normal no target");
    check(evaluate(row(9, 1, Bot::Greedy, Difficulty::Normal, 200, 0)) == Verdict::None, "e9 has no target");
    check(target_text(row(9, 1, Bot::Greedy, Difficulty::Normal, 200, 0)) == "-", "no target text");
    // Episode rows.
    struct Case {
        int episode, index, greedy;
        double median;
        int random;
        bool at_most;
        int planner;
        int moves;  // greedy mean moves floor, 0 for none
    };
    const Case cases[] = {{1, 2, 98, 100, 70, false, 90, 0},  {2, 3, 95, 70, 40, false, 80, 50},
                          {3, 5, 90, 50, 20, false, 70, 70},  {4, 2, 85, 35, 20, true, 60, 90},
                          {5, 9, 80, 25, 10, true, 50, 110}};
    for (const Case& c : cases) {
        const std::string tag = "e" + std::to_string(c.episode);
        const int seeds = 200;
        const int need = c.greedy * seeds / 100;
        const long long at_floor = static_cast<long long>(c.moves) * seeds;
        const auto greedy = [&](int wins, double median, long long moves) {
            return evaluate(row(c.episode, c.index, Bot::Greedy, Difficulty::Normal, seeds, wins,
                                std::vector<double>(static_cast<std::size_t>(wins), median), moves));
        };
        check(greedy(need, c.median, at_floor) == Verdict::Pass, tag + " greedy at threshold passes");
        check(greedy(need - 1, c.median, at_floor) == Verdict::Fail, tag + " greedy below fails");
        check(greedy(need, c.median - 0.5, at_floor) == Verdict::Fail, tag + " greedy median below fails");
        if (c.moves > 0) {
            check(greedy(need, c.median, at_floor - 1) == Verdict::Fail, tag + " greedy one move under the floor fails");
            check(greedy(need - 1, c.median, at_floor * 2) == Verdict::Fail,
                  tag + " greedy over the floor but below the win rate fails");
        } else {
            check(greedy(need, c.median, seeds) == Verdict::Pass, tag + " has no moves floor (1 move a run passes)");
        }
        const int r = c.random * seeds / 100;
        check(evaluate(row(c.episode, c.index, Bot::Random, Difficulty::Normal, seeds, r)) == Verdict::Pass,
              tag + " random at threshold passes");
        check(evaluate(row(c.episode, c.index, Bot::Random, Difficulty::Normal, seeds, c.at_most ? r + 1 : r - 1)) ==
                  Verdict::Fail,
              tag + " random past threshold fails");
        const int p = c.planner * seeds / 100;
        check(evaluate(row(c.episode, c.index, Bot::Planner, Difficulty::Hard, seeds, p)) == Verdict::Pass,
              tag + " planner hard at threshold");
        check(evaluate(row(c.episode, c.index, Bot::Planner, Difficulty::Hard, seeds, p - 1)) == Verdict::Fail,
              tag + " planner hard below");
    }
    // Moves floor (balance.md "Too easy"): greedy/normal only, mean over all runs.
    check(moves_floor(1, 1) == 0 && moves_floor(1, 5) == 0 && moves_floor(2, 1) == 50 && moves_floor(3, 1) == 70 &&
              moves_floor(4, 1) == 90 && moves_floor(5, 12) == 110 && moves_floor(9, 1) == 0,
          "moves floor per episode");
    check(evaluate(row(1, 1, Bot::Greedy, Difficulty::Normal, 200, 200, std::vector<double>(200, 150.0), 200)) ==
              Verdict::Pass,
          "e1-l01 has no moves floor");
    check(evaluate(row(2, 1, Bot::Random, Difficulty::Normal, 200, 80, {}, 0)) == Verdict::Pass,
          "e2 random ignores the floor");
    check(evaluate(row(2, 1, Bot::Planner, Difficulty::Hard, 200, 160, {}, 0)) == Verdict::Pass,
          "e2 planner hard ignores the floor");
    check(evaluate(row(2, 1, Bot::Greedy, Difficulty::Hard, 200, 200, {}, 0)) == Verdict::None, "e2 greedy hard no target");
    check(evaluate(row(2, 1, Bot::Greedy, Difficulty::Normal, 3, 3, {80, 80, 80}, 149)) == Verdict::Fail,
          "e2 mean 49.67 moves fails");
    check(evaluate(row(2, 1, Bot::Greedy, Difficulty::Normal, 3, 3, {80, 80, 80}, 150)) == Verdict::Pass,
          "e2 mean 50 moves passes");
    check(target_text(row(2, 1, Bot::Greedy, Difficulty::Normal, 200, 0)) == "win>=95% median>=70.0s moves>=50",
          "e2 greedy target text names the floor");
    check(target_text(row(5, 1, Bot::Greedy, Difficulty::Normal, 200, 0)) == "win>=80% median>=25.0s moves>=110",
          "e5 greedy target text names the floor");
    check(target_text(row(1, 2, Bot::Greedy, Difficulty::Normal, 200, 0)) == "win>=98% median>=100.0s",
          "e1 greedy target text has no floor");
    check(target_text(row(2, 1, Bot::Random, Difficulty::Normal, 200, 0)) == "win>=40%", "e2 random text unchanged");
    // Exact on counts: 3 seeds, 98 % needs all 3 (294 >= 294).
    check(evaluate(row(1, 2, Bot::Random, Difficulty::Normal, 3, 3)) == Verdict::Pass, "exact counts");
    check(evaluate(row(2, 2, Bot::Random, Difficulty::Normal, 5, 2)) == Verdict::Pass, "40 % of 5 is 2");
}

void test_curves() {
    LevelData a = level(2, 2);
    LevelData b = level(2, 3);
    LevelData idea = level(2, 3);
    idea.totems_unlocked = {"gift"};
    check(!introduces_idea(b, {&a}) && introduces_idea(idea, {&a}), "new idea: a new totem id");
    LevelData chains = level(2, 3);
    chains.chained_cells = {{1, 1}};
    check(introduces_idea(chains, {&a}), "new idea: first chains");
    LevelData steel = level(2, 3);
    steel.steel_rate = 0.02;
    check(introduces_idea(steel, {&a}), "new idea: first steel");
    check(introduces_idea(level(1, 4), {}), "new idea: Frenzy at e1-l04");
    // Chains and steel are new only on their first level in campaign order:
    // chains (e3-l01), then steel alone (e3-l06), then chains again (e3-l07)
    // is not a new idea, and neither is steel again after a steel-free level.
    LevelData e31 = level(3, 1), e36 = level(3, 6), e37 = level(3, 7), e38 = level(3, 8);
    e31.chained_cells = {{0, 7}};
    e36.steel_rate = 0.02;
    e37.chained_cells = {{1, 7}};
    e38.steel_rate = 0.03;
    check(!introduces_idea(e37, {&e31, &e36}), "chains after a chain-free level are not new when seen before");
    check(!introduces_idea(e38, {&e36, &e37}), "steel after a steel-free level is not new when seen before");
    check(introduces_idea(e37, {&e36}), "chains with no earlier chains are new");
    {
        std::vector<Row> rows{row(3, 6, Bot::Greedy, Difficulty::Normal, 100, 100),
                              row(3, 7, Bot::Greedy, Difficulty::Normal, 100, 95)};
        bool seen = false;
        for (const CurveCheck& c : curve_checks({e31, e36, e37}, rows))
            if (c.rule == "curve-1" && c.levels == "e3-l06>e3-l07") {
                seen = true;
                check(c.verdict == Verdict::Pass && c.detail.find("new idea") == std::string::npos,
                      "curve-1 treats e3-l07's chains as practice, not a new idea");
            }
        check(seen, "curve-1 e3-l06>e3-l07 reported");
    }

    const auto run = [&](const LevelData& second, int w1, int w2) {
        std::vector<Row> rows{row(2, 2, Bot::Greedy, Difficulty::Normal, 100, w1),
                              row(2, 3, Bot::Greedy, Difficulty::Normal, 100, w2)};
        return curve_checks({a, second}, rows);
    };
    check(run(b, 80, 90)[0].verdict == Verdict::Pass, "curve-1 +10 passes");
    check(run(b, 80, 91)[0].verdict == Verdict::Fail, "curve-1 +11 fails");
    check(run(b, 80, 50)[0].verdict == Verdict::Pass, "curve-1 harder next level passes");
    check(run(idea, 80, 81)[0].verdict == Verdict::Pass, "curve-1 idea level easier passes");
    check(run(idea, 80, 80)[0].verdict == Verdict::Fail, "curve-1 idea level not easier fails");
    check(run(idea, 100, 100)[0].verdict == Verdict::Pass, "curve-1 idea level after 100% at 100% passes");
    check(run(idea, 100, 99)[0].verdict == Verdict::Fail, "curve-1 idea level below 100% after 100% fails");
    // Missing next level or rows: skipped.
    auto lone = curve_checks({a}, {row(2, 2, Bot::Greedy, Difficulty::Normal, 100, 80)});
    check(!lone.empty() && lone[0].verdict == Verdict::Skipped, "curve-1 missing level skipped");
    auto no_rows = curve_checks({a, b}, {row(2, 2, Bot::Random, Difficulty::Normal, 100, 80)});
    check(no_rows[0].verdict == Verdict::Skipped, "curve-1 without greedy/normal skipped");
    // Curve rule 2.
    LevelData l1 = level(3, 1), l3 = level(3, 3);
    auto c2 = curve_checks({l1, l3}, {row(3, 1, Bot::Greedy, Difficulty::Normal, 100, 90),
                                      row(3, 3, Bot::Greedy, Difficulty::Normal, 100, 90)});
    bool found = false;
    for (const CurveCheck& c : c2)
        if (c.rule == "curve-2") {
            found = true;
            check(c.verdict == Verdict::Pass, "curve-2 equal passes");
        }
    check(found, "curve-2 reported");
    auto c2f = curve_checks({l1, l3}, {row(3, 1, Bot::Greedy, Difficulty::Normal, 100, 89),
                                       row(3, 3, Bot::Greedy, Difficulty::Normal, 100, 90)});
    for (const CurveCheck& c : c2f)
        if (c.rule == "curve-2") check(c.verdict == Verdict::Fail, "curve-2 opener harder fails");
}

void test_clock_rule() {
    // balance.md clock rule: ceil(mean moves x 3.5 s x 1.15 / 10) x 10.
    check(required_clock(0, 200) == 0, "no moves need no clock");
    check(required_clock(49 * 200, 200) == 200, "49 moves: 197.2 s rounds up to 200");
    check(required_clock(60 * 200, 200) == 250, "60 moves: 241.5 s rounds up to 250");
    check(required_clock(200000, 4025) == 200, "exactly 200.0 s stays 200");
    check(required_clock(200001, 4025) == 210, "a hair over 200 s rounds up to 210");
    check(required_clock(1, 0) == 0, "no seeds need no clock");

    LevelData l = level(2, 2);
    const auto checks_for = [&](const LevelData& lv, long long normal_moves, long long hard_moves) {
        Row n = row(2, 2, Bot::Greedy, Difficulty::Normal, 100, 100);
        Row h = row(2, 2, Bot::Greedy, Difficulty::Hard, 100, 100);
        n.total_moves = normal_moves;
        h.total_moves = hard_moves;
        return clock_checks({lv}, {n, h, row(2, 2, Bot::Random, Difficulty::Normal, 100, 100)});
    };
    // 270 s default base: 67 moves need 270 s (269.7), 68 moves need 280 s.
    auto fit = checks_for(l, 6700, 6700);
    check(fit.size() == 2 && fit[0].rule == "clock" && fit[0].levels == "e2-l02 normal" &&
              fit[1].levels == "e2-l02 hard",
          "one clock check per difficulty");
    check(fit[0].verdict == Verdict::Pass && fit[1].verdict == Verdict::Pass, "270 s covers 67 moves");
    auto tight = checks_for(l, 6700, 6800);
    check(tight[0].verdict == Verdict::Pass && tight[1].verdict == Verdict::Fail, "270 s does not cover 68 moves");
    check(tight[1].detail.find("280") != std::string::npos && tight[1].detail.find("270") != std::string::npos,
          "the detail names the base clock and the required clock");
    l.base_clock = BaseClock{270, 280};
    check(checks_for(l, 6700, 6800)[1].verdict == Verdict::Pass, "a base clock of 280 covers 68 moves");
    // The rule assumes a player with no upgrades: tier 0, no timer bonus.
    l.base_clock = BaseClock{250, 250};
    check(checks_for(l, 6700, 6700)[0].verdict == Verdict::Fail, "the timer tier bonus does not count");
    // No greedy row for a difficulty: skipped.
    auto none = clock_checks({level(2, 2)}, {row(2, 2, Bot::Random, Difficulty::Normal, 100, 100)});
    check(none.size() == 2 && none[0].verdict == Verdict::Skipped && none[1].verdict == Verdict::Skipped,
          "clock check without greedy rows is skipped");
}

void test_bots_and_play() {
    LevelData l = level(1, 3);
    l.gem_goal = GemGoal{20, 30};
    l.totems_unlocked = {"fire"};
    l.powerups_unlocked = {"tnt"};
    const Upgrades up = auto_upgrades(l);
    LevelSession s(l, Difficulty::Normal, up, 3);
    const std::vector<Swap> moves = s.legal_moves();
    Rng r1(1), r2(1);
    check(choose_move(Bot::Random, s, moves, r1) == choose_move(Bot::Random, s, moves, r2), "random is seeded");
    const Swap g = choose_move(Bot::Greedy, s, moves, r1);
    int best = 0;
    for (const Swap& m : moves) best = std::max(best, greedy_score(s, m));
    check(greedy_score(s, g) == best, "greedy takes the top score");
    for (const Swap& m : moves) {
        if (greedy_score(s, m) == best) {
            check(m == g, "greedy tie goes to the first in order");
            break;
        }
    }
    // Planner prefers a swap that fires a totem.
    {
        LevelSession p = s;
        p.totems_for_setup();
        bool any_red = false;
        for (const Swap& m : moves)
            for (const Run& run : p.runs_for_swap(m[0], m[1])) any_red = any_red || run.kind == 0;
        // Charge fire once by playing a red swap if one exists.
        if (any_red) {
            for (const Swap& m : moves) {
                bool red = false;
                for (const Run& run : p.runs_for_swap(m[0], m[1])) red = red || run.kind == 0;
                if (red) {
                    p.swap(m[0], m[1]);
                    break;
                }
            }
        }
        if (p.outcome() == Outcome::Playing && p.totems().charge_count(TotemId::Fire) == 1) {
            const auto pm = p.legal_moves();
            const Swap pick = choose_move(Bot::Planner, p, pm, r1);
            bool exists = false;
            for (const Swap& m : pm) exists = exists || fires_totem(p, m);
            check(!exists || fires_totem(p, pick), "planner prefers firing a totem");
        }
    }
    // play(): deterministic, ends, records time at a win.
    for (Bot bot : {Bot::Random, Bot::Greedy, Bot::Planner}) {
        RunResult a = play(l, Difficulty::Normal, up, bot, run_seed(l.seed, 0));
        RunResult b = play(l, Difficulty::Normal, up, bot, run_seed(l.seed, 0));
        check(a.won == b.won && a.moves == b.moves && a.time_left == b.time_left, "play is deterministic");
        check(a.won ? a.time_left > 0 : a.time_left == 0, "time left at the win, 0 for a loss");
        check(a.moves <= 180, "at most 179-180 bot moves in 270 s");
    }
    // Format: no wins shows '-' / empty / null.
    std::vector<Row> rows{row(9, 1, Bot::Greedy, Difficulty::Normal, 2, 0)};
    const std::string t = format_report(rows, {}, Format::Table);
    const std::string c = format_report(rows, {}, Format::Csv);
    const std::string j = format_report(rows, {}, Format::Json);
    check(t.find("e9-l01") != std::string::npos, "table has the level");
    check(c.find("e9-l01,greedy,normal,2,0,0.0,,0.0,0,-,-") != std::string::npos, "csv empty median");
    check(j.find("\"median_time_left\": null") != std::string::npos, "json null median");
    // Format: an e2 greedy row under the moves floor (40 a run) fails in every format.
    std::vector<Row> e2{row(2, 1, Bot::Greedy, Difficulty::Normal, 2, 2, {90, 90}, 80)};
    const std::string t2 = format_report(e2, {}, Format::Table);
    const std::string c2 = format_report(e2, {}, Format::Csv);
    const std::string j2 = format_report(e2, {}, Format::Json);
    check(t2.find("win>=95% median>=70.0s moves>=50   FAIL") != std::string::npos, "table floor target and FAIL");
    check(c2.find("e2-l01,greedy,normal,2,2,100.0,90.0,40.0,0,win>=95% median>=70.0s moves>=50,FAIL") !=
              std::string::npos,
          "csv floor target and FAIL, same columns");
    check(j2.find("\"target\": \"win>=95% median>=70.0s moves>=50\", \"result\": \"FAIL\"") != std::string::npos,
          "json floor target and FAIL");
}

}  // namespace

int main() {
    test_names_and_seeds();
    test_upgrades();
    test_targets();
    test_curves();
    test_clock_rule();
    test_bots_and_play();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
