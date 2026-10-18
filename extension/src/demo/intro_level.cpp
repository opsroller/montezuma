// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "intro_level.hpp"

#include <string>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "board/board_view_3d.hpp"
#include "game/match3_game.hpp"
#include "handoff_script.hpp"
#include "ui/screen_stack.hpp"
#include "ui/ui_root.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr const char* kCompleteLevelArg = "--complete-level";
constexpr const char* kSelfTestArg = "--intro-selftest";
// The self-test gives up on a level that has not ended after this many moves.
constexpr int kSelfTestMaxMoves = 500;
// The self-test's other level: any level but the intro one.
constexpr const char* kSelfTestOtherLevel = "e1-l02";

std::string utf8(const String& s) {
    const CharString c = s.utf8();
    return std::string(c.get_data(), static_cast<std::size_t>(c.length()));
}

// Runs a hand-off script on the page; does nothing off the web.
void tell_page(const std::string& script) {
    if (!OS::get_singleton()->has_feature("web")) return;
    JavaScriptBridge::get_singleton()->eval(String::utf8(script.c_str()), false);
}

UiRoot* ui_root(const Node* from) {
    if (!from->is_inside_tree()) return nullptr;
    return Object::cast_to<UiRoot>(from->get_tree()->get_root()->get_node_or_null(NodePath("UiRoot")));
}

}  // namespace

Match3Game* IntroLevel::game() const {
    if (game_path_.is_empty() || !is_inside_tree()) return nullptr;
    return Object::cast_to<Match3Game>(get_node_or_null(game_path_));
}

int IntroLevel::get_score() const {
    if (completed_) return score_;
    const Match3Game* g = game();
    return g ? g->get_score() : 0;
}

void IntroLevel::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }
    const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
    complete_on_start_ = args.has(kCompleteLevelArg);
    Match3Game* g = game();
    ERR_FAIL_NULL_MSG(g, "IntroLevel needs game_path to point at a Match3Game.");
    // The game started its autostart level in its own _ready, before this one.
    level_id_ = g->get_autostart_level();
    ERR_FAIL_COND_MSG(level_id_.is_empty(), "IntroLevel: the game has no autostart_level.");
    connect_game(g);
    // The demo opens on the board: close the menu the UI starts with.
    if (UiRoot* ui = ui_root(this))
        if (ScreenStack* stack = ui->get_stack()) stack->clear();
    if (args.has(kSelfTestArg)) callable_mp(this, &IntroLevel::run_self_test).call_deferred();
}

void IntroLevel::connect_game(Match3Game* g) {
    const Callable started = callable_mp(this, &IntroLevel::on_level_started);
    const Callable finished = callable_mp(this, &IntroLevel::on_level_finished);
    if (!g->is_connected("level_started", started)) g->connect("level_started", started);
    if (!g->is_connected("level_finished", finished)) g->connect("level_finished", finished);
}

void IntroLevel::_process(double) {
    if (!first_frame_) return;
    first_frame_ = false;
    set_process(false);
    const String version = ProjectSettings::get_singleton()->get_setting("application/config/version", "");
    tell_page(demo::ready_script(utf8(version)));
    if (complete_on_start_) complete();
}

void IntroLevel::_exit_tree() {
    Match3Game* g = game();
    if (!g) return;
    const Callable started = callable_mp(this, &IntroLevel::on_level_started);
    const Callable finished = callable_mp(this, &IntroLevel::on_level_finished);
    if (g->is_connected("level_started", started)) g->disconnect("level_started", started);
    if (g->is_connected("level_finished", finished)) g->disconnect("level_finished", finished);
}

void IntroLevel::complete() { finish(get_score()); }

void IntroLevel::finish(int score) {
    // Nothing to win before the game is set up, and the page hears it once.
    if (completed_ || level_id_.is_empty()) return;
    completed_ = true;
    score_ = score;
    ++completions_;
    emit_signal("level_completed", level_id_, score_);
    tell_page(demo::level_completed_script(utf8(level_id_), score_));
}

void IntroLevel::on_level_started(const Variant&, const String&) {
    const Match3Game* g = game();
    // Deferred: the game is still inside start_level.
    if (g && g->get_level_id() != level_id_) callable_mp(this, &IntroLevel::back_to_intro).call_deferred();
}

