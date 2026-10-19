// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/progress.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace montezuma {

using nlohmann::json;

namespace {

std::vector<std::string> make_level_ids() {
    std::vector<std::string> ids;
    for (std::size_t e = 0; e < kEpisodeLevelCounts.size(); ++e)
        for (int l = 1; l <= kEpisodeLevelCounts[e]; ++l) {
            std::string id = "e" + std::to_string(e + 1) + "-l";
            if (l < 10) id += '0';
            id += std::to_string(l);
            ids.push_back(std::move(id));
        }
    return ids;
}

const std::vector<std::string>& level_ids() {
    static const std::vector<std::string> ids = make_level_ids();
    return ids;
}

// rule: BG-01 bonus levels follow these levels (progression Episodes).
constexpr std::array<BonusLevel, kBonusLevelCount> kBonusLevels{{
    {"bg_accuracy-1", "bg_accuracy", 1, "e1-l05"},
    {"bg_memory-1", "bg_memory", 1, "e2-l06"},
    {"bg_reaction-1", "bg_reaction", 1, "e3-l04"},
    {"bg_attention-1", "bg_attention", 1, "e3-l08"},
    {"bg_accuracy-2", "bg_accuracy", 2, "e4-l05"},
    {"bg_memory-2", "bg_memory", 2, "e4-l10"},
    {"bg_reaction-2", "bg_reaction", 2, "e5-l04"},
    {"bg_attention-2", "bg_attention", 2, "e5-l08"},
}};

using Tiers = std::array<std::string_view, 4>;  // "" past the last tier

// rule: progression Upgrades table.
constexpr Tiers kTimerLevels{"e1-l02", "e3-l04", "e4-l06", "e5-l08"};
constexpr Tiers kGemsLevels{"e2-l05", "e3-l08", "e5-l02", ""};
constexpr Tiers kFrenzyLevels{"e1-l04", "e3-l03", "e4-l05", "e5-l07"};

// rule: progression Unlocks table, by TotemId.
constexpr std::array<Tiers, kTotemCount> kTotemLevels{{
    {"e1-l03", "e3-l03", "e4-l04", "e5-l02"},  // fire
    {"e1-l05", "e3-l02", "e4-l08", "e5-l05"},  // time
    {"e2-l01", "e3-l06", "e4-l03", "e5-l01"},  // gold
    {"e2-l04", "e3-l07", "e4-l10", "e5-l09"},  // harvest
    {"e4-l01", "e4-l09", "e5-l07", ""},        // ward
    {"e2-l03", "e3-l05", "e4-l07", "e5-l09"},  // gift
    {"e2-l05", "e3-l07", "e4-l06", "e5-l06"},  // paint
}};

// rule: progression Unlocks table, by PowerUpId.
constexpr std::array<Tiers, kPowerUpCount> kPowerUpLevels{{
    {"e1-l01", "e3-l01", "e4-l08", ""},  // tnt
    {"e1-l02", "e3-l06", "e4-l09", ""},  // color_clear
    {"e1-l04", "e1-l05", "e4-l01", ""},  // lightning
    {"e2-l01", "e4-l02", "e5-l04", ""},  // clock
    {"e2-l02", "e3-l05", "e4-l10", ""},  // multiplier
    {"e2-l06", "e4-l03", "e5-l03", ""},  // hint
}};

const Tiers* tiers_of(UpgradeItem item) {
    switch (item.kind) {
        case UpgradeKind::Timer: return &kTimerLevels;
        case UpgradeKind::Gems: return &kGemsLevels;
        case UpgradeKind::Frenzy: return &kFrenzyLevels;
        case UpgradeKind::Totem:
            if (item.index < 0 || item.index >= static_cast<int>(kTotemCount)) return nullptr;
            return &kTotemLevels[static_cast<std::size_t>(item.index)];
        case UpgradeKind::PowerUp:
            if (item.index < 0 || item.index >= static_cast<int>(kPowerUpCount)) return nullptr;
            return &kPowerUpLevels[static_cast<std::size_t>(item.index)];
    }
    return nullptr;
}

constexpr std::size_t di(Difficulty d) { return d == Difficulty::Hard ? 1 : 0; }
constexpr std::array<std::string_view, 2> kDifficultyNames{"normal", "hard"};

std::string_view bonus_result_name(BonusResult r) {
    return r == BonusResult::Passed ? "passed" : r == BonusResult::Failed ? "failed" : "not_played";
}

// Trophies a level play may bring in (the rest are decided by the profile).
constexpr std::array<TrophyId, 3> kInLevelTrophies{TrophyId::ChainReaction, TrophyId::Demolition,
                                                   TrophyId::Frenzy};

// --- Loading ---------------------------------------------------------------

struct LoadError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(const std::string& what) { throw LoadError(what); }

void only_keys(const json& j, const std::string& path, std::initializer_list<std::string_view> keys) {
    if (!j.is_object()) fail(path + ": must be an object");
    for (auto it = j.begin(); it != j.end(); ++it)
        if (std::find(keys.begin(), keys.end(), it.key()) == keys.end()) fail(path + "." + it.key() + ": unknown field");
}

int int_in(const json& j, const std::string& path, std::int64_t lo, std::int64_t hi) {
    if (!j.is_number_integer()) fail(path + ": must be an integer");
    const auto v = j.get<std::int64_t>();
    if (v < lo || v > hi) fail(path + ": must be " + std::to_string(lo) + ".." + std::to_string(hi));
    return static_cast<int>(v);
}

int optional_int(const json& obj, std::string_view key, const std::string& path, std::int64_t lo, std::int64_t hi) {
    const auto it = obj.find(key);
    return it == obj.end() ? 0 : int_in(*it, path + "." + std::string(key), lo, hi);
}

constexpr std::int64_t kIntMax = 2147483647;

}  // namespace

