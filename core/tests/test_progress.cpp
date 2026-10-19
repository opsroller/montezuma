// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Progress, save and trophy tests (progression.md, rules-bonus-games.md
// BG-01/BG-02, rules-spec.md FRZ-04). No framework dependency; run via ctest.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "montezuma/level_data.hpp"
#include "montezuma/level_session.hpp"
#include "montezuma/progress.hpp"
#include "montezuma/trophies.hpp"

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

constexpr Difficulty N = Difficulty::Normal;
constexpr Difficulty H = Difficulty::Hard;

LevelPlay won(int score = 1000, double time_left = 30.0) {
    LevelPlay p;
    p.outcome = Outcome::Won;
    p.score = score;
    p.time_left = time_left;
    return p;
}

LevelPlay lost(int score = 500) {
    LevelPlay p;
    p.outcome = Outcome::Lost;
    p.score = score;
    return p;
}

std::string id(int index) { return campaign_level_ids()[static_cast<std::size_t>(index)]; }

// Win every level up to and including `last` (0-based) on `d`.
void win_through(Progress& p, int last, Difficulty d) {
    for (int i = 0; i <= last; ++i) p.record_level(id(i), d, won());
}

bool has(const std::vector<TrophyId>& v, TrophyId t) {
    for (TrophyId x : v)
        if (x == t) return true;
    return false;
}

StepReport step(int index, int cleared) {
    StepReport s;
    s.step = index;
    s.cleared.assign(static_cast<std::size_t>(cleared), Cell{0, 0});
    return s;
}

void test_campaign_ids() {
    const auto ids = campaign_level_ids();
    check(ids.size() == 41, "PROG-01: 41 campaign levels");
    check(ids.front() == "e1-l01" && ids.back() == "e5-l12", "PROG-01: e1-l01 first, e5-l12 last");
    check(ids[4] == "e1-l05" && ids[5] == "e2-l01" && ids[10] == "e2-l06" && ids[11] == "e3-l01" &&
              ids[18] == "e3-l08" && ids[19] == "e4-l01" && ids[28] == "e4-l10" && ids[29] == "e5-l01",
          "PROG-01: 5, 6, 8, 10, 12 levels per episode");
    check(campaign_index("e3-l04") == 14, "campaign_index e3-l04 is 14");
    check(!campaign_index("e1-l06") && !campaign_index("e6-l01") && !campaign_index("") &&
              !campaign_index("bg_accuracy-1"),
          "campaign_index rejects ids outside the campaign");
}

void test_opening_levels() {
    Progress p;
    check(p.is_open("e1-l01", N) && !p.is_open("e1-l01", H), "PROG-01, DIFF-02: only normal e1-l01 open at first");
    check(!p.is_open("e1-l02", N) && !p.is_open("e1-l02", H), "PROG-01: e1-l02 closed on a fresh profile");
    check(!p.is_open("e9-l01", N), "unknown level is never open");

    p.record_level("e1-l01", N, lost());
    check(!p.is_open("e1-l02", N), "PROG-07: a loss opens nothing");
    check(p.is_open("e1-l01", N) && !p.is_won("e1-l01", N), "PROG-07: lost level stays open, not won");
    p.record_level("e1-l01", N, lost());  // PROG-07: retried at once

    p.record_level("e1-l01", N, won());
    check(p.is_open("e1-l02", N), "PROG-01: a win opens the next level on that difficulty");
    check(!p.is_open("e1-l02", H), "AMBIGUOUS default: unlocks are per difficulty");

    win_through(p, 4, N);
    check(p.is_open("e2-l01", N), "PROG-01: e1-l05 won opens e2-l01");

    check(throws<std::invalid_argument>([&] { p.record_level("e1-l06", N, won()); }), "e1-l06 rejected");
    check(throws<std::invalid_argument>([&] { p.record_level("e6-l01", N, won()); }), "e6-l01 rejected");
    check(throws<std::invalid_argument>([&] { p.record_level("e3-l01", N, won()); }), "closed level rejected");
    check(throws<std::invalid_argument>([&] { p.record_level("e1-l01", N, LevelPlay{}); }),
          "a play still in progress is rejected");
    check(throws<std::invalid_argument>([&] { p.record_level("e1-l01", N, won(-1)); }), "negative score rejected");
    check(throws<std::invalid_argument>([&] { p.record_level("e1-l01", N, won(1, -1.0)); }),
          "negative time left rejected");
    check(throws<std::invalid_argument>([&] { p.record_level("e1-l01", N, won(1, std::nan(""))); }),
          "NaN time left rejected");
}

