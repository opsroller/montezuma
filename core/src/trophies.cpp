// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/trophies.hpp"

#include <algorithm>

namespace montezuma {

namespace {

constexpr std::array<std::string_view, kTrophyCount> kNames{
    "first_find", "chain_reaction", "demolition", "totem_keeper", "quick_hands",
    "frenzy",     "sharp_eye",      "explorer",   "legend",
};

std::size_t at(TrophyId id) { return static_cast<std::size_t>(id); }

int cleared(const std::vector<StepReport>& steps) {
    int n = 0;
    for (const StepReport& s : steps) n += static_cast<int>(s.cleared.size());
    return n;
}

}  // namespace

std::string_view trophy_name(TrophyId id) noexcept {
    const auto i = at(id);
    return i < kNames.size() ? kNames[i] : std::string_view{};
}

std::optional<TrophyId> trophy_from_name(std::string_view name) noexcept {
    for (std::size_t i = 0; i < kNames.size(); ++i)
        if (kNames[i] == name) return static_cast<TrophyId>(i);
    return std::nullopt;
}

void TrophyWatch::on_move(const MoveReport& report) {
    // rule: progression Trophies, chain_reaction: the swap's own steps only.
    const bool chain = std::any_of(report.steps.begin(), report.steps.end(),
                                   [](const StepReport& s) { return s.step >= kChainReactionStep; });
    if (chain) earned_[at(TrophyId::ChainReaction)] = true;

    // demolition: the move's steps and every totem's steps.
    int pieces = cleared(report.steps);
    for (const TotemFiring& f : report.totems_fired) {
        pieces += cleared(f.steps);
        totems_fired_[static_cast<std::size_t>(f.effect.totem)] = true;
    }
    if (pieces >= kDemolitionPieces) earned_[at(TrophyId::Demolition)] = true;

    // frenzy: FRZ-04 gives 1 star sand per gem collected during Frenzy, so
    // the sand gained is the Frenzy's gems. A new Frenzy starts at 0.
    if (report.frenzy_started) frenzy_gems_ = 0;
    frenzy_gems_ += report.star_sand_gained;
    if (frenzy_gems_ >= kFrenzyTrophyGems) earned_[at(TrophyId::Frenzy)] = true;
}

void TrophyWatch::on_tick(const TickReport& report) {
    if (report.frenzy_ended) frenzy_gems_ = 0;
}

LevelPlay TrophyWatch::play(const LevelSession& session) const {
    LevelPlay p;
    p.outcome = session.outcome();
    p.score = session.score();
    p.time_left = session.time_left();
    p.star_sand = session.star_sand();
    p.totems_fired = totems_fired_;
    for (std::size_t i = 0; i < kTrophyCount; ++i)
        if (earned_[i]) p.earned.push_back(static_cast<TrophyId>(i));
    return p;
}

}  // namespace montezuma
