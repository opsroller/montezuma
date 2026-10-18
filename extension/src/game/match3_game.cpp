// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "match3_game.hpp"

#include <algorithm>
#include <exception>
#include <string_view>
#include <utility>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include "../audio/game_audio.hpp"
#include "../board/board_view_3d.hpp"
#include "../board/feel.hpp"
#include "display_move.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Effects stages: each totem, then each power-up.
constexpr int kEffectStages = fx::kTotemCount + fx::kPowerUpCount;

// The last cascade cue; deeper steps repeat it (audio.md, When each cue plays).
constexpr int kMaxCascadeCue = 8;
// Self-test: a level this long without an end is a failure.
constexpr int kSelfTestMaxMoves = 500;

String str(std::string_view s) { return String::utf8(s.data(), static_cast<int64_t>(s.size())); }
Vector2i to_vec(Cell c) { return Vector2i(c.x, c.y); }

int to_int(montezuma::Outcome o) {
    switch (o) {
        case montezuma::Outcome::Won: return Match3Game::WON;
        case montezuma::Outcome::Lost: return Match3Game::LOST;
        case montezuma::Outcome::Playing: break;
    }
    return Match3Game::PLAYING;
}

Array to_array(const std::vector<std::string>& xs) {
    Array out;
    for (const std::string& x : xs) out.push_back(str(x));
    return out;
}

// "e1-l03" -> "e1/l03.json"; empty for anything not shaped like an id.
String level_file(const String& id) {
    const PackedStringArray parts = id.split("-");
    if (parts.size() != 2) return String();
    const String episode = parts[0];
    const String level = parts[1];
    if (episode.length() < 2 || level.length() < 2 || episode[0] != 'e' || level[0] != 'l') return String();
    if (!episode.substr(1).is_valid_int() || !level.substr(1).is_valid_int()) return String();
    return episode + String("/") + level + String(".json");
}

Vector2i to_vec(anim::Cell c) { return Vector2i(c.x, c.y); }

Array to_array(const std::vector<anim::Cell>& cells) {
    Array out;
    for (const anim::Cell& c : cells) out.push_back(to_vec(c));
    return out;
}

String power_up_id(int id) { return id >= 0 && id < fx::kPowerUpCount ? str(fx::kPowerUpNames[static_cast<std::size_t>(id)]) : String(); }

Array to_array(const std::vector<fx::Placement>& placements) {
    Array out;
    for (const fx::Placement& p : placements) {
        Dictionary d;
        d["cell"] = to_vec(p.cell);
        d["id"] = power_up_id(p.id);
        out.push_back(d);
    }
    return out;
}

Array to_array(const std::vector<anim::Repaint>& repaints) {
    Array out;
    for (const anim::Repaint& r : repaints) {
        Dictionary d;
        d["cell"] = to_vec(r.cell);
        d["kind"] = r.kind;
        out.push_back(d);
    }
    return out;
}

// One display step's effects, in the keys BoardView3D reads (board-view.md,
// Effects).
Dictionary fx_dictionary(const fx::StepFx& f) {
    Dictionary d;
    Array power_ups;
    for (const fx::PowerUpGoOff& p : f.power_ups) {
        Dictionary g;
        g["cell"] = to_vec(p.cell);
        g["id"] = power_up_id(p.id);
        g["cells"] = to_array(p.cells);
        g["time"] = p.time_seconds;
        g["multiplier"] = p.multiplier;
        g["hint"] = p.hint_seconds;
        power_ups.push_back(g);
    }
    d["power_ups"] = power_ups;
    d["power_ups_placed"] = to_array(f.power_ups_placed);
    d["gems_collected"] = to_array(f.gems_collected);
    d["gems_placed"] = to_array(f.gems_placed);
    d["steel_spawned"] = to_array(f.steel_spawned);
    d["steel_removed"] = to_array(f.steel_removed);
    d["chains_broken"] = to_array(f.chains_broken);
    if (f.totem) {
        const fx::TotemFire& t = *f.totem;
        Dictionary td;
        td["id"] = str(fx::kTotemNames[static_cast<std::size_t>(t.id)]);
        td["targets"] = to_array(t.targets);
        td["repaints"] = to_array(t.repaints);
        td["time"] = t.time_seconds;
        td["points"] = t.points;
        td["gems_harvested"] = to_array(t.gems_harvested);
        td["gems_placed"] = to_array(t.gems_placed);
        td["chains_removed"] = to_array(t.chains_removed);
        td["power_ups_placed"] = to_array(t.power_ups_placed);
        d["totem"] = td;
    }
    return d;
}

// The display move's steps as the result's `cascades`.
Array cascades_array(const DisplayMove& dm) {
    Array out;
    for (std::size_t i = 0; i < dm.events.steps.size(); ++i) {
        const anim::Step& s = dm.events.steps[i];
        Dictionary d;
        d["cleared"] = to_array(s.cleared);
        Array falls;
        for (const anim::Fall& f : s.falls) {
            Dictionary fd;
            fd["from"] = to_vec(f.from);
            fd["to"] = to_vec(f.to);
            fd["kind"] = f.kind;
            falls.push_back(fd);
        }
        d["falls"] = falls;
        Array spawns;
        for (const anim::Spawn& sp : s.spawns) {
            Dictionary sd;
            sd["to"] = to_vec(sp.to);
            sd["kind"] = sp.kind;
            sd["drop"] = sp.drop;
            spawns.push_back(sd);
        }
        d["spawns"] = spawns;
        if (s.lead > 0.0) d["lead"] = s.lead;
        if (!s.repaints.empty()) d["repaints"] = to_array(s.repaints);
        d["fx"] = fx_dictionary(dm.fx[i]);
        out.push_back(d);
    }
    return out;
}