void test_first_win_stars() {
    Progress p;
    Reward r = p.record_level("e1-l01", N, won());
    check(r.stars == 1 && p.stars() == 1, "PROG-02: first win on normal gives 1 star");
    r = p.record_level("e1-l01", N, won());
    check(r.stars == 0 && p.stars() == 1, "PROG-02: second win on normal gives none");
    r = p.record_level("e1-l02", N, lost());
    check(r.stars == 0 && p.stars() == 1, "PROG-07: a loss gives nothing");

    Progress all;
    win_through(all, 40, N);
    const int before = all.stars();
    r = all.record_level("e1-l01", H, won());
    check(r.stars == 1 && all.stars() == before + 1, "PROG-02: first win on hard gives one more");
}

void test_best_scores() {
    Progress p;
    check(!p.best_score("e1-l01", N), "no best score before a play");
    p.record_level("e1-l01", N, lost(700));
    check(p.best_score("e1-l01", N) == 700, "AMBIGUOUS default: a lost play counts for best score");
    p.record_level("e1-l01", N, won(500));
    check(p.best_score("e1-l01", N) == 700, "a lower score does not replace the best");
    p.record_level("e1-l01", N, won(900));
    check(p.best_score("e1-l01", N) == 900, "a higher score replaces it");
    check(!p.best_score("e1-l01", H), "best scores are per difficulty");
}

void test_bonus_levels() {
    const auto b = bonus_levels();
    const char* ids[] = {"bg_accuracy-1", "bg_memory-1",   "bg_reaction-1", "bg_attention-1",
                         "bg_accuracy-2", "bg_memory-2",   "bg_reaction-2", "bg_attention-2"};
    const char* after[] = {"e1-l05", "e2-l06", "e3-l04", "e3-l08", "e4-l05", "e4-l10", "e5-l04", "e5-l08"};
    for (std::size_t i = 0; i < kBonusLevelCount; ++i)
        check(b[i].id == ids[i] && b[i].after == after[i], std::string("BG-01: bonus level ") + ids[i]);
    check(!bonus_level("bg_accuracy-3"), "no tier 3 bonus level");

    Progress p;
    check(!p.bonus_open("bg_accuracy-1"), "BG-01: closed before e1-l05 is won");
    check(throws<std::invalid_argument>([&] { p.record_bonus("bg_accuracy-1", true); }), "closed bonus rejected");
    check(throws<std::invalid_argument>([&] { p.record_bonus("bg_nothing-1", true); }), "unknown bonus rejected");
    win_through(p, 4, N);
    check(p.bonus_open("bg_accuracy-1"), "BG-01: open once e1-l05 is won on any difficulty");
    const int before = p.stars();
    Reward r = p.record_bonus("bg_accuracy-1", true);
    check(r.stars == 1 && p.stars() == before + 1, "BG-01: first pass gives 1 star");
    check(p.bonus_result("bg_accuracy-1") == BonusResult::Passed, "bonus result is Passed");
    r = p.record_bonus("bg_accuracy-1", true);
    check(r.stars == 0, "BG-02: a replay pass gives no star");
    r = p.record_bonus("bg_accuracy-1", false);
    check(p.bonus_result("bg_accuracy-1") == BonusResult::Passed, "a failed replay keeps Passed");

    win_through(p, 10, N);
    r = p.record_bonus("bg_memory-1", false);
    check(r.stars == 0 && p.bonus_result("bg_memory-1") == BonusResult::Failed, "BG-01: a failure gives nothing");
    r = p.record_bonus("bg_memory-1", true);
    check(r.stars == 0, "BG-02 AMBIGUOUS default: a pass after a failed first play gives no star");
}

