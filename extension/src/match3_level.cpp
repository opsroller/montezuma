// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "match3_level.hpp"

#include <map>
#include <utility>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

int to_int(LevelState::Outcome outcome) {
    switch (outcome) {
        case LevelState::Outcome::Won: return Match3Level::WON;
        case LevelState::Outcome::Lost: return Match3Level::LOST;
        case LevelState::Outcome::Playing: break;
    }
    return Match3Level::PLAYING;
}

}  // namespace

void Match3Level::setup(const Dictionary& quotas, double time_limit) {
    state_.reset();
    ERR_FAIL_COND_MSG(quotas.is_empty(), "Match3Level needs at least one quota.");
    ERR_FAIL_COND_MSG(!(time_limit > 0.0), "Match3Level needs a positive time limit.");
    std::map<int, int> q;
    const Array keys = quotas.keys();
    for (int64_t i = 0; i < keys.size(); ++i) {
        const int kind = keys[i];
        const int count = quotas[keys[i]];
        ERR_FAIL_COND_MSG(count <= 0, "Match3Level quotas must be positive.");
        q[kind] = count;
    }
    // Validated above, so the throwing constructor cannot throw here.
    state_.emplace(std::move(q), time_limit);
    emit_signal("score_changed", 0);
    emit_signal("time_changed", time_limit);
}

void Match3Level::apply_move(const Dictionary& result) {
    ERR_FAIL_COND_MSG(!state_, "Call Match3Level.setup first.");
    if (result.is_empty()) return;
    // LevelState only reads the per-step counts and the matched kind.
    MoveResult move;
    move.matched_kind = result.get("matched_kind", Board::kEmpty);
    const Array steps = result.get("steps", Array());
    for (int64_t i = 0; i < steps.size(); ++i) {
        const Dictionary step = steps[i];
        std::map<int, int> counts;
        const Array kinds = step.keys();
        for (int64_t k = 0; k < kinds.size(); ++k) counts[int(kinds[k])] = int(step[kinds[k]]);
        move.steps.push_back(std::move(counts));
    }

    const std::map<int, int> quotas_before = state_->quotas();
    const int score_before = state_->score();
    const int streak_before = state_->streak();
    const int kind_before = state_->streak_kind();
    const int outcome_before = get_outcome();
    state_->apply_move(move);

    if (state_->score() != score_before) emit_signal("score_changed", state_->score());
    for (const auto& [kind, remaining] : state_->quotas())
        if (quotas_before.at(kind) != remaining) emit_signal("quota_changed", kind, remaining);
    if (state_->streak() != streak_before || state_->streak_kind() != kind_before)
        emit_signal("streak_changed", state_->streak_kind(), state_->streak());
    emit_finished_if_done(outcome_before);
}

void Match3Level::tick(double delta) {
    ERR_FAIL_COND_MSG(!state_, "Call Match3Level.setup first.");
    if (state_->outcome() != LevelState::Outcome::Playing) return;
    const int outcome_before = get_outcome();
    state_->tick(delta);
    emit_signal("time_changed", state_->time_left());
    emit_finished_if_done(outcome_before);
}

void Match3Level::emit_finished_if_done(int before) {
    const int now = get_outcome();
    if (before == PLAYING && now != PLAYING) emit_signal("level_finished", now, state_->score());
}

int Match3Level::get_outcome() const { return state_ ? to_int(state_->outcome()) : PLAYING; }
int Match3Level::get_score() const { return state_ ? state_->score() : 0; }
double Match3Level::get_time_left() const { return state_ ? state_->time_left() : 0.0; }
int Match3Level::get_streak_kind() const { return state_ ? state_->streak_kind() : Board::kEmpty; }
int Match3Level::get_streak() const { return state_ ? state_->streak() : 0; }

Dictionary Match3Level::get_quotas() const {
    Dictionary out;
    if (state_)
        for (const auto& [kind, remaining] : state_->quotas()) out[kind] = remaining;
    return out;
}

void Match3Level::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "quotas", "time_limit"), &Match3Level::setup);
    ClassDB::bind_method(D_METHOD("apply_move", "result"), &Match3Level::apply_move);
    ClassDB::bind_method(D_METHOD("tick", "delta"), &Match3Level::tick);
    ClassDB::bind_method(D_METHOD("is_ready"), &Match3Level::is_ready);
    ClassDB::bind_method(D_METHOD("get_outcome"), &Match3Level::get_outcome);
    ClassDB::bind_method(D_METHOD("get_score"), &Match3Level::get_score);
    ClassDB::bind_method(D_METHOD("get_time_left"), &Match3Level::get_time_left);
    ClassDB::bind_method(D_METHOD("get_quotas"), &Match3Level::get_quotas);
    ClassDB::bind_method(D_METHOD("get_streak_kind"), &Match3Level::get_streak_kind);
    ClassDB::bind_method(D_METHOD("get_streak"), &Match3Level::get_streak);

    BIND_ENUM_CONSTANT(PLAYING);
    BIND_ENUM_CONSTANT(WON);
    BIND_ENUM_CONSTANT(LOST);

    ADD_SIGNAL(MethodInfo("score_changed", PropertyInfo(Variant::INT, "score")));
    ADD_SIGNAL(MethodInfo("quota_changed", PropertyInfo(Variant::INT, "kind"), PropertyInfo(Variant::INT, "remaining")));
    ADD_SIGNAL(MethodInfo("time_changed", PropertyInfo(Variant::FLOAT, "seconds_left")));
    ADD_SIGNAL(MethodInfo("streak_changed", PropertyInfo(Variant::INT, "kind"), PropertyInfo(Variant::INT, "length")));
    ADD_SIGNAL(MethodInfo("level_finished", PropertyInfo(Variant::INT, "outcome"), PropertyInfo(Variant::INT, "score")));
}

}  // namespace montezuma::godot_binding