PackedInt32Array marks_array(const LevelSession& s) {
    const std::vector<int> marks = session_marks(s);
    PackedInt32Array out;
    out.resize(static_cast<int64_t>(marks.size()));
    for (std::size_t i = 0; i < marks.size(); ++i) out[static_cast<int64_t>(i)] = marks[i];
    return out;
}

void step_cues(const StepReport& step, int index, std::vector<String>& out) {
    out.push_back(index == 0 ? String("match") : "cascade_" + String::num_int64(std::min(index, kMaxCascadeCue)));
    if (!step.gems_collected.empty()) out.push_back("gem_collect");
    for (const TriggeredPowerUp& p : step.power_ups_triggered) out.push_back("bonus_use_" + str(power_up_name(p.power_up.id)));
    if (!step.steel_removed.empty()) out.push_back("steel_break");
    if (!step.chains_broken.empty()) out.push_back("chain_break");
    if (!step.power_ups_placed.empty()) out.push_back("bonus_earned");
}

}  // namespace

Match3Game::Match3Game() = default;
Match3Game::~Match3Game() = default;

BoardView3D* Match3Game::view() const {
    if (board_view_path_.is_empty() || !is_inside_tree()) return nullptr;
    return Object::cast_to<BoardView3D>(get_node_or_null(board_view_path_));
}

void Match3Game::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    set_process(session_ != nullptr);
    board_.instantiate();
    connect_all();
    if (OS::get_singleton()->get_cmdline_user_args().has("--game-selftest")) {
        self_test_ = true;
        callable_mp(this, &Match3Game::run_self_test).call_deferred();
    } else if (!autostart_level_.is_empty()) {
        if (start_level(autostart_level_, autostart_difficulty_) && effects_showcase_)
            callable_mp(this, &Match3Game::next_showcase_stage).call_deferred();
    }
}

void Match3Game::_enter_tree() {
    // _ready runs once; a node moved to a new parent reconnects here.
    if (!Engine::get_singleton()->is_editor_hint() && is_node_ready()) connect_all();
}

void Match3Game::connect_all() {
    if (BoardView3D* v = view()) {
        const Callable swap = callable_mp(this, &Match3Game::on_swap_requested);
        const Callable step = callable_mp(this, &Match3Game::on_cascade_step);
        const Callable done = callable_mp(this, &Match3Game::on_animation_finished);
        if (!v->is_connected("swap_requested", swap)) v->connect("swap_requested", swap);
        if (!v->is_connected("cascade_step", step)) v->connect("cascade_step", step);
        if (!v->is_connected("animation_finished", done)) v->connect("animation_finished", done);
    } else {
        UtilityFunctions::push_error("Match3Game needs board_view_path to point at a BoardView3D.");
    }
    connect_ui();
}

void Match3Game::_exit_tree() {
    disconnect_ui();
    BoardView3D* v = view();
    if (!v) return;
    const Callable swap = callable_mp(this, &Match3Game::on_swap_requested);
    const Callable step = callable_mp(this, &Match3Game::on_cascade_step);
    const Callable done = callable_mp(this, &Match3Game::on_animation_finished);
    if (v->is_connected("swap_requested", swap)) v->disconnect("swap_requested", swap);
    if (v->is_connected("cascade_step", step)) v->disconnect("cascade_step", step);
    if (v->is_connected("animation_finished", done)) v->disconnect("animation_finished", done);
}

void Match3Game::connect_ui() {
    Node* ui = get_tree()->get_root()->get_node_or_null(NodePath("UiRoot"));
    if (!ui) return;
    const Callable play = callable_mp(this, &Match3Game::on_play_requested);
    const Callable quit = callable_mp(this, &Match3Game::on_level_quit_requested);
    if (ui->has_signal("play_requested") && !ui->is_connected("play_requested", play)) ui->connect("play_requested", play);
    if (ui->has_signal("level_quit_requested") && !ui->is_connected("level_quit_requested", quit))
        ui->connect("level_quit_requested", quit);
    const Callable settings = callable_mp(this, &Match3Game::on_settings_changed);
    if (ui->has_signal("settings_changed") && !ui->is_connected("settings_changed", settings))
        ui->connect("settings_changed", settings);
    apply_reduced_motion();
}

void Match3Game::disconnect_ui() {
    if (!is_inside_tree()) return;
    Node* ui = get_tree()->get_root()->get_node_or_null(NodePath("UiRoot"));
    if (!ui) return;
    const Callable play = callable_mp(this, &Match3Game::on_play_requested);
    const Callable quit = callable_mp(this, &Match3Game::on_level_quit_requested);
    if (ui->is_connected("play_requested", play)) ui->disconnect("play_requested", play);
    if (ui->is_connected("level_quit_requested", quit)) ui->disconnect("level_quit_requested", quit);
    const Callable settings = callable_mp(this, &Match3Game::on_settings_changed);
    if (ui->is_connected("settings_changed", settings)) ui->disconnect("settings_changed", settings);
}

void Match3Game::on_settings_changed(const String& key) {
    if (key == "reduced_motion") apply_reduced_motion();
}