void test_star_sand() {
    Progress p;
    check(p.add_star_sand(49) == 0 && p.stars() == 0 && p.star_sand() == 49, "PROG-06: 49 sand is no star");
    check(p.add_star_sand(1) == 1 && p.stars() == 1 && p.star_sand() == 0, "PROG-06: 50 sand is 1 star at once");
    check(p.add_star_sand(120) == 2 && p.stars() == 3 && p.star_sand() == 20, "PROG-06: 120 sand is 2 stars, 20 left");
    check(throws<std::invalid_argument>([&] { p.add_star_sand(-1); }), "negative sand rejected");

    LevelPlay play = lost();
    play.star_sand = 35;
    Reward r = p.record_level("e1-l01", N, play);
    check(r.stars == 1 && p.star_sand() == 5, "FRZ-04: a play's sand is added, lost or won");
}

struct TableRow {
    UpgradeItem item;
    std::vector<const char*> levels;
};

void test_unlock_tables() {
    using T = TotemId;
    using P = PowerUpId;
    const std::vector<TableRow> rows{
        {UpgradeItem::timer(), {"e1-l02", "e3-l04", "e4-l06", "e5-l08"}},
        {UpgradeItem::gems(), {"e2-l05", "e3-l08", "e5-l02"}},
        {UpgradeItem::frenzy(), {"e1-l04", "e3-l03", "e4-l05", "e5-l07"}},
        {UpgradeItem::power_up(P::Tnt), {"e1-l01", "e3-l01", "e4-l08"}},
        {UpgradeItem::power_up(P::ColorClear), {"e1-l02", "e3-l06", "e4-l09"}},
        {UpgradeItem::power_up(P::Lightning), {"e1-l04", "e1-l05", "e4-l01"}},
        {UpgradeItem::power_up(P::Clock), {"e2-l01", "e4-l02", "e5-l04"}},
        {UpgradeItem::power_up(P::Multiplier), {"e2-l02", "e3-l05", "e4-l10"}},
        {UpgradeItem::power_up(P::Hint), {"e2-l06", "e4-l03", "e5-l03"}},
        {UpgradeItem::totem(T::Fire), {"e1-l03", "e3-l03", "e4-l04", "e5-l02"}},
        {UpgradeItem::totem(T::Time), {"e1-l05", "e3-l02", "e4-l08", "e5-l05"}},
        {UpgradeItem::totem(T::Gold), {"e2-l01", "e3-l06", "e4-l03", "e5-l01"}},
        {UpgradeItem::totem(T::Gift), {"e2-l03", "e3-l05", "e4-l07", "e5-l09"}},
        {UpgradeItem::totem(T::Harvest), {"e2-l04", "e3-l07", "e4-l10", "e5-l09"}},
        {UpgradeItem::totem(T::Paint), {"e2-l05", "e3-l07", "e4-l06", "e5-l06"}},
        {UpgradeItem::totem(T::Ward), {"e4-l01", "e4-l09", "e5-l07"}},
    };
    int n = 0;
    for (const TableRow& row : rows) {
        const std::string label = "unlock table row " + std::to_string(n++);
        check(max_tier(row.item) == static_cast<int>(row.levels.size()), label + " max tier");
        for (std::size_t t = 0; t < row.levels.size(); ++t)
            check(unlock_level(row.item, static_cast<int>(t) + 1) == std::string_view(row.levels[t]),
                  label + " tier " + std::to_string(t + 1));
        check(!unlock_level(row.item, 0) && !unlock_level(row.item, max_tier(row.item) + 1),
              label + " no tier 0 or past the last");
    }
    check(!tier_is_free(UpgradeItem::timer(), 1) && !tier_is_free(UpgradeItem::gems(), 1),
          "PROG-03: timer and gems tier 1 cost a star");
    check(tier_is_free(UpgradeItem::frenzy(), 1) && tier_is_free(UpgradeItem::totem(T::Fire), 1) &&
              tier_is_free(UpgradeItem::power_up(P::Tnt), 1),
          "PROG-03: frenzy, totem and power-up tier 1 are free");
    check(!tier_is_free(UpgradeItem::totem(T::Fire), 2), "PROG-04: tier 2 is never free");
}