void IntroLevel::back_to_intro() {
    Match3Game* g = game();
    if (!g || g->get_level_id() == level_id_) return;
    // Through the UI when there is one, so its retry button replays the intro level too.
    if (UiRoot* ui = ui_root(this)) {
        ui->perform("play", level_id_, Dictionary());
        if (g->get_level_id() == level_id_) return;
    }
    g->start_level(level_id_, g->get_autostart_difficulty());
}

void IntroLevel::on_level_finished(int outcome, int score) {
    const Match3Game* g = game();
    if (outcome == Match3Game::WON && g && g->get_level_id() == level_id_) finish(score);
}

// ---- Self-test -----------------------------------------------------------------

void IntroLevel::run_self_test() {
    // Started with `-- --intro-selftest`: play the intro level to a win
    // through the game, as a player would, then check the hand-off.
    auto fail = [this](const String& what) {
        UtilityFunctions::push_error("IntroLevel self-test: ", what);
        ++self_test_failures_;
    };
    Match3Game* g = game();
    if (!g) {
        fail("no Match3Game");
        finish_self_test();
        return;
    }
    BoardView3D* v = Object::cast_to<BoardView3D>(g->get_node_or_null(g->get_board_view_path()));
    if (!v) fail("no board view");
    if (level_id_ != "e1-l01") fail("the intro level is " + level_id_ + ", not e1-l01");
    if (!g->is_playing() || g->get_level_id() != level_id_) fail("the intro level did not start on load");
    if (g->get_gem_goal() <= 0) fail("the intro level has no gem goal");

    // A loss is not a completion.
    on_level_finished(Match3Game::LOST, 0);
    if (completions_ != 0) fail("a lost level completed the demo");

    // Two wins: the first completes the demo, a replayed win does not again.
    for (int round = 0; round < 2; ++round) {
        if (round == 1 && !g->start_level(level_id_, g->get_autostart_difficulty())) fail("the replay did not start");
        int moves = 0;
        while (g->is_playing() && moves < kSelfTestMaxMoves) {
            const Array legal = g->get_legal_moves();
            if (legal.is_empty()) break;
            const Array m = legal[0];
            if (!g->request_swap(m[0], m[1])) {
                fail("a listed legal move was refused");
                break;
            }
            ++moves;
            if (v) v->skip_animation();
        }
        if (round == 0) {
            self_test_moves_ = moves;
            self_test_outcome_ = g->get_outcome();
        }
        if (g->get_outcome() != Match3Game::WON) fail("the intro level played by the first legal move did not win");
        if (g->get_gems_collected() < g->get_gem_goal()) fail("won without the gem goal");
        if (completions_ != 1) fail("level_completed fired " + String::num_int64(completions_) + " times");
        if (round == 0 && score_ != g->get_score()) fail("level_completed score differs from the game's");
    }

    // The demo plays the intro level only: another level is sent back.
    if (UiRoot* ui = ui_root(this)) ui->perform("play", kSelfTestOtherLevel, Dictionary());
    else g->start_level(kSelfTestOtherLevel, g->get_autostart_difficulty());
    if (g->get_level_id() != kSelfTestOtherLevel) fail(String("the other level did not start: ") + kSelfTestOtherLevel);
    // After the deferred redirect, which was queued first.
    callable_mp(this, &IntroLevel::finish_self_test).call_deferred();
}

void IntroLevel::finish_self_test() {
    if (const Match3Game* g = game(); !g || g->get_level_id() != level_id_ || !g->is_playing()) {
        UtilityFunctions::push_error("IntroLevel self-test: another level was not sent back to the intro level");
        ++self_test_failures_;
    }
    UtilityFunctions::print("IntroLevel self-test: ", self_test_moves_, " moves, outcome ", self_test_outcome_, ", ",
                            completions_, " completions, ", self_test_failures_, " failures");
}

void IntroLevel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_game_path", "path"), &IntroLevel::set_game_path);
    ClassDB::bind_method(D_METHOD("get_game_path"), &IntroLevel::get_game_path);
    ClassDB::bind_method(D_METHOD("get_level_id"), &IntroLevel::get_level_id);
    ClassDB::bind_method(D_METHOD("get_score"), &IntroLevel::get_score);
    ClassDB::bind_method(D_METHOD("is_completed"), &IntroLevel::is_completed);
    ClassDB::bind_method(D_METHOD("complete"), &IntroLevel::complete);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "game_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Match3Game"),
                 "set_game_path", "get_game_path");

    ADD_SIGNAL(MethodInfo("level_completed", PropertyInfo(Variant::STRING, "level_id"), PropertyInfo(Variant::INT, "score")));
}

}  // namespace montezuma::godot_binding
