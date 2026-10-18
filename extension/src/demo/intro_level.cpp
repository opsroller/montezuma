// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "intro_level.hpp"

#include <string>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "board/board_view_3d.hpp"
#include "handoff_script.hpp"
#include "montezuma/level_data.hpp"
#include "ui/screen_stack.hpp"
#include "ui/ui_root.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr const char* kCompleteLevelArg = "--complete-level";

std::string utf8(const String& s) {
    const CharString c = s.utf8();
    return std::string(c.get_data(), static_cast<std::size_t>(c.length()));
}

// Runs a hand-off script on the page; does nothing off the web.
void tell_page(const std::string& script) {
    if (!OS::get_singleton()->has_feature("web")) return;
    JavaScriptBridge::get_singleton()->eval(String::utf8(script.c_str()), false);
}

}  // namespace

BoardView3D* IntroLevel::view() const {
    if (board_view_path_.is_empty() || !is_inside_tree()) return nullptr;
    return Object::cast_to<BoardView3D>(get_node_or_null(board_view_path_));
}

bool IntroLevel::load_level() {
    const String text = FileAccess::get_file_as_string(level_path_);
    ERR_FAIL_COND_V_MSG(text.is_empty(), false, "IntroLevel: cannot read " + level_path_);
    const LevelParseResult parsed = LevelData::parse(utf8(text));
    ERR_FAIL_COND_V_MSG(!parsed.ok(), false,
                        "IntroLevel: " + level_path_ + ":\n" + String::utf8(parsed.error_text().c_str()));
    const LevelData& data = *parsed.level;
    level_id_ = String::utf8(data.id.c_str());
    width_ = data.board.width;
    height_ = data.board.height;
    kinds_ = data.board.kinds;
    seed_ = static_cast<int64_t>(data.seed);
    if (!data.quotas.empty()) {
        Dictionary quotas;
        for (const auto& [kind, count] : data.quotas) quotas[kind] = count;
        level_.instantiate();
        level_->connect("level_finished", callable_mp(this, &IntroLevel::on_level_finished));
        level_->setup(quotas, data.time_limit_for(Difficulty::Normal));
    } else {
        UtilityFunctions::print("IntroLevel: ", level_id_,
                                " has a gem goal, which the core does not track yet; it plays without a win.");
    }
    return true;
}

void IntroLevel::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }
    complete_on_start_ = OS::get_singleton()->get_cmdline_user_args().has(kCompleteLevelArg);
    BoardView3D* v = view();
    ERR_FAIL_NULL_MSG(v, "IntroLevel needs board_view_path to point at a BoardView3D.");
    if (!load_level()) return;
    // The demo opens on the board: close the menu the UI starts with.
    if (auto* ui = Object::cast_to<UiRoot>(get_node_or_null(NodePath("/root/UiRoot")))) {
        if (ScreenStack* stack = ui->get_stack()) stack->clear();
    }
    board_.instantiate();
    board_->setup(width_, height_, kinds_, seed_);
    v->connect("swap_requested", callable_mp(this, &IntroLevel::on_swap_requested));
    v->connect("animation_finished", callable_mp(this, &IntroLevel::on_animation_finished));
    v->show_board(board_);
}

void IntroLevel::_process(double delta) {
    if (first_frame_) {
        first_frame_ = false;
        const String version = ProjectSettings::get_singleton()->get_setting("application/config/version", "");
        tell_page(demo::ready_script(utf8(version)));
        if (complete_on_start_) complete();
    }
    if (level_.is_valid() && !completed_) level_->tick(delta);
}

void IntroLevel::_exit_tree() {
    if (level_.is_valid()) {
        const Callable finished = callable_mp(this, &IntroLevel::on_level_finished);
        if (level_->is_connected("level_finished", finished)) level_->disconnect("level_finished", finished);
    }
    BoardView3D* v = view();
    if (!v) return;
    const Callable swap = callable_mp(this, &IntroLevel::on_swap_requested);
    const Callable done = callable_mp(this, &IntroLevel::on_animation_finished);
    if (v->is_connected("swap_requested", swap)) v->disconnect("swap_requested", swap);
    if (v->is_connected("animation_finished", done)) v->disconnect("animation_finished", done);
}

void IntroLevel::complete() {
    // Nothing to win before a level is loaded.
    if (completed_ || level_id_.is_empty()) return;
    completed_ = true;
    emit_signal("level_completed", level_id_, score_);
    tell_page(demo::level_completed_script(utf8(level_id_), score_));
}

void IntroLevel::on_swap_requested(Vector2i a, Vector2i b) {
    BoardView3D* v = view();
    if (!v || v->is_busy() || completed_) return;
    const Dictionary result = board_->try_swap(a, b);
    if (result.is_empty()) {
        v->play_illegal(a, b);
        return;
    }
    score_ += static_cast<int>(result["score"]);
    // No playable arrangement is left: deal a new board once this move is shown.
    redeal_ = static_cast<bool>(result["stuck"]);
    v->play(result);
    if (level_.is_valid()) level_->apply_move(result);
}

void IntroLevel::on_animation_finished() {
    if (!redeal_) return;
    redeal_ = false;
    board_->setup(width_, height_, kinds_, ++seed_);
    if (BoardView3D* v = view()) v->show_board(board_);
}

void IntroLevel::on_level_finished(int outcome, int score) {
    score_ = score;
    if (outcome == Match3Level::WON) complete();
}

void IntroLevel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_board_view_path", "path"), &IntroLevel::set_board_view_path);
    ClassDB::bind_method(D_METHOD("get_board_view_path"), &IntroLevel::get_board_view_path);
    ClassDB::bind_method(D_METHOD("set_level_path", "path"), &IntroLevel::set_level_path);
    ClassDB::bind_method(D_METHOD("get_level_path"), &IntroLevel::get_level_path);
    ClassDB::bind_method(D_METHOD("get_level_id"), &IntroLevel::get_level_id);
    ClassDB::bind_method(D_METHOD("get_score"), &IntroLevel::get_score);
    ClassDB::bind_method(D_METHOD("is_completed"), &IntroLevel::is_completed);
    ClassDB::bind_method(D_METHOD("complete"), &IntroLevel::complete);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "board_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BoardView3D"),
                 "set_board_view_path", "get_board_view_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "level_path", PROPERTY_HINT_FILE, "*.json"), "set_level_path",
                 "get_level_path");

    ADD_SIGNAL(MethodInfo("level_completed", PropertyInfo(Variant::STRING, "level_id"), PropertyInfo(Variant::INT, "score")));
}

}  // namespace montezuma::godot_binding