std::span<const std::string> campaign_level_ids() { return level_ids(); }

std::optional<int> campaign_index(std::string_view level_id) {
    const auto& ids = level_ids();
    const auto it = std::find(ids.begin(), ids.end(), level_id);
    if (it == ids.end()) return std::nullopt;
    return static_cast<int>(it - ids.begin());
}

std::span<const BonusLevel, kBonusLevelCount> bonus_levels() { return kBonusLevels; }

std::optional<BonusLevel> bonus_level(std::string_view id) {
    for (const BonusLevel& b : kBonusLevels)
        if (b.id == id) return b;
    return std::nullopt;
}

int max_tier(UpgradeItem item) {
    const Tiers* t = tiers_of(item);
    if (!t) return 0;
    return static_cast<int>(std::count_if(t->begin(), t->end(), [](std::string_view s) { return !s.empty(); }));
}

std::optional<std::string_view> unlock_level(UpgradeItem item, int tier) {
    if (tier < 1 || tier > max_tier(item)) return std::nullopt;
    return (*tiers_of(item))[static_cast<std::size_t>(tier - 1)];
}

bool tier_is_free(UpgradeItem item, int tier) {
    return tier == 1 && item.kind != UpgradeKind::Timer && item.kind != UpgradeKind::Gems && max_tier(item) >= 1;
}

// --- Campaign ----------------------------------------------------------------

bool Progress::is_open(std::string_view level_id, Difficulty d) const {
    const auto i = campaign_index(level_id);
    if (!i) return false;
    if (*i == 0) {
        // rule: DIFF-02 hard unlocks when the last normal level is won.
        return d == Difficulty::Normal || is_won(level_ids().back(), Difficulty::Normal);
    }
    return is_won(level_ids()[static_cast<std::size_t>(*i - 1)], d);
}

bool Progress::is_won(std::string_view level_id, Difficulty d) const {
    const auto& m = levels_[di(d)];
    const auto it = m.find(level_id);
    return it != m.end() && it->second.won;
}

