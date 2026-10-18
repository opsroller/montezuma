// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "ui_root.hpp"

#include <algorithm>

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "screen_stack.hpp"
#include "ui_theme.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

const char* const kPauseAction = "ui_pause";
const char* const kSection = "settings";

bool is_desktop() {
    OS* os = OS::get_singleton();
    return !os->has_feature("mobile") && !os->has_feature("web") && DisplayServer::get_singleton()->get_name() != "headless";
}

void set_bus_volume(const char* bus, float linear) {
    AudioServer* audio = AudioServer::get_singleton();
    const int32_t idx = audio->get_bus_index(bus);
    if (idx >= 0) audio->set_bus_volume_db(idx, ui::linear_to_db(linear));
}

}  // namespace

void UiRoot::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    // Menus, including pause, keep working while the game tree is paused.
    set_process_mode(PROCESS_MODE_ALWAYS);
    scaled_ = Object::cast_to<Control>(get_node_or_null(NodePath("Scaled")));
    safe_area_ = Object::cast_to<MarginContainer>(get_node_or_null(NodePath("Scaled/SafeArea")));
    if (!scaled_ || !safe_area_ || !get_stack()) {
        UtilityFunctions::push_error("UiRoot: expected Scaled/SafeArea/ScreenStack children");
        return;
    }
    ensure_actions();
    if (OS::get_singleton()->get_cmdline_user_args().has("--pseudolocalize"))
        TranslationServer::get_singleton()->set_pseudolocalization_enabled(true);
    tokens_ = UiTheme::load_tokens(tokens_path_);
    load_settings();
    apply_settings({});
    get_viewport()->connect("size_changed", callable_mp(this, &UiRoot::relayout));
    get_stack()->connect("action_requested", callable_mp(this, &UiRoot::on_stack_action));
    relayout();
}

void UiRoot::_unhandled_input(const Ref<InputEvent>& event) {
    ScreenStack* stack = get_stack();
    if (!stack || !event.is_valid()) return;
    const bool pause = event->is_action_pressed(kPauseAction);
    if (stack->is_empty() && (pause || event->is_action_pressed("ui_cancel"))) {
        open_pause();
        get_viewport()->set_input_as_handled();
    } else if (pause && stack->top_id() == "pause" && stack->depth() == 1) {
        stack->back();
        get_viewport()->set_input_as_handled();
    }
}

void UiRoot::ensure_actions() {
    InputMap* map = InputMap::get_singleton();
    if (map->has_action(kPauseAction)) return;
    map->add_action(kPauseAction);
    Ref<InputEventKey> key;
    key.instantiate();
    key->set_physical_keycode(KEY_P);
    map->action_add_event(kPauseAction, key);
    Ref<InputEventJoypadButton> start;
    start.instantiate();
    start->set_button_index(JOY_BUTTON_START);
    map->action_add_event(kPauseAction, start);
}

ScreenStack* UiRoot::get_stack() const {
    return Object::cast_to<ScreenStack>(get_node_or_null(NodePath("Scaled/SafeArea/ScreenStack")));
}

void UiRoot::relayout() {
    if (!scaled_ || !get_viewport()) return;
    const Vector2 window = get_viewport()->get_visible_rect().size;
    if (window.x <= 0 || window.y <= 0 || reference_size_.x <= 0 || reference_size_.y <= 0) return;
    // Uniform scale so the reference layout fits; the long side gets the
    // extra room (like the canvas_items / expand stretch mode, UI only).
    const real_t s = std::min(window.x / reference_size_.x, window.y / reference_size_.y);
    set_scale(Vector2(s, s));
    scaled_->set_position(Vector2());
    scaled_->set_size(window / s);

    // Notches, rounded corners and system bars: phones only report them.
    real_t left = 0, top = 0, right = 0, bottom = 0;
    if (OS::get_singleton()->has_feature("mobile")) {
        const Rect2i safe = DisplayServer::get_singleton()->get_display_safe_area();
        left = static_cast<real_t>(safe.position.x);
        top = static_cast<real_t>(safe.position.y);
        right = window.x - static_cast<real_t>(safe.position.x + safe.size.x);
        bottom = window.y - static_cast<real_t>(safe.position.y + safe.size.y);
    }
    safe_area_->add_theme_constant_override("margin_left", static_cast<int32_t>(std::max<real_t>(0, left / s)));
    safe_area_->add_theme_constant_override("margin_top", static_cast<int32_t>(std::max<real_t>(0, top / s)));
    safe_area_->add_theme_constant_override("margin_right", static_cast<int32_t>(std::max<real_t>(0, right / s)));
    safe_area_->add_theme_constant_override("margin_bottom", static_cast<int32_t>(std::max<real_t>(0, bottom / s)));
}