void Match3Game::apply_reduced_motion() {
    Node* ui = get_tree()->get_root()->get_node_or_null(NodePath("UiRoot"));
    BoardView3D* v = view();
    if (!ui || !v || !ui->has_method("get_setting")) return;
    v->set_animation_speed(feel::animation_speed_for(bool(ui->call("get_setting", "reduced_motion"))));
}

void Match3Game::on_play_requested(const String& level_id, const String& difficulty) {
    if (start_level(level_id, difficulty)) return;
    // Back to the map rather than an empty board.
    if (Node* ui = get_tree()->get_root()->get_node_or_null(NodePath("UiRoot")))
        ui->call("perform", "quit_level", String(), Dictionary());
}

void Match3Game::on_level_quit_requested() { stop_level(); }

// ---- Commands ---------------------------------------------------------------

bool Match3Game::start_level(const String& level_id, const String& difficulty) {
    const String file = level_file(level_id);
    ERR_FAIL_COND_V_MSG(file.is_empty(), false, "Match3Game: level id must look like e1-l01, got '" + level_id + "'.");
    const std::optional<Difficulty> diff = difficulty_from_name(difficulty.utf8().get_data());
    ERR_FAIL_COND_V_MSG(!diff, false, "Match3Game: difficulty must be normal or hard, got '" + difficulty + "'.");
    const String path = levels_dir_.path_join(file);
    ERR_FAIL_COND_V_MSG(!FileAccess::file_exists(path), false, "Match3Game: no level file " + path + ".");
    const CharString text = FileAccess::get_file_as_string(path).utf8();
    const LevelParseResult parsed = LevelData::parse(std::string_view(text.get_data(), static_cast<size_t>(text.length())));
    ERR_FAIL_COND_V_MSG(!parsed.ok(), false, "Match3Game: " + path + ":\n" + str(parsed.error_text()));
    ERR_FAIL_COND_V_MSG(str(parsed.level->id) != level_id, false,
                        "Match3Game: " + path + " has id '" + str(parsed.level->id) + "'.");

    std::unique_ptr<LevelSession> session;
    try {
        // AMBIGUOUS until progress (WP-08): every upgrade at tier 0, and the
        // level's own seed, so a level deals the same board each time.
        session = std::make_unique<LevelSession>(*parsed.level, *diff, Upgrades{}, parsed.level->seed);
    } catch (const std::exception& e) {
        ERR_FAIL_V_MSG(false, "Match3Game: cannot start " + level_id + ": " + String::utf8(e.what()));
    }

    session_ = std::move(session);
    level_id_ = level_id;
    difficulty_ = difficulty;
    shown_ = Shown{};
    for (std::size_t i = 0; i < kTotemCount; ++i) totem_charge_[i] = session_->totems().charge(static_cast<TotemId>(i));
    step_cues_.clear();
    finish_pending_ = false;
    finished_ = false;
    warned_ = false;
    board_->set_core_board(session_->board());
    hint_left_ = 0.0;
    pending_hint_.reset();
    if (BoardView3D* v = view()) {
        v->skip_animation();
        v->show_board(board_);
        v->show_marks(marks_array(*session_));
    }
    send("level_started", level_dictionary(), difficulty_);
    publish(true);
    send("time_changed", session_->time_left());
    cue("music_play_" + String::num_int64(session_->level().episode));
    cue("ambience_jungle");
    set_process(true);
    return true;
}

void Match3Game::stop_level() {
    if (!session_) return;
    session_.reset();
    level_id_ = String();
    step_cues_.clear();
    finish_pending_ = false;
    set_process(false);
    if (BoardView3D* v = view()) v->skip_animation();
    if (!self_test_)
        if (GameAudio* audio = GameAudio::get_singleton()) audio->stop("ambience_jungle");
}