void test_buying() {
    Progress p;
    check(p.tier(UpgradeItem::power_up(PowerUpId::Tnt)) == 1, "PROG-03: tnt tier 1 owned from the start");
    check(p.tier(UpgradeItem::totem(TotemId::Fire)) == 0, "fire not owned before e1-l03 opens");
    check(!p.can_buy(UpgradeItem::timer()), "PROG-04: timer tier 1 needs e1-l02 open");

    p.record_level("e1-l01", N, won());  // 1 star, e1-l02 open
    check(p.can_buy(UpgradeItem::timer()), "PROG-04: timer tier 1 buyable once e1-l02 opens");
    check(p.buy(UpgradeItem::timer()) && p.stars() == 0 && p.tier(UpgradeItem::timer()) == 1,
          "PROG-04: buying timer tier 1 costs 1 star");
    check(!p.buy(UpgradeItem::timer()), "PROG-04: timer tier 2 needs e3-l04 open");
    check(p.tier(UpgradeItem::timer()) == 1 && p.stars() == 0, "a failed buy changes nothing");

    win_through(p, 13, N);  // through e3-l03, e3-l04 open
    check(p.tier(UpgradeItem::totem(TotemId::Fire)) == 1, "PROG-03: fire tier 1 free once e1-l03 opens");
    check(p.tier(UpgradeItem::frenzy()) == 1, "PROG-03: frenzy tier 1 free once e1-l04 opens");
    check(p.tier(UpgradeItem::totem(TotemId::Ward)) == 0, "ward not owned before e4-l01");
    check(!p.can_buy(UpgradeItem::totem(TotemId::Ward)), "PROG-04: ward tier 2 needs tier 1 owned");
    check(p.buy(UpgradeItem::timer()) && p.tier(UpgradeItem::timer()) == 2, "timer tier 2 bought at e3-l04");
    check(!p.buy(UpgradeItem::timer()), "timer tier 3 needs e4-l06");

    // Every star in the campaign and every tier: buy everything at the end.
    Progress all;
    win_through(all, 40, N);
    all.add_star_sand(kStarSandPerStar * 100);
    const std::vector<UpgradeItem> items{
        UpgradeItem::timer(), UpgradeItem::gems(), UpgradeItem::frenzy(),
        UpgradeItem::totem(TotemId::Fire), UpgradeItem::totem(TotemId::Ward),
        UpgradeItem::power_up(PowerUpId::Tnt), UpgradeItem::power_up(PowerUpId::Hint)};
    for (const UpgradeItem& item : items) {
        while (all.buy(item)) {
        }
        check(all.tier(item) == max_tier(item), "PROG-04: every tier can be bought by the end");
        check(!all.can_buy(item), "PROG-04: nothing past the last tier");
    }
    check(all.tier(UpgradeItem::timer()) == 4 && all.tier(UpgradeItem::gems()) == 3 &&
              all.tier(UpgradeItem::power_up(PowerUpId::Tnt)) == 3,
          "PROG-04: timer 4, gems 3, tnt 3 are the last tiers");
}

