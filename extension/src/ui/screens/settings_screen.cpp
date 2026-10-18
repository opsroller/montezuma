// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "settings_screen.hpp"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/range.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../ui_root.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {
Node* unique(const Node* screen, const char* name) { return screen->get_node_or_null(NodePath(String("%") + name)); }
}  // namespace

UiRoot* SettingsScreen::root() const {
    return Object::cast_to<UiRoot>(get_tree()->get_root()->get_node_or_null(NodePath("UiRoot")));
}

void SettingsScreen::on_open(const Dictionary& params) {
    if (root()) {
        bind_slider("Music", "music_volume");
        bind_slider("Sfx", "sfx_volume");
        bind_slider("TextSize", "text_scale");
        bind_toggle("ReducedMotion", "reduced_motion");
        bind_toggle("Fullscreen", "fullscreen");

        PackedStringArray locales;
        PackedStringArray names;
        locales.push_back("");
        names.push_back("settings.language.system");
        PackedStringArray loaded = TranslationServer::get_singleton()->get_loaded_locales();
        loaded.sort();
        for (int64_t i = 0; i < loaded.size(); ++i) {
            if (locales.has(loaded[i])) continue;
            locales.push_back(loaded[i]);
            names.push_back(TranslationServer::get_singleton()->get_locale_name(loaded[i]));
        }
        bind_option("Language", "locale", locales, names);
        bind_option("Difficulty", "difficulty", PackedStringArray({"normal", "hard"}),
                    PackedStringArray({"settings.difficulty.normal", "settings.difficulty.hard"}));
    }
    // Window mode only means something on desktop.
    OS* os = OS::get_singleton();
    if (os->has_feature("mobile") || os->has_feature("web")) {
        if (auto* row = Object::cast_to<CanvasItem>(unique(this, "FullscreenRow"))) row->hide();
    }
    Screen::on_open(params);
}

void SettingsScreen::on_close() {
    if (UiRoot* r = root()) r->save_settings();
    Screen::on_close();
}

void SettingsScreen::bind_slider(const char* node, const char* key) {
    auto* range = Object::cast_to<Range>(unique(this, node));
    if (!range) return;
    range->set_value_no_signal(root()->get_setting(key));
    range->connect("value_changed", callable_mp(this, &SettingsScreen::on_value).bind(String(key)));
}

void SettingsScreen::bind_toggle(const char* node, const char* key) {
    auto* button = Object::cast_to<BaseButton>(unique(this, node));
    if (!button) return;
    button->set_pressed_no_signal(root()->get_setting(key));
    button->connect("toggled", callable_mp(this, &SettingsScreen::on_toggled).bind(String(key)));
}

void SettingsScreen::bind_option(const char* node, const char* key, const PackedStringArray& values,
                                 const PackedStringArray& labels) {
    auto* option = Object::cast_to<OptionButton>(unique(this, node));
    if (!option) return;
    option->clear();
    const String current = root()->get_setting(key);
    for (int64_t i = 0; i < values.size(); ++i) {
        option->add_item(labels[i]);
        option->set_item_metadata(static_cast<int32_t>(i), values[i]);
        if (values[i] == current) option->select(static_cast<int32_t>(i));
    }
    option->connect("item_selected", callable_mp(this, &SettingsScreen::on_option).bind(String(node), String(key)));
}

void SettingsScreen::on_value(double value, const String& key) {
    if (UiRoot* r = root()) r->set_setting(key, value);
}

void SettingsScreen::on_toggled(bool on, const String& key) {
    if (UiRoot* r = root()) r->set_setting(key, on);
}

void SettingsScreen::on_option(int64_t index, const String& node, const String& key) {
    auto* option = Object::cast_to<OptionButton>(get_node_or_null(NodePath("%" + node)));
    UiRoot* r = root();
    if (option && r) r->set_setting(key, option->get_item_metadata(static_cast<int32_t>(index)));
}

}  // namespace montezuma::godot_binding