bool Match3Game::request_swap(Vector2i a, Vector2i b) {
    BoardView3D* v = view();
    if (!session_ || session_->outcome() != montezuma::Outcome::Playing || (v && v->is_busy())) return false;
    std::optional<MoveReport> report;
    try {
        report = session_->swap({a.x, a.y}, {b.x, b.y});
    } catch (const std::exception& e) {
        ERR_FAIL_V_MSG(false, "Match3Game: swap failed: " + String::utf8(e.what()));
    }
    if (!report) {
        cue("swap_illegal");
        if (v) v->play_illegal(a, b);
        return false;
    }

    // One animation: the swap's cascades, then each fired totem's (one that
    // clears nothing gets an empty step for its effect), then the automatic
    // reshuffle, with what each step set off. The view ends on the
    // session's board and markers either way.
    const DisplayMove dm = display_move(*report);
    Dictionary result = Match3Board::to_dictionary(report->result);
    result["cascades"] = cascades_array(dm);
    result["marks"] = marks_array(*session_);
    step_cues_.clear();
    for (std::size_t i = 0; i < report->steps.size(); ++i) {
        step_cues_.emplace_back();
        step_cues(report->steps[i], static_cast<int>(i), step_cues_.back());
    }
    cue("swap");
    for (const TotemFiring& firing : report->totems_fired) {
        const String fire = "totem_fire_" + str(totem_name(firing.effect.totem));
        // The same steps display_move() lays out: one per cascade.
        if (!firing.result || firing.result->cascades.empty()) {
            step_cues_.push_back({fire});  // its own empty step: the effect alone
            continue;
        }
        for (std::size_t i = 0; i < firing.result->cascades.size(); ++i) {
            step_cues_.emplace_back();
            if (i == 0) step_cues_.back().push_back(fire);
            if (i < firing.steps.size())
                step_cues(firing.steps[i], static_cast<int>(step_cues_.size() - 1), step_cues_.back());
        }
    }
    if (report->auto_shuffle && report->auto_shuffle->shuffled) {
        const ShuffleReport& sh = *report->auto_shuffle;
        result["shuffled"] = true;
        result["redealt"] = sh.redealt;
        Array map;
        for (const auto& [from, to] : sh.shuffle_map) {
            Array pair;
            pair.push_back(to_vec(from));
            pair.push_back(to_vec(to));
            map.push_back(pair);
        }
        result["shuffle_map"] = map;
    }
    // PWR-11: the hint power-up shows every legal swap for a while; a new
    // one restarts it. The view shows it once this move has played.
    if (report->hint_seconds > 0.0) hint_left_ = report->hint_seconds;

    // Each display step has its cues; a gap means the report and the board
    // result disagree, which the self-test fails on.
    if (static_cast<int64_t>(step_cues_.size()) != Array(result["cascades"]).size()) {
        ++step_mismatches_;
        ERR_PRINT("Match3Game: report steps do not line up with the animation's steps.");
    }
    // totem_charge: at most once per move, when any totem gains charge
    // without firing (a firing has its own cue).
    bool charged = false;
    for (const TotemCharge& c : report->totem_charges) {
        const auto i = static_cast<std::size_t>(c.id);
        charged = charged || (c.charge > totem_charge_[i] && c.charge < 1.0);
        totem_charge_[i] = c.charge;
    }
    if (charged) cue("totem_charge");
    const std::map<int, int> quotas_before = shown_.quotas;
    play_board(result);
    publish(false);
    for (const auto& [kind, remaining] : shown_.quotas) {
        const auto before = quotas_before.find(kind);
        if (remaining == 0 && before != quotas_before.end() && before->second > 0) cue("quota_done");
    }
    if (report->time_delta_seconds != 0.0) send("time_changed", session_->time_left());
    after_command();
    return true;
}

void Match3Game::shuffle() {
    BoardView3D* v = view();
    if (!session_ || (v && v->is_busy())) return;
    std::optional<ShuffleReport> sh;
    try {
        sh = session_->shuffle();
    } catch (const std::exception& e) {
        ERR_FAIL_MSG("Match3Game: shuffle failed: " + String::utf8(e.what()));
    }
    if (!sh || !sh->shuffled) return;
    MoveResult as_result;
    as_result.shuffled = true;
    as_result.redealt = sh->redealt;
    as_result.shuffle_map = sh->shuffle_map;
    step_cues_.clear();
    Dictionary result = Match3Board::to_dictionary(as_result);
    result["marks"] = marks_array(*session_);
    play_board(result);
    publish(false);
    after_command();
}

void Match3Game::buy_hint() {
    if (!session_) return;
    std::optional<Swap> hint;
    try {
        hint = session_->buy_hint();
    } catch (const std::exception& e) {
        ERR_FAIL_MSG("Match3Game: buy_hint failed: " + String::utf8(e.what()));
    }
    if (!hint) return;
    show_hint(*hint);
    publish(false);
}

void Match3Game::_process(double delta) {
    if (!session_ || session_->outcome() != montezuma::Outcome::Playing) return;
    std::optional<TickReport> t;
    try {
        t = session_->tick(std::max(0.0, delta));
    } catch (const std::exception& e) {
        ERR_FAIL_MSG("Match3Game: tick failed: " + String::utf8(e.what()));
    }
    if (!t) return;
    send("time_changed", t->time_left);
    const bool low = t->time_left > 0.0 && t->time_left <= warning_seconds_;
    if (low && !warned_) cue("time_warning");
    warned_ = low;
    if (t->free_hint) show_hint(*t->free_hint);
    if (hint_left_ > 0.0) hint_left_ = std::max(0.0, hint_left_ - delta);
    publish(false);
    if (t->outcome_changed) finish_if_over();
}

// ---- Presentation -------------------------------------------------------------

void Match3Game::play_board(const Dictionary& result) {
    board_->set_core_board(session_->board());
    if (BoardView3D* v = view()) v->play(result);
}

void Match3Game::after_command() {
    if (session_->outcome() != montezuma::Outcome::Playing) finish_if_over();
}

void Match3Game::finish_if_over() {
    if (finished_ || !session_ || session_->outcome() == montezuma::Outcome::Playing) return;
    BoardView3D* v = view();
    // Let the winning (or last) move finish on screen first.
    if (v && v->is_busy()) {
        finish_pending_ = true;
        return;
    }
    emit_finished();
}

void Match3Game::emit_finished() {
    finish_pending_ = false;
    finished_ = true;
    set_process(false);
    const int outcome = to_int(session_->outcome());
    cue(outcome == WON ? "level_won" : "level_lost");
    send("level_finished", outcome, session_->score());
}

void Match3Game::on_swap_requested(Vector2i a, Vector2i b) { request_swap(a, b); }

void Match3Game::on_cascade_step(int step) {
    if (step < 0 || static_cast<std::size_t>(step) >= step_cues_.size()) return;
    for (const String& name : step_cues_[static_cast<std::size_t>(step)]) cue(name);
}

