// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "model/settings.hpp"

namespace montezuma::godot_binding {

class ScreenStack;

// The UI's autoload (res://ui/ui_root.tscn, reachable as /root/UiRoot).
// Scales the UI from its reference size to the window, keeps it inside the
// safe area, builds the theme from the tokens file, owns player settings,
// and turns menu actions into requests for the game scene:
// play_requested(level_id, difficulty) and level_quit_requested().
class UiRoot : public godot::CanvasLayer {
    GDCLASS(UiRoot, godot::CanvasLayer)

public:
    void _ready() override;
    void _unhandled_input(const godot::Ref<godot::InputEvent>& event) override;

    // Actions NavButton and screens route here (see NavButton).
    void perform(const godot::String& action, const godot::String& target, const godot::Dictionary& params);
    // Opens the results screen; outcome is LevelState::Outcome (1 won, 2 lost).
    void show_results(int outcome, int score, const godot::String& level_id);
    void open_pause();

    godot::Variant get_setting(const godot::String& key) const;
    // Applies at once; saved by save_settings() (the settings screen saves on close).
    void set_setting(const godot::String& key, const godot::Variant& value);
    void save_settings() const;

    ScreenStack* get_stack() const;
    godot::Dictionary get_tokens() const { return tokens_; }
    godot::String get_current_level() const { return current_level_; }

    void set_reference_size(const godot::Vector2& v) { reference_size_ = v; }
    godot::Vector2 get_reference_size() const { return reference_size_; }
    void set_tokens_path(const godot::String& v) { tokens_path_ = v; }
    godot::String get_tokens_path() const { return tokens_path_; }
    void set_settings_path(const godot::String& v) { settings_path_ = v; }
    godot::String get_settings_path() const { return settings_path_; }

protected:
    static void _bind_methods();

private:
    void ensure_actions();
    void load_settings();
    // Applies one setting, or all of them for an empty key.
    void apply_settings(const godot::String& key);
    void apply_theme();
    void relayout();
    void on_stack_action(const godot::String& action, const godot::String& target, const godot::Dictionary& params);
    void start_level(const godot::String& level_id);

    godot::Vector2 reference_size_{1280, 720};
    godot::String tokens_path_ = "res://ui/theme/placeholder.tokens.json";
    godot::String settings_path_ = "user://settings.cfg";
    godot::Dictionary tokens_;
    ui::Settings settings_;
    godot::String current_level_;
    godot::Control* scaled_ = nullptr;
    godot::MarginContainer* safe_area_ = nullptr;
};

}  // namespace montezuma::godot_binding