void UiRoot::apply_theme() {
    if (scaled_) scaled_->set_theme(UiTheme::build(tokens_, settings_.text_scale));
}

void UiRoot::load_settings() {
    Ref<ConfigFile> cfg;
    cfg.instantiate();
    if (cfg->load(settings_path_) != OK) return;  // first run: defaults
    const ui::Settings d;
    settings_.music_volume = static_cast<float>(double(cfg->get_value(kSection, "music_volume", d.music_volume)));
    settings_.sfx_volume = static_cast<float>(double(cfg->get_value(kSection, "sfx_volume", d.sfx_volume)));
    settings_.fullscreen = cfg->get_value(kSection, "fullscreen", d.fullscreen);
    settings_.locale = String(cfg->get_value(kSection, "locale", "")).utf8().get_data();
    settings_.text_scale = static_cast<float>(double(cfg->get_value(kSection, "text_scale", d.text_scale)));
    settings_.reduced_motion = cfg->get_value(kSection, "reduced_motion", d.reduced_motion);
    settings_.difficulty = String(cfg->get_value(kSection, "difficulty", d.difficulty.c_str())).utf8().get_data();
    settings_.sanitize();
}

void UiRoot::save_settings() const {
    Ref<ConfigFile> cfg;
    cfg.instantiate();
    cfg->set_value(kSection, "music_volume", settings_.music_volume);
    cfg->set_value(kSection, "sfx_volume", settings_.sfx_volume);
    cfg->set_value(kSection, "fullscreen", settings_.fullscreen);
    cfg->set_value(kSection, "locale", String::utf8(settings_.locale.c_str()));
    cfg->set_value(kSection, "text_scale", settings_.text_scale);
    cfg->set_value(kSection, "reduced_motion", settings_.reduced_motion);
    cfg->set_value(kSection, "difficulty", String::utf8(settings_.difficulty.c_str()));
    if (cfg->save(settings_path_) != OK) UtilityFunctions::push_warning("UiRoot: could not save settings to ", settings_path_);
}

void UiRoot::apply_settings(const String& key) {
    const bool all = key.is_empty();
    if (all || key == "music_volume") set_bus_volume("Music", settings_.music_volume);
    if (all || key == "sfx_volume") set_bus_volume("SFX", settings_.sfx_volume);
    if (all || key == "locale")
        TranslationServer::get_singleton()->set_locale(settings_.locale.empty() ? OS::get_singleton()->get_locale()
                                                                                : String::utf8(settings_.locale.c_str()));
    if ((all || key == "fullscreen") && is_desktop()) {
        DisplayServer* ds = DisplayServer::get_singleton();
        const bool full = ds->window_get_mode() == DisplayServer::WINDOW_MODE_FULLSCREEN;
        if (full != settings_.fullscreen)
            ds->window_set_mode(settings_.fullscreen ? DisplayServer::WINDOW_MODE_FULLSCREEN : DisplayServer::WINDOW_MODE_WINDOWED);
    }
    if (all || key == "reduced_motion") {
        if (ScreenStack* stack = get_stack())
            stack->set_transition_seconds(
                settings_.reduced_motion ? 0.0 : UiTheme::token_number(tokens_, "motion.duration.normal", 150) / 1000.0);
    }
    if (all || key == "text_scale") apply_theme();
}

Variant UiRoot::get_setting(const String& key) const {
    if (key == "music_volume") return settings_.music_volume;
    if (key == "sfx_volume") return settings_.sfx_volume;
    if (key == "fullscreen") return settings_.fullscreen;
    if (key == "locale") return String::utf8(settings_.locale.c_str());
    if (key == "text_scale") return settings_.text_scale;
    if (key == "reduced_motion") return settings_.reduced_motion;
    if (key == "difficulty") return String::utf8(settings_.difficulty.c_str());
    UtilityFunctions::push_error("UiRoot: unknown setting: ", key);
    return {};
}