void Match3Game::on_animation_finished() {
    step_cues_.clear();
    if (finish_pending_ && session_) emit_finished();
    // The hint power-up's time is still running: show the new board's swaps.
    if (hint_left_ > 0.0 && is_playing()) {
        if (BoardView3D* v = view()) v->show_hints(get_legal_moves(), hint_left_);
    } else if (pending_hint_ && is_playing()) {
        // A free hint that arrived mid-move rings once the board settles.
        show_hint(*pending_hint_);
    }
    pending_hint_.reset();
    if (showcase_running_) advance_showcase();
}

void Match3Game::show_hint(const Swap& hint) {
    if (!pending_hint_ || *pending_hint_ != hint) send("hint_shown", to_vec(hint[0]), to_vec(hint[1]));
    BoardView3D* v = view();
    if (!v) return;
    if (v->is_busy()) {
        pending_hint_ = hint;
        return;
    }
    pending_hint_.reset();
    Array pair;
    pair.push_back(to_vec(hint[0]));
    pair.push_back(to_vec(hint[1]));
    Array pairs;
    pairs.push_back(pair);
    v->show_hints(pairs, 0.0);  // until the next move
}

void Match3Game::cue(const String& name) {
    if (self_test_) {
        cues_played_.push_back(name);
        return;
    }
    if (GameAudio* audio = GameAudio::get_singleton()) audio->play(name);
}

void Match3Game::publish(bool all) {
    if (!session_) return;
    const LevelSession& s = *session_;
    if (all || s.score() != shown_.score) {
        shown_.score = s.score();
        send("score_changed", shown_.score);
    }
    const int goal = s.gem_goal().value_or(0);
    if (all || s.gems_collected() != shown_.gems || goal != shown_.gem_goal) {
        shown_.gems = s.gems_collected();
        shown_.gem_goal = goal;
        send("gems_changed", shown_.gems, shown_.gem_goal);
    }
    for (const auto& [kind, needed] : s.quotas()) {
        const auto cleared = s.cleared_by_kind().find(kind);
        const int remaining = std::max(0, needed - (cleared == s.cleared_by_kind().end() ? 0 : cleared->second));
        const auto it = shown_.quotas.find(kind);
        if (all || it == shown_.quotas.end() || it->second != remaining) {
            shown_.quotas[kind] = remaining;
            send("quota_changed", kind, remaining);
        }
    }
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        const auto id = static_cast<TotemId>(i);
        if (!s.totems().unlocked(id)) continue;
        const double charge = totem_charge_[i];
        const int uses = s.totems().uses_left(id);
        if (all || charge != shown_.totem_charge[i] || uses != shown_.totem_uses[i]) {
            shown_.totem_charge[i] = charge;
            shown_.totem_uses[i] = uses;
            const String name = str(totem_name(id));
            send("totem_changed", name, charge, uses);
            send("totem_charge_changed", name, charge);
        }
    }
    // While Frenzy is on the bar drains with its time; otherwise it shows the fill.
    const double frenzy_seconds = kFrenzySeconds[static_cast<std::size_t>(std::clamp(s.frenzy_tier(), 0, kMaxFrenzyTier))];
    const double bar = s.frenzy_on() && frenzy_seconds > 0.0
                           ? s.frenzy_left() / frenzy_seconds
                           : static_cast<double>(s.frenzy_bar()) / static_cast<double>(kFrenzyBarMax);
    const double left = s.frenzy_on() ? s.frenzy_left() : 0.0;
    if (all || bar != shown_.frenzy_bar || left != shown_.frenzy_left) {
        shown_.frenzy_bar = bar;
        shown_.frenzy_left = left;
        send("frenzy_changed", std::clamp(bar, 0.0, 1.0), left);
    }
    if (all || s.manual_shuffles_left() != shown_.shuffles) {
        shown_.shuffles = s.manual_shuffles_left();
        send("shuffles_changed", shown_.shuffles);
    }
    // A hint already shown is given again at no cost (SHUF-04).
    const bool affordable = s.outcome() == montezuma::Outcome::Playing && (s.hint().has_value() || s.score() >= kHintCost);
    if (all || kHintCost != shown_.hint_cost || affordable != shown_.hint_affordable) {
        shown_.hint_cost = kHintCost;
        shown_.hint_affordable = affordable;
        send("hint_cost_changed", kHintCost, affordable);
    }
}

Dictionary Match3Game::level_dictionary() const {
    const LevelData& l = session_->level();
    Dictionary d;
    d["id"] = str(l.id);
    d["episode"] = l.episode;
    d["index"] = l.index;
    Dictionary board;
    board["width"] = l.board.width;
    board["height"] = l.board.height;
    board["kinds"] = l.board.kinds;
    d["board"] = board;
    if (l.gem_goal) {
        Dictionary goal;
        goal["normal"] = l.gem_goal->normal;
        goal["hard"] = l.gem_goal->hard;
        d["gem_goal"] = goal;
    }
    if (!l.quotas.empty()) {
        Dictionary quotas;
        for (const auto& [kind, count] : l.quotas)
            if (kind >= 0 && static_cast<std::size_t>(kind) < kKindNames.size()) quotas[str(kKindNames[static_cast<std::size_t>(kind)])] = count;
        d["quotas"] = quotas;
    }
    // The HUD reads the time it starts from; this is the session's, after
    // the timer tier (GOAL-03), on both keys' own difficulty.
    Dictionary time;
    time["normal"] = l.time_limit ? l.time_limit->normal : session_->time_left();
    time["hard"] = l.time_limit ? l.time_limit->hard : session_->time_left();
    time[difficulty_] = session_->time_left();
    d["time_limit"] = time;
    d["totems_unlocked"] = to_array(l.totems_unlocked);
    d["powerups_unlocked"] = to_array(l.powerups_unlocked);
    d["steel_rate"] = l.steel_rate;
    d["rules"] = str(l.rules);
    d["theme"] = str(l.theme);
    d["seed"] = static_cast<int64_t>(l.seed);
    return d;
}