std::optional<int> Progress::best_score(std::string_view level_id, Difficulty d) const {
    const auto& m = levels_[di(d)];
    const auto it = m.find(level_id);
    return it == m.end() ? std::nullopt : it->second.best_score;
}

bool Progress::reached(std::string_view level_id) const {
    return is_open(level_id, Difficulty::Normal) || is_open(level_id, Difficulty::Hard);
}

Reward Progress::record_level(std::string_view level_id, Difficulty d, const LevelPlay& play) {
    if (!campaign_index(level_id)) throw std::invalid_argument("unknown level: " + std::string(level_id));
    if (!is_open(level_id, d)) throw std::invalid_argument("level is not open: " + std::string(level_id));
    if (play.outcome == Outcome::Playing) throw std::invalid_argument("the play is not finished");
    if (play.score < 0) throw std::invalid_argument("score must not be negative");
    if (play.star_sand < 0) throw std::invalid_argument("star sand must not be negative");
    if (!(play.time_left >= 0.0)) throw std::invalid_argument("time left must be a number at least 0");

    Reward reward;
    LevelRecord& rec = levels_[di(d)][std::string(level_id)];
    rec.best_score = std::max(rec.best_score.value_or(play.score), play.score);
    if (play.outcome == Outcome::Won && !rec.won) {
        rec.won = true;
        reward.stars += add_stars(kStarsPerFirstWin);
    }
    reward.stars += add_star_sand(play.star_sand);

    for (std::size_t i = 0; i < kTotemCount; ++i) totems_fired_[i] = totems_fired_[i] || play.totems_fired[i];
    for (TrophyId t : kInLevelTrophies)
        if (std::find(play.earned.begin(), play.earned.end(), t) != play.earned.end()) award(t, reward);
    if (play.outcome == Outcome::Won && play.time_left >= kQuickHandsSeconds) award(TrophyId::QuickHands, reward);
    check_profile_trophies(reward);
    std::sort(reward.trophies.begin(), reward.trophies.end());
    return reward;
}

// --- Bonus levels --------------------------------------------------------------

bool Progress::bonus_open(std::string_view bonus_id) const {
    const auto b = bonus_level(bonus_id);
    return b && (is_won(b->after, Difficulty::Normal) || is_won(b->after, Difficulty::Hard));
}

BonusResult Progress::bonus_result(std::string_view bonus_id) const {
    const auto it = bonus_.find(bonus_id);
    return it == bonus_.end() ? BonusResult::NotPlayed : it->second;
}

Reward Progress::record_bonus(std::string_view bonus_id, bool passed) {
    if (!bonus_level(bonus_id)) throw std::invalid_argument("unknown bonus level: " + std::string(bonus_id));
    if (!bonus_open(bonus_id)) throw std::invalid_argument("bonus level is not open: " + std::string(bonus_id));
    Reward reward;
    const BonusResult before = bonus_result(bonus_id);
    if (before == BonusResult::NotPlayed) {
        // rule: BG-01 the first play decides the star; BG-02 replays never give one.
        bonus_[std::string(bonus_id)] = passed ? BonusResult::Passed : BonusResult::Failed;
        if (passed) reward.stars = add_stars(kStarsPerFirstWin);
    } else if (passed && before == BonusResult::Failed) {
        bonus_[std::string(bonus_id)] = BonusResult::Passed;  // passed in practice, no star
    }
    check_profile_trophies(reward);
    std::sort(reward.trophies.begin(), reward.trophies.end());
    return reward;
}

// --- Stars -----------------------------------------------------------------------

int Progress::add_stars(int n) {
    // Saturate at kMaxStars so the count never wraps and every save loads.
    const int added = std::min(n, kMaxStars - stars_);
    stars_ += added;
    return added;
}

int Progress::add_star_sand(int sand) {
    if (sand < 0) throw std::invalid_argument("star sand must not be negative");
    const std::int64_t total = static_cast<std::int64_t>(star_sand_) + sand;
    star_sand_ = static_cast<int>(total % kStarSandPerStar);
    return add_stars(static_cast<int>(total / kStarSandPerStar));
}

