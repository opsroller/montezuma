// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace montezuma::godot_binding {

// A button whose press is data: an action, a target and params set in the
// scene, routed through the enclosing Screen (or UiRoot when outside one).
// Actions: push, replace, reset, back (ScreenStack); play, pause, resume,
// restart, quit_level, quit, url (UiRoot). Plays the ui_click / ui_back cue.
class NavButton : public godot::Button {
    GDCLASS(NavButton, godot::Button)

public:
    // Touch target floor in reference pixels (48 dp guideline).
    static constexpr float kMinHeight = 48.0f;

    void _ready() override;

    void set_action(const godot::String& v) { action_ = v; }
    godot::String get_action() const { return action_; }
    void set_target(const godot::String& v) { target_ = v; }
    godot::String get_target() const { return target_; }
    void set_params(const godot::Dictionary& v) { params_ = v; }
    godot::Dictionary get_params() const { return params_; }
    void set_hidden_on(const godot::PackedStringArray& v) { hidden_on_ = v; }
    godot::PackedStringArray get_hidden_on() const { return hidden_on_; }

    // Plays a UI cue through the GameAudio autoload when it exists.
    static void play_cue(godot::Node* from, const godot::String& cue);

protected:
    static void _bind_methods();

private:
    void on_pressed();

    godot::String action_ = "push";
    godot::String target_;
    godot::Dictionary params_;
    godot::PackedStringArray hidden_on_;
};

}  // namespace montezuma::godot_binding
