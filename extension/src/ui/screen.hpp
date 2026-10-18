// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>

namespace montezuma::godot_binding {

class ScreenStack;

// Base for every menu screen and modal. A screen is a scene whose root is a
// Screen (or subclass); only ScreenStack opens and closes it. Screens never
// open each other: buttons call perform(), which the stack routes.
class Screen : public godot::Control {
    GDCLASS(Screen, godot::Control)

public:
    enum BackBehavior {
        BACK_POP,      // back closes this screen
        BACK_BLOCKED,  // back does nothing (root screens)
        BACK_ACTION,   // back runs back_action / back_target
    };

    void _ready() override;

    // Lifecycle, called by ScreenStack only.
    virtual void on_open(const godot::Dictionary& params);
    virtual void on_cover();
    virtual void on_uncover();
    virtual void on_close();

    // Routes a navigation or game action (see NavButton::action) to the stack.
    void perform(const godot::String& action, const godot::String& target, const godot::Dictionary& params);
    // What ui_cancel does on this screen.
    void back();
    // Moves focus to default_focus, or the first focusable descendant.
    virtual void focus_default();
    // Covered modals' owners stay visible but must not take focus.
    void set_inert(bool inert);

    void set_screen_id(const godot::String& v) { screen_id_ = v; }
    godot::String get_screen_id() const { return screen_id_; }
    void set_default_focus(const godot::NodePath& v) { default_focus_ = v; }
    godot::NodePath get_default_focus() const { return default_focus_; }
    void set_modal(bool v) { modal_ = v; }
    bool is_modal() const { return modal_; }
    void set_pauses_game(bool v) { pauses_game_ = v; }
    bool get_pauses_game() const { return pauses_game_; }
    void set_back_behavior(int v) { back_behavior_ = static_cast<BackBehavior>(v); }
    int get_back_behavior() const { return back_behavior_; }
    void set_back_action(const godot::String& v) { back_action_ = v; }
    godot::String get_back_action() const { return back_action_; }
    void set_back_target(const godot::String& v) { back_target_ = v; }
    godot::String get_back_target() const { return back_target_; }
    godot::Dictionary get_params() const { return params_; }

protected:
    static void _bind_methods();
    ScreenStack* stack() const;

private:
    godot::String screen_id_;
    godot::NodePath default_focus_;
    bool modal_ = false;
    bool pauses_game_ = false;
    BackBehavior back_behavior_ = BACK_POP;
    godot::String back_action_ = "back";
    godot::String back_target_;
    godot::Dictionary params_;
};

}  // namespace montezuma::godot_binding

VARIANT_ENUM_CAST(montezuma::godot_binding::Screen::BackBehavior);