void test_upgrades_for() {
    Progress p;
    win_through(p, 40, N);
    p.add_star_sand(kStarSandPerStar * 100);
    while (p.buy(UpgradeItem::totem(TotemId::Fire))) {
    }
    while (p.buy(UpgradeItem::gems())) {
    }
    const Upgrades up = p.upgrades_for("e5-l12");
    check(up.totem_tiers[0] == 4 && up.gems_tier == 3, "upgrades_for reports owned tiers");
    check(up.frenzy_tier == 1, "upgrades_for: frenzy tier 1 free");
    check(p.upgrades_for("e1-l01").frenzy_tier == 0, "FRZ-01: no Frenzy before e1-l04");
    check(p.upgrades_for("e1-l04").frenzy_tier == 1, "FRZ-01: Frenzy from e1-l04");
    check(p.upgrades_for("e1-l01").gems_tier == 3, "PROG-05: board upgrades apply on every level");
    check(throws<std::invalid_argument>([&] { (void)p.upgrades_for("e7-l01"); }), "upgrades_for unknown level");

    Progress fresh;
    const Upgrades none = fresh.upgrades_for("e1-l01");
    check(none.timer_tier == 0 && none.gems_tier == 0 && none.frenzy_tier == 0, "fresh profile: base upgrades");

    // Every committed level starts with what progress hands it.
    namespace fs = std::filesystem;
    const fs::path root = fs::path(MONTEZUMA_SOURCE_DIR) / "game" / "data" / "levels";
    int started = 0;
    for (const std::string& level_id : campaign_level_ids()) {
        const fs::path file = root / level_id.substr(0, 2) / (level_id.substr(3) + ".json");
        std::ifstream in(file, std::ios::binary);
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        const LevelData level = LevelData::load(ss.str());
        for (Difficulty d : {N, H}) {
            bool ok = true;
            try {
                LevelSession session(level, d, p.upgrades_for(level_id), 1);
                ok = session.outcome() == Outcome::Playing;
            } catch (const std::exception&) {
                ok = false;
            }
            check(ok, "LevelSession accepts upgrades_for(" + level_id + ")");
        }
        ++started;
    }
    check(started == 41, "every campaign level file found");
}

void test_trophy_names() {
    const char* names[] = {"first_find", "chain_reaction", "demolition", "totem_keeper", "quick_hands",
                           "frenzy",     "sharp_eye",      "explorer",   "legend"};
    for (std::size_t i = 0; i < kTrophyCount; ++i) {
        const auto t = static_cast<TrophyId>(i);
        check(trophy_name(t) == names[i], std::string("trophy name ") + names[i]);
        check(trophy_from_name(names[i]) == t, std::string("trophy_from_name ") + names[i]);
    }
    check(!trophy_from_name("t01") && !trophy_from_name(""), "unknown trophy name is empty");
}

void test_watch_moves() {
    TrophyWatch w;
    MoveReport m;
    for (int s = 0; s <= 4; ++s) m.steps.push_back(step(s, 3));
    w.on_move(m);
    check(!w.earned()[static_cast<std::size_t>(TrophyId::ChainReaction)], "chain_reaction: step 4 is not enough");
    m.steps.push_back(step(5, 3));
    w.on_move(m);
    check(w.earned()[static_cast<std::size_t>(TrophyId::ChainReaction)], "chain_reaction: step 5 earns it");

    TrophyWatch d;
    MoveReport big;
    big.steps.push_back(step(0, 30));
    TotemFiring f;
    f.effect.totem = TotemId::Fire;
    f.steps.push_back(step(1, 9));
    big.totems_fired.push_back(f);
    d.on_move(big);
    check(!d.earned()[static_cast<std::size_t>(TrophyId::Demolition)], "demolition: 39 pieces is not enough");
    big.totems_fired[0].steps[0] = step(1, 10);
    d.on_move(big);
    check(d.earned()[static_cast<std::size_t>(TrophyId::Demolition)], "demolition: 40 with a totem's clear");

    TrophyWatch t;
    TotemFiring totem_step;
    totem_step.steps.push_back(step(9, 1));
    MoveReport only_totem;
    only_totem.steps.push_back(step(0, 3));
    only_totem.totems_fired.push_back(totem_step);
    t.on_move(only_totem);
    check(!t.earned()[static_cast<std::size_t>(TrophyId::ChainReaction)],
          "chain_reaction AMBIGUOUS default: a totem's step numbers do not count");
}

void test_watch_frenzy() {
    TrophyWatch w;
    MoveReport m;
    m.frenzy_started = true;
    m.star_sand_gained = 5;
    w.on_move(m);
    MoveReport more;
    more.star_sand_gained = 4;
    w.on_move(more);
    MoveReport restart;
    restart.frenzy_restarted = true;
    restart.star_sand_gained = 2;
    w.on_move(restart);
    check(w.frenzy_gems() == 11 && !w.earned()[static_cast<std::size_t>(TrophyId::Frenzy)],
          "frenzy: 11 gems is not enough, a restart continues it");
    TickReport end;
    end.frenzy_ended = true;
    w.on_tick(end);
    check(w.frenzy_gems() == 0, "frenzy: count resets when the Frenzy ends");
    MoveReport again;
    again.frenzy_started = true;
    again.star_sand_gained = 12;
    w.on_move(again);
    check(w.earned()[static_cast<std::size_t>(TrophyId::Frenzy)], "frenzy: 12 gems in one Frenzy earns it");
}

