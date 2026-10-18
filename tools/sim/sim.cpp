// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "sim.hpp"

#include <algorithm>
#include <climits>
#include <array>
#include <cstdio>
#include <set>

namespace montezuma::sim {

namespace {

constexpr std::array<std::string_view, 3> kBotNames{"random", "greedy", "planner"};

// progression PROG-01: levels per episode.
constexpr std::array<int, 5> kEpisodeSizes{5, 6, 8, 10, 12};

// An unlock level as (episode, index).
struct At {
    int episode;
    int index;
};

// progression.md Upgrades: the level each tier becomes available.
constexpr std::array<At, 4> kTimerTiers{{{1, 2}, {3, 4}, {4, 6}, {5, 8}}};
constexpr std::array<At, 3> kGemsTiers{{{2, 5}, {3, 8}, {5, 2}}};
constexpr std::array<At, 4> kFrenzyTiers{{{1, 4}, {3, 3}, {4, 5}, {5, 7}}};
// progression.md Unlocks; {0, 0} marks a tier that does not exist.
constexpr std::array<std::array<At, 3>, kPowerUpCount> kPowerUpTiers{{
    {{{1, 1}, {3, 1}, {4, 8}}},   // tnt
    {{{1, 2}, {3, 6}, {4, 9}}},   // color_clear
    {{{1, 4}, {1, 5}, {4, 1}}},   // lightning
    {{{2, 1}, {4, 2}, {5, 4}}},   // clock
    {{{2, 2}, {3, 5}, {4, 10}}},  // multiplier
    {{{2, 6}, {4, 3}, {5, 3}}},   // hint
}};
constexpr std::array<std::array<At, 4>, kTotemCount> kTotemTiers{{
    {{{1, 3}, {3, 3}, {4, 4}, {5, 2}}},   // fire
    {{{1, 5}, {3, 2}, {4, 8}, {5, 5}}},   // time
    {{{2, 1}, {3, 6}, {4, 3}, {5, 1}}},   // gold
    {{{2, 4}, {3, 7}, {4, 10}, {5, 9}}},  // harvest
    {{{4, 1}, {4, 9}, {5, 7}, {0, 0}}},   // ward
    {{{2, 3}, {3, 5}, {4, 7}, {5, 9}}},   // gift
    {{{2, 5}, {3, 7}, {4, 6}, {5, 6}}},   // paint
}};
// Passive ideas a level brings with no id in its data (progression Episodes).
constexpr std::array<std::string_view, 1> kPassiveIdeaLevels{"e1-l04"};

template <std::size_t N>
int tiers_owned(const std::array<At, N>& tiers, int position) {
    int owned = 0;
    for (const At& at : tiers)
        if (at.episode > 0 && campaign_position(at.episode, at.index) <= position) ++owned;
    return owned;
}

// wins_a / seeds_a compared with wins_b / seeds_b, exactly.
long long cross(int wins_a, int seeds_a, int wins_b, int seeds_b) {
    return static_cast<long long>(wins_a) * seeds_b - static_cast<long long>(wins_b) * seeds_a;
}

std::string fmt1(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

const Row* find_row(const std::vector<Row>& rows, const std::string& level, Bot bot, Difficulty d) {
    for (const Row& r : rows)
        if (r.level == level && r.bot == bot && r.difficulty == d) return &r;
    return nullptr;
}

std::string json_string(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

}  // namespace

std::string_view bot_name(Bot bot) noexcept { return kBotNames[static_cast<std::size_t>(bot)]; }

std::optional<Bot> bot_from_name(std::string_view name) noexcept {
    for (std::size_t i = 0; i < kBotNames.size(); ++i)
        if (kBotNames[i] == name) return static_cast<Bot>(i);
    return std::nullopt;
}

std::string_view difficulty_name(Difficulty d) noexcept { return d == Difficulty::Hard ? "hard" : "normal"; }

std::string_view verdict_name(Verdict v) noexcept {
    switch (v) {
    case Verdict::Pass: return "PASS";
    case Verdict::Fail: return "FAIL";
    case Verdict::Skipped: return "skipped";
    case Verdict::None: break;
    }
    return "-";
}

// ---- Bots -------------------------------------------------------------------

int greedy_score(const LevelSession& s, const Swap& swap) {
    // AMBIGUOUS: the swap's own run cells only, gems counted 2.
    std::set<std::pair<int, int>> cells;
    for (const Run& run : s.runs_for_swap(swap[0], swap[1]))
        for (Cell c : run.cells) cells.insert({c.x, c.y});
    int score = 0;
    for (const auto& [x, y] : cells) {
        // The gem sits on the piece before the swap moves it.
        Cell from{x, y};
        if (from == swap[0]) from = swap[1];
        else if (from == swap[1]) from = swap[0];
        score += s.gem(from) ? 2 : 1;
    }
    return score;
}

bool fires_totem(const LevelSession& s, const Swap& swap) {
    const TotemSystem& t = s.totems();
    for (const Run& run : s.runs_for_swap(swap[0], swap[1])) {
        const auto id = totem_for_kind(run.kind);
        if (id && t.unlocked(*id) && t.uses_left(*id) > 0 && t.charge_count(*id) + 1 >= t.def(*id).charges_to_fire)
            return true;
    }
    return false;
}

Swap choose_move(Bot bot, const LevelSession& s, const std::vector<Swap>& moves, Rng& rng) {
    if (bot == Bot::Random) return moves[static_cast<std::size_t>(rng.below(static_cast<int>(moves.size())))];
    // AMBIGUOUS: planner classes: fires a totem (0), run of 4+ (1), rest (2).
    const auto klass = [&](const Swap& m) {
        if (bot != Bot::Planner) return 0;
        if (fires_totem(s, m)) return 0;
        for (const Run& run : s.runs_for_swap(m[0], m[1]))
            if (run.cells.size() >= 4) return 1;
        return 2;
    };
    std::size_t best = 0;
    int best_class = klass(moves[0]);
    int best_score = greedy_score(s, moves[0]);
    for (std::size_t i = 1; i < moves.size(); ++i) {
        const int c = klass(moves[i]);
        const int sc = greedy_score(s, moves[i]);
        if (c < best_class || (c == best_class && sc > best_score)) {
            best = i;
            best_class = c;
            best_score = sc;
        }
    }
    return moves[best];
}

// ---- Runs ---------------------------------------------------------------------

std::uint64_t run_seed(std::uint64_t level_seed, int i) noexcept {
    // AMBIGUOUS: splitmix64(level.seed XOR i).
    std::uint64_t z = (level_seed ^ static_cast<std::uint64_t>(i)) + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

RunResult play(const LevelData& level, Difficulty d, const Upgrades& up, Bot bot, std::uint64_t seed) {
    LevelSession s(level, d, up, seed);
    Rng rng(derive_seed(seed, kBotStream));
    RunResult out;
    while (s.outcome() == Outcome::Playing) {
        // AMBIGUOUS: tick before each move; a tick to 0 loses before it.
        s.tick(kBotMoveSeconds);
        if (s.outcome() != Outcome::Playing || s.time_left() <= 0.0) break;
        const std::vector<Swap> moves = s.legal_moves();
        if (moves.empty()) {
            out.stuck = true;
            break;
        }
        const Swap m = choose_move(bot, s, moves, rng);
        const std::optional<MoveReport> r = s.swap(m[0], m[1]);
        if (!r) {
            out.stuck = true;
            break;
        }
        ++out.moves;
        if (r->stuck && s.outcome() != Outcome::Won) out.stuck = true;
    }
    out.won = s.outcome() == Outcome::Won;
    out.time_left = out.won ? s.time_left() : 0.0;
    if (out.won) out.stuck = false;
    return out;
}

Row simulate(const LevelData& level, Difficulty d, const Upgrades& up, Bot bot, int seeds) {
    Row row;
    row.level = level.id;
    row.episode = level.episode;
    row.index = level.index;
    row.bot = bot;
    row.difficulty = d;
    row.seeds = seeds;
    for (int i = 0; i < seeds; ++i) {
        const RunResult r = play(level, d, up, bot, run_seed(level.seed, i));
        row.total_moves += r.moves;
        if (r.won) {
            ++row.wins;
            row.win_moves += r.moves;
            row.win_times.push_back(r.time_left);
        }
        if (r.stuck) ++row.stuck;
    }
    return row;
}

// ---- Upgrades -----------------------------------------------------------------

int campaign_position(int episode, int index) noexcept {
    if (episode < 1) return 1;
    if (episode > static_cast<int>(kEpisodeSizes.size())) return kCampaignLevels;
    int pos = 0;
    for (int e = 1; e < episode; ++e) pos += kEpisodeSizes[static_cast<std::size_t>(e - 1)];
    return pos + std::clamp(index, 1, kEpisodeSizes[static_cast<std::size_t>(episode - 1)]);
}

Upgrades auto_upgrades(const LevelData& level) {
    // AMBIGUOUS: a tier whose level is L is owned while playing L.
    const int pos = campaign_position(level.episode, level.index);
    Upgrades u;
    u.timer_tier = tiers_owned(kTimerTiers, pos);
    u.gems_tier = tiers_owned(kGemsTiers, pos);
    u.frenzy_tier = tiers_owned(kFrenzyTiers, pos);
    for (std::size_t i = 0; i < kPowerUpCount; ++i) u.power_up_tiers[i] = tiers_owned(kPowerUpTiers[i], pos);
    for (std::size_t i = 0; i < kTotemCount; ++i) u.totem_tiers[i] = tiers_owned(kTotemTiers[i], pos);
    return u;
}

Upgrades clock_upgrades(const LevelData& level) {
    Upgrades u = auto_upgrades(level);
    u.timer_tier = 0;
    return u;
}

Upgrades no_upgrades(const LevelData& level) {
    Upgrades u;
    // AMBIGUOUS: Frenzy tier 1 is free from its unlock level on.
    if (campaign_position(level.episode, level.index) >= campaign_position(kFrenzyTiers[0].episode, kFrenzyTiers[0].index))
        u.frenzy_tier = 1;
    return u;
}

// ---- Targets ------------------------------------------------------------------

std::optional<double> Row::median_time_left() const {
    if (win_times.empty()) return std::nullopt;
    std::vector<double> t = win_times;
    std::sort(t.begin(), t.end());
    const std::size_t n = t.size();
    return n % 2 == 1 ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) / 2.0;
}

std::optional<TargetRow> target_row(int episode, int index) {
    // balance.md Targets.
    static constexpr std::array<TargetRow, 6> kRows{{
        {"e1-l01", 100, 150.0, 95, 99, 0},
        {"e1", 98, 100.0, 70, 90, 0},
        {"e2", 95, 70.0, 40, 80, 50},
        {"e3", 90, 50.0, 20, 70, 70},
        {"e4", 85, 35.0, -1, 60, 90},
        {"e5", 80, 25.0, -1, 50, 110},
    }};
    if (episode == 1) return index == 1 ? kRows[0] : kRows[1];
    if (episode >= 2 && episode <= 5) return kRows[static_cast<std::size_t>(episode)];
    return std::nullopt;
}

int moves_floor(int episode, int index) {
    const auto t = target_row(episode, index);
    return t ? t->greedy_min_moves : 0;
}

std::string target_text(const Row& row) {
    const auto t = target_row(row.episode, row.index);
    if (!t) return "-";
    if (row.bot == Bot::Greedy && row.difficulty == Difficulty::Normal)
        return "win>=" + std::to_string(t->greedy_win_pct) + "% median>=" + fmt1(t->greedy_median) + "s" +
               (t->greedy_min_moves > 0 ? " moves>=" + std::to_string(t->greedy_min_moves) : std::string());
    if (row.bot == Bot::Random && row.difficulty == Difficulty::Normal && t->random_pct >= 0)
        return "win>=" + std::to_string(t->random_pct) + "%";
    if (row.bot == Bot::Planner && row.difficulty == Difficulty::Hard)
        return "win>=" + std::to_string(t->planner_hard_pct) + "%";
    return "-";
}

Verdict evaluate(const Row& row) {
    const auto t = target_row(row.episode, row.index);
    if (!t || row.seeds <= 0) return Verdict::None;
    // Exact: wins x 100 against pct x seeds.
    const long long w = static_cast<long long>(row.wins) * 100;
    const auto at_least = [&](int pct) { return w >= static_cast<long long>(pct) * row.seeds; };
    const auto verdict = [](bool ok) { return ok ? Verdict::Pass : Verdict::Fail; };
    if (row.bot == Bot::Greedy && row.difficulty == Difficulty::Normal) {
        const auto median = row.median_time_left();
        // Exact: total moves against floor x seeds.
        const bool enough_moves = row.total_moves >= static_cast<long long>(t->greedy_min_moves) * row.seeds;
        return verdict(at_least(t->greedy_win_pct) && median && *median >= t->greedy_median && enough_moves);
    }
    if (row.bot == Bot::Random && row.difficulty == Difficulty::Normal)
        return t->random_pct >= 0 ? verdict(at_least(t->random_pct)) : Verdict::None;
    if (row.bot == Bot::Planner && row.difficulty == Difficulty::Hard) return verdict(at_least(t->planner_hard_pct));
    return Verdict::None;
}

bool introduces_idea(const LevelData& level, const std::vector<const LevelData*>& earlier) {
    // AMBIGUOUS: a new totem or power-up id, first chains, first steel, or a
    // passive idea from progression's Episodes table.
    if (std::find(kPassiveIdeaLevels.begin(), kPassiveIdeaLevels.end(), level.id) != kPassiveIdeaLevels.end())
        return true;
    const auto seen = [&](auto&& has) {
        return std::any_of(earlier.begin(), earlier.end(), [&](const LevelData* l) { return l && has(*l); });
    };
    const auto lists = [](const std::vector<std::string>& v, const std::string& id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    };
    for (const std::string& id : level.totems_unlocked)
        if (!seen([&](const LevelData& l) { return lists(l.totems_unlocked, id); })) return true;
    for (const std::string& id : level.powerups_unlocked)
        if (!seen([&](const LevelData& l) { return lists(l.powerups_unlocked, id); })) return true;
    // Chains and steel are new on their first level only, not each time they
    // come back after a level without them.
    if (!level.chained_cells.empty() && !seen([](const LevelData& l) { return !l.chained_cells.empty(); }))
        return true;
    if (level.steel_rate > 0.0 && !seen([](const LevelData& l) { return l.steel_rate > 0.0; })) return true;
    return false;
}

std::vector<CurveCheck> curve_checks(const std::vector<LevelData>& levels, const std::vector<Row>& rows) {
    std::vector<CurveCheck> out;
    const auto greedy = [&](const LevelData& l) { return find_row(rows, l.id, Bot::Greedy, Difficulty::Normal); };
    const auto by_position = [&](int pos) -> const LevelData* {
        for (const LevelData& l : levels)
            if (campaign_position(l.episode, l.index) == pos && l.episode <= 5) return &l;
        return nullptr;
    };
    // Curve rule 1: consecutive campaign levels.
    for (const LevelData& l : levels) {
        if (l.episode > 5) continue;
        const int pos = campaign_position(l.episode, l.index);
        if (pos >= kCampaignLevels) continue;
        const LevelData* next = by_position(pos + 1);
        CurveCheck c;
        c.rule = "curve-1";
        c.levels = l.id + ">" + (next ? next->id : std::string("?"));
        const Row* a = greedy(l);
        const Row* b = next ? greedy(*next) : nullptr;
        if (!next || !a || !b || a->seeds <= 0 || b->seeds <= 0) {
            c.detail = !next ? "next level not in input" : "no greedy/normal rows";
            out.push_back(c);
            continue;
        }
        const long long diff = cross(b->wins, b->seeds, a->wins, a->seeds);  // (b - a) x seeds_a x seeds_b
        std::vector<const LevelData*> earlier;
        for (int p = 1; p <= pos; ++p)
            if (const LevelData* e = by_position(p)) earlier.push_back(e);
        if (introduces_idea(*next, earlier)) {
            // A level can be no easier than winning every run.
            const bool both_full = a->wins == a->seeds && b->wins == b->seeds;
            c.verdict = diff > 0 || both_full ? Verdict::Pass : Verdict::Fail;
            c.detail = "new idea: " + fmt1(b->win_rate_pct()) + "% must be above " + fmt1(a->win_rate_pct()) + "%" +
                       (both_full ? " (both 100%)" : "");
        } else {
            // 100 (b/sb - a/sa) <= 10.
            c.verdict = diff * 100 <= 10LL * a->seeds * b->seeds ? Verdict::Pass : Verdict::Fail;
            c.detail = fmt1(b->win_rate_pct()) + "% vs " + fmt1(a->win_rate_pct()) + "%, at most +10 points";
        }
        out.push_back(c);
    }
    // Curve rule 2: each episode's l01 at least as easy as its l03.
    std::set<int> episodes;
    for (const LevelData& l : levels) episodes.insert(l.episode);
    for (int e : episodes) {
        const LevelData* first = nullptr;
        const LevelData* third = nullptr;
        for (const LevelData& l : levels) {
            if (l.episode == e && l.index == 1) first = &l;
            if (l.episode == e && l.index == 3) third = &l;
        }
        CurveCheck c;
        c.rule = "curve-2";
        c.levels = "e" + std::to_string(e) + "-l01>=e" + std::to_string(e) + "-l03";
        const Row* a = first ? greedy(*first) : nullptr;
        const Row* b = third ? greedy(*third) : nullptr;
        if (!a || !b || a->seeds <= 0 || b->seeds <= 0) {
            c.detail = (!first || !third) ? "level not in input" : "no greedy/normal rows";
        } else {
            c.verdict = cross(a->wins, a->seeds, b->wins, b->seeds) >= 0 ? Verdict::Pass : Verdict::Fail;
            c.detail = fmt1(a->win_rate_pct()) + "% vs " + fmt1(b->win_rate_pct()) + "%";
        }
        out.push_back(c);
    }
    return out;
}

// ---- Clock rule ---------------------------------------------------------------

int required_clock(long long moves, int runs) noexcept {
    if (runs <= 0 || moves <= 0) return 0;
    // Clock in tens of seconds: ceil(moves x millis x percent /
    // (runs x 1000 x 100 x 10)). Past this many moves the product would
    // overflow; no run gets near it.
    constexpr long long kFactor = static_cast<long long>(kHumanMoveMillis) * kClockMarginPercent;
    if (moves > LLONG_MAX / kFactor) return INT_MAX;
    const long long num = moves * kFactor;
    const long long den = static_cast<long long>(runs) * 1000 * 100 * kClockStepSeconds;
    const long long tens = (num + den - 1) / den;
    return tens > INT_MAX / kClockStepSeconds ? INT_MAX : static_cast<int>(tens) * kClockStepSeconds;
}

std::vector<Row> clock_rows(const std::vector<LevelData>& levels, int seeds) {
    std::vector<Row> rows;
    for (const LevelData& l : levels)
        for (Difficulty d : {Difficulty::Normal, Difficulty::Hard})
            rows.push_back(simulate(l, d, clock_upgrades(l), Bot::Greedy, seeds));
    return rows;
}

std::vector<CurveCheck> clock_checks(const std::vector<LevelData>& levels, const std::vector<Row>& rows) {
    std::vector<CurveCheck> out;
    for (const LevelData& l : levels)
        for (Difficulty d : {Difficulty::Normal, Difficulty::Hard}) {
            CurveCheck c;
            c.rule = "clock";
            c.levels = l.id + " " + std::string(difficulty_name(d));
            const Row* r = find_row(rows, l.id, Bot::Greedy, d);
            if (!r || r->seeds <= 0) {
                c.detail = "no greedy rows";
                out.push_back(c);
                continue;
            }
            const double base = l.time_limit_for(d, 0);
            if (r->wins <= 0) {
                c.verdict = Verdict::Fail;
                c.detail = "base " + fmt1(base) + "s: greedy never won, so no clock can be set";
                out.push_back(c);
                continue;
            }
            const int need = required_clock(r->win_moves, r->wins);
            c.verdict = base >= need ? Verdict::Pass : Verdict::Fail;
            c.detail = "base " + fmt1(base) + "s must reach " + std::to_string(need) + "s (" +
                       fmt1(static_cast<double>(r->win_moves) / r->wins) + " moves a win x 3.5 s x 1.15)";
            out.push_back(c);
        }
    return out;
}

// ---- Output -------------------------------------------------------------------

std::string format_report(const std::vector<Row>& rows, const std::vector<CurveCheck>& curves, Format f) {
    std::string out;
    const auto median_text = [](const Row& r, const char* none) {
        const auto m = r.median_time_left();
        return m ? fmt1(*m) : std::string(none);
    };
    if (f == Format::Csv) {
        out += "level,bot,difficulty,seeds,wins,win_rate_pct,median_time_left,mean_moves,stuck,target,result\n";
        for (const Row& r : rows) {
            out += r.level + "," + std::string(bot_name(r.bot)) + "," + std::string(difficulty_name(r.difficulty)) +
                   "," + std::to_string(r.seeds) + "," + std::to_string(r.wins) + "," + fmt1(r.win_rate_pct()) + "," +
                   median_text(r, "") + "," + fmt1(r.mean_moves()) + "," + std::to_string(r.stuck) + "," +
                   target_text(r) + "," + std::string(verdict_name(evaluate(r))) + "\n";
        }
        out += "\nrule,levels,result,detail\n";
        for (const CurveCheck& c : curves)
            out += c.rule + "," + c.levels + "," + std::string(verdict_name(c.verdict)) + "," + c.detail + "\n";
        return out;
    }
    if (f == Format::Json) {
        out += "{\n  \"rows\": [";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const Row& r = rows[i];
            const auto m = r.median_time_left();
            out += std::string(i ? "," : "") + "\n    {\"level\": " + json_string(r.level) +
                   ", \"bot\": " + json_string(bot_name(r.bot)) +
                   ", \"difficulty\": " + json_string(difficulty_name(r.difficulty)) +
                   ", \"seeds\": " + std::to_string(r.seeds) + ", \"wins\": " + std::to_string(r.wins) +
                   ", \"win_rate_pct\": " + fmt1(r.win_rate_pct()) +
                   ", \"median_time_left\": " + (m ? fmt1(*m) : std::string("null")) +
                   ", \"mean_moves\": " + fmt1(r.mean_moves()) + ", \"stuck\": " + std::to_string(r.stuck) +
                   ", \"target\": " + json_string(target_text(r)) +
                   ", \"result\": " + json_string(verdict_name(evaluate(r))) + "}";
        }
        out += "\n  ],\n  \"curves\": [";
        for (std::size_t i = 0; i < curves.size(); ++i) {
            const CurveCheck& c = curves[i];
            out += std::string(i ? "," : "") + "\n    {\"rule\": " + json_string(c.rule) +
                   ", \"levels\": " + json_string(c.levels) + ", \"result\": " + json_string(verdict_name(c.verdict)) +
                   ", \"detail\": " + json_string(c.detail) + "}";
        }
        out += "\n  ]\n}\n";
        return out;
    }
    char line[512];
    std::snprintf(line, sizeof line, "%-8s %-8s %-7s %6s %6s %7s %8s %8s %6s  %-34s %s\n", "level", "bot", "diff",
                  "seeds", "wins", "win%", "median", "moves", "stuck", "target", "result");
    out += line;
    for (const Row& r : rows) {
        std::snprintf(line, sizeof line, "%-8s %-8s %-7s %6d %6d %7s %8s %8s %6d  %-34s %s\n", r.level.c_str(),
                      std::string(bot_name(r.bot)).c_str(), std::string(difficulty_name(r.difficulty)).c_str(),
                      r.seeds, r.wins, fmt1(r.win_rate_pct()).c_str(), median_text(r, "-").c_str(),
                      fmt1(r.mean_moves()).c_str(), r.stuck, target_text(r).c_str(),
                      std::string(verdict_name(evaluate(r))).c_str());
        out += line;
    }
    if (!curves.empty()) {
        out += "\n";
        for (const CurveCheck& c : curves) {
            std::snprintf(line, sizeof line, "%-8s %-22s %-8s %s\n", c.rule.c_str(), c.levels.c_str(),
                          std::string(verdict_name(c.verdict)).c_str(), c.detail.c_str());
            out += line;
        }
    }
    return out;
}

}  // namespace montezuma::sim