// ---- Queries -------------------------------------------------------------------

bool Match3Game::is_playing() const { return session_ && session_->outcome() == montezuma::Outcome::Playing; }
String Match3Game::get_level_id() const { return level_id_; }
int Match3Game::get_outcome() const { return session_ ? to_int(session_->outcome()) : PLAYING; }
int Match3Game::get_score() const { return session_ ? session_->score() : 0; }
double Match3Game::get_time_left() const { return session_ ? session_->time_left() : 0.0; }
int Match3Game::get_gems_collected() const { return session_ ? session_->gems_collected() : 0; }
int Match3Game::get_gem_goal() const { return session_ ? session_->gem_goal().value_or(0) : 0; }
int Match3Game::get_moves() const { return session_ ? session_->moves() : 0; }

Array Match3Game::get_legal_moves() const {
    Array out;
    if (!session_) return out;
    for (const Swap& m : session_->legal_moves()) {
        Array pair;
        pair.push_back(to_vec(m[0]));
        pair.push_back(to_vec(m[1]));
        out.push_back(pair);
    }
    return out;
}

// ---- Effects stages (self-test and look-dev showcase) -------------------------

bool Match3Game::run_effect_stage(int stage) {
    BoardView3D* v = view();
    if (!session_ || !is_playing() || (v && v->is_busy()) || stage < 0 || stage >= kEffectStages) return false;
    LevelSession& s = *session_;
    s.set_gems_collected(0);  // keep the level going through every stage
    const int w = s.board().width();
    const int h = s.board().height();
    auto plain = [&](Cell c) {
        return !s.gem(c) && !s.steel(c) && !s.chained(c) && !s.power_ups().has(c) && s.board().at(c.x, c.y) >= 0;
    };
    auto used = [](const Swap& m, const Run& run, Cell c) {
        if (c == m[0] || c == m[1]) return true;
        return std::find(run.cells.begin(), run.cells.end(), c) != run.cells.end();
    };
    for (const Swap& m : s.legal_moves()) {
        for (const Run& run : s.runs_for_swap(m[0], m[1])) {
            if (stage < fx::kTotemCount) {
                const auto id = static_cast<TotemId>(stage);
                if (run.kind != totem_kind(id)) continue;
                TotemSystem& totems = s.totems_for_setup();
                totems.unlock(id, 3);  // tier 3: every totem has it (ward's highest)
                // Charged once, as if the last move matched this colour too.
                MoveResult charge;
                charge.swap = m;
                charge.cascades.emplace_back();
                charge.cascades.back().runs.push_back(run);
                totems.record_move(charge);
                // Something for ward to break and harvest to unchain.
                int staged = 0;
                for (int i = 0; i < w * h && staged < 3; i += 7) {
                    const Cell c{i % w, i / w};
                    if (used(m, run, c) || !plain(c)) continue;
                    if (id == TotemId::Ward) s.set_steel(c, true);
                    if (id == TotemId::Harvest) s.set_chain(c, true);
                    ++staged;
                }
                totem_charge_[static_cast<std::size_t>(stage)] = totems.charge(id);
            } else {
                Cell at{-1, -1};
                for (const Cell& c : run.cells)
                    if (c != m[0] && c != m[1] && plain(c)) at = c;
                if (at.x < 0) continue;
                s.set_power_up(at, PowerUp{static_cast<PowerUpId>(stage - fx::kTotemCount), 3});
            }
            if (v) v->show_marks(marks_array(s));
            return request_swap(Vector2i(m[0].x, m[0].y), Vector2i(m[1].x, m[1].y));
        }
    }
    return false;
}

void Match3Game::next_showcase_stage() {
    if (!effects_showcase_ || !is_inside_tree()) return;
    if (!is_playing()) {
        if (!start_level(autostart_level_.is_empty() ? String("e1-l01") : autostart_level_, autostart_difficulty_)) return;
    }
    showcase_running_ = true;
    if (run_effect_stage(showcase_stage_)) {
        showcase_stage_ = (showcase_stage_ + 1) % kEffectStages;
        return;
    }
    // No move of the colour this stage needs: play another and try again.
    const Array legal = get_legal_moves();
    if (!legal.is_empty()) {
        const Array m = legal[0];
        if (request_swap(m[0], m[1])) return;
    }
    // Nothing played, so no animation_finished will follow: try again later.
    advance_showcase();
}

void Match3Game::advance_showcase() {
    // A pause between stages, so each effect reads on its own.
    showcase_running_ = false;
    get_tree()->create_timer(0.6)->connect("timeout", callable_mp(this, &Match3Game::next_showcase_stage),
                                           Object::CONNECT_ONE_SHOT);
}

// ---- Self-test -----------------------------------------------------------------