// --- Upgrades ----------------------------------------------------------------------

int Progress::bought(UpgradeItem item) const {
    switch (item.kind) {
        case UpgradeKind::Timer: return timer_bought_;
        case UpgradeKind::Gems: return gems_bought_;
        case UpgradeKind::Frenzy: return frenzy_bought_;
        case UpgradeKind::Totem: return totem_bought_.at(static_cast<std::size_t>(item.index));
        case UpgradeKind::PowerUp: return power_up_bought_.at(static_cast<std::size_t>(item.index));
    }
    return 0;
}

int& Progress::bought_ref(UpgradeItem item) {
    switch (item.kind) {
        case UpgradeKind::Timer: return timer_bought_;
        case UpgradeKind::Gems: return gems_bought_;
        case UpgradeKind::Frenzy: return frenzy_bought_;
        case UpgradeKind::Totem: return totem_bought_.at(static_cast<std::size_t>(item.index));
        case UpgradeKind::PowerUp: return power_up_bought_.at(static_cast<std::size_t>(item.index));
    }
    throw std::invalid_argument("unknown upgrade kind");
}

int Progress::tier(UpgradeItem item) const {
    if (max_tier(item) == 0) return 0;
    const int b = bought(item);
    if (b > 0) return b;
    return tier_is_free(item, 1) && reached(*unlock_level(item, 1)) ? 1 : 0;
}

bool Progress::can_buy(UpgradeItem item) const {
    if (max_tier(item) == 0) return false;
    const int next = tier(item) + 1;
    const auto level = unlock_level(item, next);
    return level && !tier_is_free(item, next) && reached(*level) && stars_ >= kStarsPerTier;
}

bool Progress::buy(UpgradeItem item) {
    if (!can_buy(item)) return false;
    const int next = tier(item) + 1;
    bought_ref(item) = next;
    stars_ -= kStarsPerTier;
    return true;
}

Upgrades Progress::upgrades_for(std::string_view level_id) const {
    const auto index = campaign_index(level_id);
    if (!index) throw std::invalid_argument("unknown level: " + std::string(level_id));
    Upgrades up;
    up.timer_tier = tier(UpgradeItem::timer());
    up.gems_tier = tier(UpgradeItem::gems());
    // rule: FRZ-01 no Frenzy before its unlock level.
    const auto frenzy_from = campaign_index(*unlock_level(UpgradeItem::frenzy(), 1));
    up.frenzy_tier = *index >= *frenzy_from ? tier(UpgradeItem::frenzy()) : 0;
    for (std::size_t i = 0; i < kTotemCount; ++i)
        up.totem_tiers[i] = tier(UpgradeItem::totem(static_cast<TotemId>(i)));
    for (std::size_t i = 0; i < kPowerUpCount; ++i)
        up.power_up_tiers[i] = tier(UpgradeItem::power_up(static_cast<PowerUpId>(i)));
    return up;
}

// --- Trophies ------------------------------------------------------------------------

void Progress::award(TrophyId id, Reward& reward) {
    bool& held = trophies_[static_cast<std::size_t>(id)];
    if (held) return;
    held = true;
    reward.trophies.push_back(id);
}

void Progress::check_profile_trophies(Reward& reward) {
    if (is_won("e1-l01", Difficulty::Normal) || is_won("e1-l01", Difficulty::Hard)) award(TrophyId::FirstFind, reward);
    if (std::all_of(totems_fired_.begin(), totems_fired_.end(), [](bool b) { return b; }))
        award(TrophyId::TotemKeeper, reward);
    if (std::all_of(kBonusLevels.begin(), kBonusLevels.end(),
                    [this](const BonusLevel& b) { return bonus_result(b.id) == BonusResult::Passed; }))
        award(TrophyId::SharpEye, reward);
    if (is_won("e5-l12", Difficulty::Normal)) award(TrophyId::Explorer, reward);
    if (is_won("e5-l12", Difficulty::Hard)) award(TrophyId::Legend, reward);
}