void test_watch_totems_and_play() {
    // A real session: the watch reads the session's outcome, score and time.
    namespace fs = std::filesystem;
    std::ifstream in(fs::path(MONTEZUMA_SOURCE_DIR) / "game" / "data" / "levels" / "e1" / "l01.json");
    std::stringstream ss;
    ss << in.rdbuf();
    const LevelData level = LevelData::load(ss.str());
    Progress fresh;
    LevelSession session(level, N, fresh.upgrades_for("e1-l01"), 7);
    TrophyWatch w;
    MoveReport fired;
    TotemFiring f;
    f.effect.totem = TotemId::Gold;
    fired.totems_fired.push_back(f);
    w.on_move(fired);
    session.set_time_left(160.0);
    session.set_score(4321);
    LevelPlay play = w.play(session);
    check(play.outcome == Outcome::Playing && play.score == 4321 && play.time_left == 160.0,
          "play() copies outcome, score and time");
    check(play.totems_fired[static_cast<std::size_t>(TotemId::Gold)] &&
              !play.totems_fired[static_cast<std::size_t>(TotemId::Fire)],
          "play() lists the totems that fired");

    // quick_hands is decided on the recorded outcome.
    Progress p;
    LevelPlay fast = won(100, 150.0);
    Reward r = p.record_level("e1-l01", N, fast);
    check(has(r.trophies, TrophyId::QuickHands), "quick_hands: won with 150 s left");
    check(has(r.trophies, TrophyId::FirstFind), "first_find: e1-l01 won");
    r = p.record_level("e1-l01", N, fast);
    check(r.trophies.empty(), "trophies are earned once");

    Progress q;
    win_through(q, 40, N);  // DIFF-02: opens hard
    r = q.record_level("e1-l01", H, won(100, 149.9));
    check(!has(r.trophies, TrophyId::QuickHands), "quick_hands: 149.9 s is not enough");
    LevelPlay slow_loss = lost();
    slow_loss.time_left = 200.0;
    r = q.record_level("e1-l02", H, slow_loss);
    check(!has(r.trophies, TrophyId::QuickHands), "quick_hands: a loss never earns it");

    // In-level trophies come through LevelPlay::earned, once.
    LevelPlay chained = won();
    chained.earned = {TrophyId::ChainReaction, TrophyId::Demolition};
    r = q.record_level("e1-l01", H, chained);
    check(r.trophies == std::vector<TrophyId>{TrophyId::ChainReaction, TrophyId::Demolition},
          "in-level trophies reported in order");
    r = q.record_level("e1-l01", H, chained);
    check(r.trophies.empty(), "in-level trophies once only");
    LevelPlay bogus = won();
    bogus.earned = {TrophyId::Legend};
    r = q.record_level("e1-l01", H, bogus);
    check(!q.has_trophy(TrophyId::Legend), "profile trophies are never taken from a play");
}

void test_profile_trophies() {
    Progress p;
    for (int t = 0; t < 6; ++t) {
        LevelPlay play = lost();
        play.totems_fired[static_cast<std::size_t>(t)] = true;
        p.record_level("e1-l01", N, play);
    }
    check(!p.has_trophy(TrophyId::TotemKeeper), "totem_keeper: six totems are not enough");
    // Totems fired persist through a save.
    const ProgressLoadResult loaded = Progress::try_from_json(p.to_json());
    check(loaded.progress.has_value(), "save with totems fired loads");
    Progress q = *loaded.progress;
    LevelPlay last = lost();
    last.totems_fired[static_cast<std::size_t>(TotemId::Paint)] = true;
    Reward r = q.record_level("e1-l01", N, last);
    check(has(r.trophies, TrophyId::TotemKeeper), "totem_keeper: the seventh earns it across levels and saves");

    Progress e;
    win_through(e, 40, N);
    check(e.has_trophy(TrophyId::Explorer) && !e.has_trophy(TrophyId::Legend), "explorer: e5-l12 won on normal");
    win_through(e, 40, H);
    check(e.has_trophy(TrophyId::Legend), "legend: e5-l12 won on hard");

    for (std::size_t i = 0; i + 1 < kBonusLevelCount; ++i) e.record_bonus(bonus_levels()[i].id, true);
    check(!e.has_trophy(TrophyId::SharpEye), "sharp_eye: seven bonus levels are not enough");
    r = e.record_bonus(bonus_levels()[kBonusLevelCount - 1].id, true);
    check(has(r.trophies, TrophyId::SharpEye), "sharp_eye: all eight passed");
}