void UiRoot::set_setting(const String& key, const Variant& value) {
    if (key == "music_volume")
        settings_.music_volume = static_cast<float>(double(value));
    else if (key == "sfx_volume")
        settings_.sfx_volume = static_cast<float>(double(value));
    else if (key == "fullscreen")
        settings_.fullscreen = value;
    else if (key == "locale")
        settings_.locale = String(value).utf8().get_data();
    else if (key == "text_scale")
        settings_.text_scale = static_cast<float>(double(value));
    else if (key == "reduced_motion")
        settings_.reduced_motion = value;
    else if (key == "difficulty")
        settings_.difficulty = String(value).utf8().get_data();
    else {
        UtilityFunctions::push_error("UiRoot: unknown setting: ", key);
        return;
    }
    settings_.sanitize();
    apply_settings(key);
    emit_signal("settings_changed", key);
}

void UiRoot::start_level(const String& level_id) {
    if (level_id.is_empty()) return;
    current_level_ = level_id;
    if (ScreenStack* stack = get_stack()) stack->clear();
    emit_signal("play_requested", level_id, String::utf8(settings_.difficulty.c_str()));
}

void UiRoot::open_pause() {
    if (ScreenStack* stack = get_stack(); stack && stack->is_empty()) stack->push("pause", {});
}

void UiRoot::show_results(int outcome, int score, const String& level_id) {
    Dictionary params;
    params["outcome"] = outcome;
    params["score"] = score;
    params["level_id"] = level_id.is_empty() ? current_level_ : level_id;
    if (ScreenStack* stack = get_stack()) stack->reset_to("results", params);
}

void UiRoot::perform(const String& action, const String& target, const Dictionary& params) {
    if (ScreenStack* stack = get_stack()) stack->perform(action, target, params);
}

void UiRoot::on_stack_action(const String& action, const String& target, const Dictionary& params) {
    ScreenStack* stack = get_stack();
    if (action == "play") {
        start_level(target);
    } else if (action == "restart") {
        start_level(current_level_);
    } else if (action == "resume") {
        if (stack && stack->top_id() == "pause") stack->back();
    } else if (action == "quit_level") {
        emit_signal("level_quit_requested");
        if (stack) stack->reset_to(target.is_empty() ? String("episode_map") : target, params);
    } else if (action == "quit") {
        get_tree()->quit();
    } else if (action == "url") {
        if (target.begins_with("https://")) OS::get_singleton()->shell_open(target);
    } else {
        UtilityFunctions::push_error("UiRoot: unknown action: ", action);
    }
}

void UiRoot::_bind_methods() {
    ClassDB::bind_method(D_METHOD("perform", "action", "target", "params"), &UiRoot::perform, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("show_results", "outcome", "score", "level_id"), &UiRoot::show_results, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("open_pause"), &UiRoot::open_pause);
    ClassDB::bind_method(D_METHOD("get_setting", "key"), &UiRoot::get_setting);
    ClassDB::bind_method(D_METHOD("set_setting", "key", "value"), &UiRoot::set_setting);
    ClassDB::bind_method(D_METHOD("save_settings"), &UiRoot::save_settings);
    ClassDB::bind_method(D_METHOD("get_stack"), &UiRoot::get_stack);
    ClassDB::bind_method(D_METHOD("get_tokens"), &UiRoot::get_tokens);
    ClassDB::bind_method(D_METHOD("get_current_level"), &UiRoot::get_current_level);

    ClassDB::bind_method(D_METHOD("set_reference_size", "size"), &UiRoot::set_reference_size);
    ClassDB::bind_method(D_METHOD("get_reference_size"), &UiRoot::get_reference_size);
    ClassDB::bind_method(D_METHOD("set_tokens_path", "path"), &UiRoot::set_tokens_path);
    ClassDB::bind_method(D_METHOD("get_tokens_path"), &UiRoot::get_tokens_path);
    ClassDB::bind_method(D_METHOD("set_settings_path", "path"), &UiRoot::set_settings_path);
    ClassDB::bind_method(D_METHOD("get_settings_path"), &UiRoot::get_settings_path);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "reference_size"), "set_reference_size", "get_reference_size");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "tokens_path", PROPERTY_HINT_FILE, "*.json"), "set_tokens_path", "get_tokens_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "settings_path"), "set_settings_path", "get_settings_path");

    ADD_SIGNAL(MethodInfo("play_requested", PropertyInfo(Variant::STRING, "level_id"), PropertyInfo(Variant::STRING, "difficulty")));
    ADD_SIGNAL(MethodInfo("level_quit_requested"));
    ADD_SIGNAL(MethodInfo("settings_changed", PropertyInfo(Variant::STRING, "key")));
}

}  // namespace montezuma::godot_binding
