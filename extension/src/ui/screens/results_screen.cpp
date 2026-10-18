// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "results_screen.hpp"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../model/format.hpp"
#include "../nav_button.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// The level after id ("e1-l03" -> "e1-l04"), or empty when no such
// LevelData file exists. Before level files land, any next index is offered.
String next_level(const String& id) {
    const PackedStringArray parts = id.trim_prefix("e").split("-l");
    if (parts.size() != 2 || !parts[0].is_valid_int() || !parts[1].is_valid_int()) return {};
    const int episode = static_cast<int>(parts[0].to_int());
    const int index = static_cast<int>(parts[1].to_int());
    const String next = String::utf8(ui::level_id(episode, index + 1).c_str());
    const String dir = "res://data/levels/e" + String::num_int64(episode);
    if (!FileAccess::file_exists(dir + String("/l01.json"))) return next;
    return FileAccess::file_exists(dir + String("/l") + next.get_slice("-l", 1) + String(".json")) ? next : String();
}

}  // namespace

void ResultsScreen::on_open(const Dictionary& params) {
    Screen::on_open(params);
    refresh();
}

void ResultsScreen::_notification(int what) {
    if (what == NOTIFICATION_TRANSLATION_CHANGED && is_node_ready()) refresh();
}

void ResultsScreen::refresh() {
    const Dictionary params = get_params();
    const bool won = int(params.get("outcome", 0)) == kOutcomeWon;
    const String level = params.get("level_id", "");
    if (auto* title = Object::cast_to<Label>(get_node_or_null(NodePath("%Title"))))
        title->set_text(won ? "results.won" : "results.lost");
    if (auto* score = Object::cast_to<Label>(get_node_or_null(NodePath("%Score")))) {
        Dictionary args;
        args["score"] = int(params.get("score", 0));
        score->set_text(tr("results.score").format(args));
    }
    if (auto* retry = Object::cast_to<NavButton>(get_node_or_null(NodePath("%Retry")))) retry->set_target(level);
    if (auto* next = Object::cast_to<NavButton>(get_node_or_null(NodePath("%Next")))) {
        const String target = won ? next_level(level) : String();
        next->set_target(target);
        next->set_visible(!target.is_empty());
    }
}

}  // namespace montezuma::godot_binding