void test_save_round_trip() {
    Progress p;
    win_through(p, 40, N);
    p.record_level("e1-l01", H, won(4242, 151.0));
    p.record_level("e1-l02", H, lost(777));
    p.record_bonus("bg_accuracy-1", true);
    p.record_bonus("bg_memory-1", false);
    p.add_star_sand(37);
    p.buy(UpgradeItem::timer());
    p.buy(UpgradeItem::power_up(PowerUpId::Lightning));
    LevelPlay play = won();
    play.totems_fired[static_cast<std::size_t>(TotemId::Harvest)] = true;
    play.earned = {TrophyId::Frenzy};
    p.record_level("e1-l02", H, play);

    const std::string text = p.to_json();
    const ProgressLoadResult r = Progress::try_from_json(text);
    check(r.progress.has_value() && r.error.empty(), "save loads: " + r.error);
    check(r.progress && *r.progress == p, "save round-trips every field");
    check(r.progress && r.progress->to_json() == text, "save is byte-identical after a round trip");
    Progress copy = p;
    check(copy.to_json() == text, "same progress gives the same text");
    check(text.find("\"version\":1") != std::string::npos, "save carries a version");
    check(r.progress && r.progress->tier(UpgradeItem::timer()) == 1 &&
              r.progress->tier(UpgradeItem::power_up(PowerUpId::Lightning)) == 2,
          "PROG-05: bought tiers survive a save");

    const ProgressLoadResult empty = Progress::try_from_json("");
    check(empty.progress && *empty.progress == Progress{}, "an empty save is a fresh profile");
    const ProgressLoadResult blank = Progress::try_from_json("  \n");
    check(blank.progress && *blank.progress == Progress{}, "a blank save is a fresh profile");
}

void test_bad_saves() {
    const std::vector<std::pair<std::string, std::string>> bad{
        {"{", "malformed JSON"},
        {"[]", "not an object"},
        {R"({"version":2})", "wrong version"},
        {R"({"stars":1})", "no version"},
        {R"({"version":1,"stars":-1})", "negative stars"},
        {R"({"version":1,"star_sand":50})", "sand at or above a star"},
        {R"({"version":1,"stars":"x"})", "stars not a number"},
        {R"({"version":1,"levels":{"normal":{"e9-l01":{"won":true}}}})", "unknown level"},
        {R"({"version":1,"levels":{"easy":{}}})", "unknown difficulty"},
        {R"({"version":1,"levels":{"normal":{"e1-l01":{"best_score":-5}}}})", "negative best score"},
        {R"({"version":1,"bonus":{"bg_x-1":"passed"}})", "unknown bonus level"},
        {R"({"version":1,"bonus":{"bg_accuracy-1":"maybe"}})", "unknown bonus result"},
        {R"({"version":1,"upgrades":{"timer":5}})", "timer tier out of range"},
        {R"({"version":1,"upgrades":{"totems":{"fire":5}}})", "totem tier out of range"},
        {R"({"version":1,"upgrades":{"totems":{"water":1}}})", "unknown totem"},
        {R"({"version":1,"upgrades":{"power_ups":{"tnt":4}}})", "power-up tier out of range"},
        {R"({"version":1,"trophies":["t01"]})", "unknown trophy"},
        {R"({"version":1,"totems_fired":["water"]})", "unknown fired totem"},
        {R"({"version":1,"extra":1})", "unknown field"},
    };
    for (const auto& [text, what] : bad) {
        ProgressLoadResult r;
        bool threw = false;
        try {
            r = Progress::try_from_json(text);
        } catch (...) {
            threw = true;
        }
        check(!threw && !r.progress && !r.error.empty(), "bad save rejected: " + what);
    }
}

