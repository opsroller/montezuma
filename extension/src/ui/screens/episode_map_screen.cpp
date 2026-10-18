// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "episode_map_screen.hpp"

#include <algorithm>

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../model/format.hpp"
#include "../nav_button.hpp"

using namespace godot;

namespace montezuma::godot_binding {

int EpisodeMapScreen::level_count(int episode) const {
    const String dir = "res://data/levels/e" + String::num_int64(episode);
    if (!DirAccess::dir_exists_absolute(dir)) return placeholder_levels_;
    const PackedStringArray files = DirAccess::get_files_at(dir);
    int count = 0;
    for (int64_t i = 0; i < files.size(); ++i)
        if (files[i].begins_with("l") && files[i].ends_with(".json")) ++count;
    return count > 0 ? count : placeholder_levels_;
}

bool EpisodeMapScreen::is_unlocked(const String& level_id) const {
    static const bool unlock_all = OS::get_singleton()->get_cmdline_user_args().has("--unlock-all");
    return unlock_all || level_id == "e1-l01";
}

void EpisodeMapScreen::on_open(const Dictionary& params) {
    if (auto* prev = Object::cast_to<BaseButton>(get_node_or_null(NodePath("%Prev"))))
        prev->connect("pressed", callable_mp(this, &EpisodeMapScreen::step).bind(-1));
    if (auto* next = Object::cast_to<BaseButton>(get_node_or_null(NodePath("%Next"))))
        next->connect("pressed", callable_mp(this, &EpisodeMapScreen::step).bind(1));
    show_episode(params.get("episode", 1));
    Screen::on_open(params);
}

void EpisodeMapScreen::_notification(int what) {
    // Level labels are built from translated text; rebuild on a language change.
    if (what == NOTIFICATION_TRANSLATION_CHANGED && is_node_ready()) show_episode(episode_);
}

void EpisodeMapScreen::focus_default() {
    if (auto* grid = Object::cast_to<Container>(get_node_or_null(NodePath("%Levels")))) {
        for (int32_t i = 0; i < grid->get_child_count(); ++i) {
            auto* button = Object::cast_to<BaseButton>(grid->get_child(i));
            if (button && !button->is_queued_for_deletion() && !button->is_disabled()) {
                button->grab_focus();
                return;
            }
        }
    }
    Screen::focus_default();
}

void EpisodeMapScreen::step(int delta) {
    show_episode(episode_ + delta);
    focus_default();
}

void EpisodeMapScreen::show_episode(int episode) {
    episode_ = std::clamp(episode, 1, std::max(1, episodes_));
    if (auto* title = Object::cast_to<Label>(get_node_or_null(NodePath("%EpisodeTitle"))))
        title->set_text("episode." + String::num_int64(episode_) + ".name");
    if (auto* prev = Object::cast_to<BaseButton>(get_node_or_null(NodePath("%Prev")))) prev->set_disabled(episode_ <= 1);
    if (auto* next = Object::cast_to<BaseButton>(get_node_or_null(NodePath("%Next")))) next->set_disabled(episode_ >= episodes_);

    auto* grid = Object::cast_to<Container>(get_node_or_null(NodePath("%Levels")));
    if (!grid) return;
    for (int32_t i = grid->get_child_count() - 1; i >= 0; --i) {
        Node* child = grid->get_child(i);
        grid->remove_child(child);
        child->queue_free();
    }
    const int count = level_count(episode_);
    for (int i = 1; i <= count; ++i) {
        const String id = String::utf8(ui::level_id(episode_, i).c_str());
        const bool open = is_unlocked(id);
        auto* button = memnew(NavButton);
        button->set_action("play");
        button->set_target(id);
        // State is in the words, not only the colour.
        Dictionary args;
        args["n"] = i;
        button->set_text(tr(open ? "map.level.open" : "map.level.locked").format(args));
        button->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
        button->set_disabled(!open);
        button->set_custom_minimum_size(Vector2(96, 64));
        grid->add_child(button);
    }
}

void EpisodeMapScreen::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_episodes", "count"), &EpisodeMapScreen::set_episodes);
    ClassDB::bind_method(D_METHOD("get_episodes"), &EpisodeMapScreen::get_episodes);
    ClassDB::bind_method(D_METHOD("set_placeholder_levels", "count"), &EpisodeMapScreen::set_placeholder_levels);
    ClassDB::bind_method(D_METHOD("get_placeholder_levels"), &EpisodeMapScreen::get_placeholder_levels);
    ClassDB::bind_method(D_METHOD("level_count", "episode"), &EpisodeMapScreen::level_count);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "episodes", PROPERTY_HINT_RANGE, "1,20"), "set_episodes", "get_episodes");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "placeholder_levels", PROPERTY_HINT_RANGE, "1,99"), "set_placeholder_levels",
                 "get_placeholder_levels");
}

}  // namespace montezuma::godot_binding