// --- Save file -------------------------------------------------------------------------

std::string Progress::to_json() const {
    json j = json::object();
    j["version"] = kSaveVersion;
    j["stars"] = stars_;
    j["star_sand"] = star_sand_;
    json levels = json::object();
    for (std::size_t d = 0; d < 2; ++d) {
        json m = json::object();
        for (const auto& [id, rec] : levels_[d]) {
            json r = json::object();
            r["won"] = rec.won;
            if (rec.best_score) r["best_score"] = *rec.best_score;
            m[id] = std::move(r);
        }
        levels[std::string(kDifficultyNames[d])] = std::move(m);
    }
    j["levels"] = std::move(levels);
    json bonus = json::object();
    for (const auto& [id, r] : bonus_) bonus[id] = std::string(bonus_result_name(r));
    j["bonus"] = std::move(bonus);
    json up = json::object();
    up["timer"] = timer_bought_;
    up["gems"] = gems_bought_;
    up["frenzy"] = frenzy_bought_;
    json totems = json::object();
    for (std::size_t i = 0; i < kTotemCount; ++i)
        if (totem_bought_[i] > 0) totems[std::string(totem_name(static_cast<TotemId>(i)))] = totem_bought_[i];
    up["totems"] = std::move(totems);
    json power_ups = json::object();
    for (std::size_t i = 0; i < kPowerUpCount; ++i)
        if (power_up_bought_[i] > 0)
            power_ups[std::string(power_up_name(static_cast<PowerUpId>(i)))] = power_up_bought_[i];
    up["power_ups"] = std::move(power_ups);
    j["upgrades"] = std::move(up);
    json fired = json::array();
    for (std::size_t i = 0; i < kTotemCount; ++i)
        if (totems_fired_[i]) fired.push_back(std::string(totem_name(static_cast<TotemId>(i))));
    j["totems_fired"] = std::move(fired);
    json trophies = json::array();
    for (std::size_t i = 0; i < kTrophyCount; ++i)
        if (trophies_[i]) trophies.push_back(std::string(trophy_name(static_cast<TrophyId>(i))));
    j["trophies"] = std::move(trophies);
    return j.dump();
}