void test_star_count_overflow() {
    // The loader accepts stars up to INT_MAX; every later gain must not wrap.
    const ProgressLoadResult full = Progress::try_from_json(
        R"({"version":1,"stars":2147483647,"levels":{"normal":{"e1-l05":{"won":true}}}})");
    check(full.progress.has_value(), "a save with INT_MAX stars loads");
    if (!full.progress) return;

    Progress a = *full.progress;
    try { a.record_level("e1-l01", N, won()); } catch (const std::exception&) {}
    check(a.stars() >= 0, "PROG-02: a first win at INT_MAX stars does not wrap negative");
    check(Progress::try_from_json(a.to_json()).progress.has_value(),
          "save written after a first win at INT_MAX stars loads again");

    Progress b = *full.progress;
    try { b.add_star_sand(100); } catch (const std::exception&) {}
    check(b.stars() >= 0, "PROG-06: sand turning into stars at INT_MAX does not wrap negative");
    check(Progress::try_from_json(b.to_json()).progress.has_value(),
          "save written after sand at INT_MAX stars loads again");

    Progress c = *full.progress;
    try { c.record_bonus("bg_accuracy-1", true); } catch (const std::exception&) {}
    check(c.stars() >= 0, "BG-01: a bonus star at INT_MAX stars does not wrap negative");
    check(Progress::try_from_json(c.to_json()).progress.has_value(),
          "save written after a bonus star at INT_MAX stars loads again");
}

void test_star_count_bounds() {
    // The loader accepts stars up to INT_MAX; any further star must not
    // wrap the count negative, and every profile to_json writes must load.
    ProgressLoadResult r = Progress::try_from_json(R"({"version":1,"stars":2147483647})");
    check(r.progress.has_value(), "a save at the stars bound loads");
    Progress p = *r.progress;
    p.record_level("e1-l01", N, won());
    check(p.stars() >= 0, "PROG-02: a first win at the stars bound does not wrap the count negative");
    check(Progress::try_from_json(p.to_json()).progress.has_value(),
          "a profile written by to_json always loads back");

    Progress q;
    for (int i = 0; i < 60; ++i) q.add_star_sand(2147483647);
    check(q.stars() >= 0, "PROG-06: star sand never wraps the star count negative");
    check(Progress::try_from_json(q.to_json()).progress.has_value(),
          "a profile written by to_json after much star sand loads back");
}

void test_hard_unlocks_after_last_normal_level() {
    // rule: DIFF-02 hard unlocks when the last normal level is won.
    Progress p;
    check(!p.is_open("e1-l01", H), "DIFF-02: hard e1-l01 closed on a fresh profile");
    check(throws<std::invalid_argument>([&] { p.record_level("e1-l01", H, won()); }),
          "DIFF-02: a hard play before hard unlocks is rejected");
    check(p.stars() == 0, "DIFF-02: no star from a locked hard level");
    win_through(p, 39, N);  // e1-l01 .. e5-l11 on normal
    check(!p.is_open("e1-l01", H), "DIFF-02: hard still closed before e5-l12 is won on normal");
    p.record_level("e5-l12", N, won());
    check(p.is_open("e1-l01", H), "DIFF-02: hard e1-l01 opens when e5-l12 is won on normal");
    check(!p.is_open("e1-l02", H), "DIFF-02: only hard e1-l01 opens, not the rest");
}

}  // namespace

int main() {
    test_campaign_ids();
    test_opening_levels();
    test_first_win_stars();
    test_best_scores();
    test_bonus_levels();
    test_star_sand();
    test_unlock_tables();
    test_buying();
    test_upgrades_for();
    test_trophy_names();
    test_watch_moves();
    test_watch_frenzy();
    test_watch_totems_and_play();
    test_profile_trophies();
    test_save_round_trip();
    test_bad_saves();
    test_star_count_overflow();
    test_star_count_bounds();
    test_hard_unlocks_after_last_normal_level();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