void Match3Game::run_self_test() {
    // Started with `-- --game-selftest`: play the intro level to its end
    // through the same paths the view and HUD use, count the signals, and
    // check every cue against the cue table.
    int failures = 0;
    auto fail = [&failures](const String& what) {
        UtilityFunctions::push_error("Match3Game self-test: ", what);
        ++failures;
    };
    signal_counts_.clear();
    step_mismatches_ = 0;
    if (start_level("e1-l99", "normal")) fail("a missing level started");
    if (start_level("nonsense", "normal")) fail("a bad id started");
    if (!start_level("e1-l01", "normal")) fail("e1-l01 did not start");
    BoardView3D* v = view();
    if (!v) fail("no board view");
    int moves = 0;
    int first_outcome = PLAYING;
    if (session_) {
        if (get_gem_goal() != 10) fail("e1-l01 normal goal is not 10 gems");
        shuffle();
        if (v) v->skip_animation();
        if (session_->manual_shuffles_left() != kManualShufflesPerLevel - 1) fail("shuffle() did not use a shuffle");
        if (request_swap(Vector2i(0, 0), Vector2i(5, 5))) fail("a far swap was legal");
        if (v) v->skip_animation();
        while (is_playing() && moves < kSelfTestMaxMoves) {
            const Array legal = get_legal_moves();
            if (legal.is_empty()) break;
            const Array m = legal[0];
            if (!request_swap(m[0], m[1])) {
                fail("a listed legal move was refused");
                break;
            }
            ++moves;
            if (v) v->skip_animation();
            // The view must end on the session's board after every move.
            for (int y = 0; y < board_->get_height(); ++y)
                for (int x = 0; x < board_->get_width(); ++x)
                    if (board_->get_cell(x, y) != session_->board().at(x, y)) {
                        fail("mirror board differs from the session");
                        y = board_->get_height();
                        break;
                    }
        }
        if (is_playing()) fail("e1-l01 did not end");
        first_outcome = get_outcome();
        if (first_outcome != WON) fail("e1-l01 played by the first legal move did not win");
        _process(1.0);  // no ticks after the end
    }

    // Effects (WP-29): fire every totem and set off every power-up through
    // the real session; the view must schedule each one's beat and end with
    // the session's markers.
    int effect_moves = 0;
    if (start_level("e1-l01", "normal") && v) {
        for (int stage = 0; stage < kEffectStages; ++stage) {
            bool played = false;
            for (int attempt = 0; attempt < 40 && !played && is_playing(); ++attempt) {
                played = run_effect_stage(stage);
                if (!played) {
                    const Array legal = get_legal_moves();
                    if (legal.is_empty()) break;
                    const Array m = legal[static_cast<int64_t>(attempt) % legal.size()];
                    request_swap(m[0], m[1]);
                }
                v->skip_animation();
                ++effect_moves;
            }
            const String name = stage < fx::kTotemCount
                                    ? "totem_" + str(totem_name(static_cast<TotemId>(stage)))
                                    : "power_up_" + str(power_up_name(static_cast<PowerUpId>(stage - fx::kTotemCount)));
            if (!played) fail(String("could not stage ") + name);
            if (static_cast<int>(v->get_effects_played().get(name, 0)) < 1) fail(name + String(" played no effect"));
            if (session_ && v->get_marks() != marks_array(*session_)) fail(String("markers differ from the session after ") + name);
        }
        const Dictionary played = v->get_effects_played();
        for (const char* name : {"gem_collect", "steel_break", "chain_break", "mark_placed"})
            if (static_cast<int>(played.get(name, 0)) < 1) fail(String(name) + " never played");
        // Readability rule 3: every move's effects end within its plan, so
        // animation_finished (and input) is never held past plan.total.
        if (const double over = played.get("overrun_seconds", 0.0); over > 1e-6)
            fail("effects ran " + String::num(over, 3) + " s past their move's plan");
        stop_level();
    } else {
        fail("e1-l01 did not start again for the effects");
    }
    const int finished = signal_counts_["level_finished"];
    if (finished != 1) fail("level_finished fired " + String::num_int64(finished) + " times");
    for (const char* name : {"level_started", "score_changed", "gems_changed", "shuffles_changed", "hint_cost_changed"})
        if (signal_counts_[name] < 1) fail(String(name) + " never fired");
    if (step_mismatches_ > 0) fail("report steps did not line up with the animation");
    if (signal_counts_["gems_changed"] < 2) fail("gems_changed did not follow the gems");

    GameAudio* audio = GameAudio::get_singleton();
    if (!audio) fail("no GameAudio autoload");
    std::map<std::string, bool> checked;
    for (const String& name : cues_played_) {
        const std::string key = name.utf8().get_data();
        if (checked[key]) continue;
        checked[key] = true;
        if (audio && !audio->has_cue(name)) fail("cue '" + name + "' is not in the cue table");
    }
    UtilityFunctions::print("Match3Game effects: ", fx::kTotemCount, " totems and ", fx::kPowerUpCount,
                            " power-ups staged in ", effect_moves, " moves");
    UtilityFunctions::print("Match3Game self-test: ", moves, " moves, outcome ", first_outcome, ", ",
                            int64_t(checked.size()), " cues, ", failures, " failures");
}

// ---- Binding ---------------------------------------------------------------------