ProgressLoadResult Progress::try_from_json(std::string_view text) noexcept {
    ProgressLoadResult result;
    try {
        if (text.find_first_not_of(" \t\r\n") == std::string_view::npos) {
            result.progress = Progress{};
            return result;
        }
        const json j = json::parse(text.begin(), text.end());
        only_keys(j, "save", {"version", "stars", "star_sand", "levels", "bonus", "upgrades", "totems_fired", "trophies"});
        const auto v = j.find("version");
        if (v == j.end()) fail("save.version: missing");
        if (int_in(*v, "save.version", 0, kIntMax) != kSaveVersion)
            fail("save.version: must be " + std::to_string(kSaveVersion));

        Progress p;
        p.stars_ = optional_int(j, "stars", "save", 0, kMaxStars);
        p.star_sand_ = optional_int(j, "star_sand", "save", 0, kStarSandPerStar - 1);

        if (const auto it = j.find("levels"); it != j.end()) {
            only_keys(*it, "save.levels", {"normal", "hard"});
            for (std::size_t d = 0; d < 2; ++d) {
                const auto m = it->find(kDifficultyNames[d]);
                if (m == it->end()) continue;
                const std::string path = "save.levels." + std::string(kDifficultyNames[d]);
                if (!m->is_object()) fail(path + ": must be an object");
                for (auto e = m->begin(); e != m->end(); ++e) {
                    const std::string lp = path + "." + e.key();
                    if (!campaign_index(e.key())) fail(lp + ": unknown level");
                    only_keys(*e, lp, {"won", "best_score"});
                    LevelRecord rec;
                    if (const auto w = e->find("won"); w != e->end()) {
                        if (!w->is_boolean()) fail(lp + ".won: must be true or false");
                        rec.won = w->get<bool>();
                    }
                    if (const auto s = e->find("best_score"); s != e->end())
                        rec.best_score = int_in(*s, lp + ".best_score", 0, kIntMax);
                    p.levels_[d][e.key()] = rec;
                }
            }
        }

        if (const auto it = j.find("bonus"); it != j.end()) {
            if (!it->is_object()) fail("save.bonus: must be an object");
            for (auto e = it->begin(); e != it->end(); ++e) {
                const std::string bp = "save.bonus." + e.key();
                if (!bonus_level(e.key())) fail(bp + ": unknown bonus level");
                if (!e->is_string()) fail(bp + ": must be passed or failed");
                const auto s = e->get<std::string>();
                if (s == "passed") p.bonus_[e.key()] = BonusResult::Passed;
                else if (s == "failed") p.bonus_[e.key()] = BonusResult::Failed;
                else fail(bp + ": must be passed or failed");
            }
        }

        if (const auto it = j.find("upgrades"); it != j.end()) {
            only_keys(*it, "save.upgrades", {"timer", "gems", "frenzy", "totems", "power_ups"});
            p.timer_bought_ = optional_int(*it, "timer", "save.upgrades", 0, max_tier(UpgradeItem::timer()));
            p.gems_bought_ = optional_int(*it, "gems", "save.upgrades", 0, max_tier(UpgradeItem::gems()));
            p.frenzy_bought_ = optional_int(*it, "frenzy", "save.upgrades", 0, max_tier(UpgradeItem::frenzy()));
            if (const auto t = it->find("totems"); t != it->end()) {
                if (!t->is_object()) fail("save.upgrades.totems: must be an object");
                for (auto e = t->begin(); e != t->end(); ++e) {
                    const auto id = totem_from_name(e.key());
                    if (!id) fail("save.upgrades.totems." + e.key() + ": unknown totem");
                    p.totem_bought_[static_cast<std::size_t>(*id)] =
                        int_in(*e, "save.upgrades.totems." + e.key(), 0, max_tier(UpgradeItem::totem(*id)));
                }
            }
            if (const auto t = it->find("power_ups"); t != it->end()) {
                if (!t->is_object()) fail("save.upgrades.power_ups: must be an object");
                for (auto e = t->begin(); e != t->end(); ++e) {
                    const auto id = power_up_from_name(e.key());
                    if (!id) fail("save.upgrades.power_ups." + e.key() + ": unknown power-up");
                    p.power_up_bought_[static_cast<std::size_t>(*id)] =
                        int_in(*e, "save.upgrades.power_ups." + e.key(), 0, max_tier(UpgradeItem::power_up(*id)));
                }
            }
        }

        if (const auto it = j.find("totems_fired"); it != j.end()) {
            if (!it->is_array()) fail("save.totems_fired: must be an array");
            for (const json& e : *it) {
                const auto id = e.is_string() ? totem_from_name(e.get<std::string>()) : std::nullopt;
                if (!id) fail("save.totems_fired: unknown totem " + e.dump());
                p.totems_fired_[static_cast<std::size_t>(*id)] = true;
            }
        }

        if (const auto it = j.find("trophies"); it != j.end()) {
            if (!it->is_array()) fail("save.trophies: must be an array");
            for (const json& e : *it) {
                const auto id = e.is_string() ? trophy_from_name(e.get<std::string>()) : std::nullopt;
                if (!id) fail("save.trophies: unknown trophy " + e.dump());
                p.trophies_[static_cast<std::size_t>(*id)] = true;
            }
        }
        result.progress = std::move(p);
    } catch (const std::exception& e) {
        result.progress.reset();
        result.error = e.what();
        if (result.error.empty()) result.error = "unreadable save";
    } catch (...) {
        result.progress.reset();
        result.error = "unreadable save";
    }
    return result;
}

}  // namespace montezuma