void Match3Game::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_level", "level_id", "difficulty"), &Match3Game::start_level);
    ClassDB::bind_method(D_METHOD("stop_level"), &Match3Game::stop_level);
    ClassDB::bind_method(D_METHOD("shuffle"), &Match3Game::shuffle);
    ClassDB::bind_method(D_METHOD("buy_hint"), &Match3Game::buy_hint);
    ClassDB::bind_method(D_METHOD("request_swap", "a", "b"), &Match3Game::request_swap);
    ClassDB::bind_method(D_METHOD("is_playing"), &Match3Game::is_playing);
    ClassDB::bind_method(D_METHOD("get_level_id"), &Match3Game::get_level_id);
    ClassDB::bind_method(D_METHOD("get_outcome"), &Match3Game::get_outcome);
    ClassDB::bind_method(D_METHOD("get_score"), &Match3Game::get_score);
    ClassDB::bind_method(D_METHOD("get_time_left"), &Match3Game::get_time_left);
    ClassDB::bind_method(D_METHOD("get_gems_collected"), &Match3Game::get_gems_collected);
    ClassDB::bind_method(D_METHOD("get_gem_goal"), &Match3Game::get_gem_goal);
    ClassDB::bind_method(D_METHOD("get_moves"), &Match3Game::get_moves);
    ClassDB::bind_method(D_METHOD("get_board"), &Match3Game::get_board);
    ClassDB::bind_method(D_METHOD("get_legal_moves"), &Match3Game::get_legal_moves);
    ClassDB::bind_method(D_METHOD("set_board_view_path", "path"), &Match3Game::set_board_view_path);
    ClassDB::bind_method(D_METHOD("get_board_view_path"), &Match3Game::get_board_view_path);
    ClassDB::bind_method(D_METHOD("set_levels_dir", "dir"), &Match3Game::set_levels_dir);
    ClassDB::bind_method(D_METHOD("get_levels_dir"), &Match3Game::get_levels_dir);
    ClassDB::bind_method(D_METHOD("set_autostart_level", "level_id"), &Match3Game::set_autostart_level);
    ClassDB::bind_method(D_METHOD("get_autostart_level"), &Match3Game::get_autostart_level);
    ClassDB::bind_method(D_METHOD("set_autostart_difficulty", "difficulty"), &Match3Game::set_autostart_difficulty);
    ClassDB::bind_method(D_METHOD("get_autostart_difficulty"), &Match3Game::get_autostart_difficulty);
    ClassDB::bind_method(D_METHOD("set_warning_seconds", "seconds"), &Match3Game::set_warning_seconds);
    ClassDB::bind_method(D_METHOD("get_warning_seconds"), &Match3Game::get_warning_seconds);
    ClassDB::bind_method(D_METHOD("set_effects_showcase", "on"), &Match3Game::set_effects_showcase);
    ClassDB::bind_method(D_METHOD("get_effects_showcase"), &Match3Game::get_effects_showcase);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "board_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BoardView3D"),
                 "set_board_view_path", "get_board_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "levels_dir", PROPERTY_HINT_DIR), "set_levels_dir", "get_levels_dir");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "autostart_level"), "set_autostart_level", "get_autostart_level");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "autostart_difficulty", PROPERTY_HINT_ENUM, "normal,hard"),
                 "set_autostart_difficulty", "get_autostart_difficulty");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "warning_seconds"), "set_warning_seconds", "get_warning_seconds");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "effects_showcase"), "set_effects_showcase", "get_effects_showcase");

    BIND_ENUM_CONSTANT(PLAYING);
    BIND_ENUM_CONSTANT(WON);
    BIND_ENUM_CONSTANT(LOST);

    ADD_SIGNAL(MethodInfo("level_started", PropertyInfo(Variant::DICTIONARY, "level"),
                          PropertyInfo(Variant::STRING, "difficulty")));
    ADD_SIGNAL(MethodInfo("time_changed", PropertyInfo(Variant::FLOAT, "seconds_left")));
    ADD_SIGNAL(MethodInfo("score_changed", PropertyInfo(Variant::INT, "score")));
    ADD_SIGNAL(MethodInfo("gems_changed", PropertyInfo(Variant::INT, "collected"), PropertyInfo(Variant::INT, "goal")));
    ADD_SIGNAL(MethodInfo("quota_changed", PropertyInfo(Variant::INT, "kind"), PropertyInfo(Variant::INT, "remaining")));
    ADD_SIGNAL(MethodInfo("totem_changed", PropertyInfo(Variant::STRING, "totem_id"),
                          PropertyInfo(Variant::FLOAT, "charge"), PropertyInfo(Variant::INT, "uses_left")));
    ADD_SIGNAL(MethodInfo("totem_charge_changed", PropertyInfo(Variant::STRING, "totem_id"),
                          PropertyInfo(Variant::FLOAT, "charge")));
    ADD_SIGNAL(MethodInfo("frenzy_changed", PropertyInfo(Variant::FLOAT, "bar"), PropertyInfo(Variant::FLOAT, "seconds_left")));
    ADD_SIGNAL(MethodInfo("shuffles_changed", PropertyInfo(Variant::INT, "left")));
    ADD_SIGNAL(MethodInfo("hint_cost_changed", PropertyInfo(Variant::INT, "cost"), PropertyInfo(Variant::BOOL, "affordable")));
    ADD_SIGNAL(MethodInfo("hint_shown", PropertyInfo(Variant::VECTOR2I, "a"), PropertyInfo(Variant::VECTOR2I, "b")));
    ADD_SIGNAL(MethodInfo("level_finished", PropertyInfo(Variant::INT, "outcome"), PropertyInfo(Variant::INT, "score")));
}

}  // namespace montezuma::godot_binding
