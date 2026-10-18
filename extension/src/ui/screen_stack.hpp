// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <vector>

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "model/nav_stack.hpp"

namespace montezuma::godot_binding {

class Screen;

// The one navigation owner. Opens screens by id from the `screens` table
// (id -> scene path), keeps the back stack, restores focus when a screen is
// uncovered and routes ui_cancel to the top screen. Actions it does not
// handle itself (play, resume, quit, url ...) are emitted as
// action_requested for UiRoot.
class ScreenStack : public godot::Control {
    GDCLASS(ScreenStack, godot::Control)

public:
    void _ready() override;
    void _unhandled_input(const godot::Ref<godot::InputEvent>& event) override;

    bool push(const godot::String& id, const godot::Dictionary& params);
    bool replace(const godot::String& id, const godot::Dictionary& params);
    bool reset_to(const godot::String& id, const godot::Dictionary& params);
    // Closes the top screen unless it is the root (a lone modal, such as
    // pause over play, does close); returns whether it closed.
    bool back();
    // Closes every screen (gameplay underneath takes over).
    void clear();
    // push / replace / reset / back are handled here; anything else is emitted.
    void perform(const godot::String& action, const godot::String& target, const godot::Dictionary& params);

    godot::String top_id() const;
    int depth() const { return static_cast<int>(entries_.size()); }
    bool is_empty() const { return entries_.empty(); }
    Screen* top() const { return entries_.empty() ? nullptr : entries_.back().screen; }

    void set_screens(const godot::Dictionary& v) { screens_ = v; }
    godot::Dictionary get_screens() const { return screens_; }
    void set_initial_screen(const godot::String& v) { initial_screen_ = v; }
    godot::String get_initial_screen() const { return initial_screen_; }
    void set_transition_seconds(double v) { transition_seconds_ = v; }
    double get_transition_seconds() const { return transition_seconds_; }

protected:
    static void _bind_methods();

private:
    struct Entry {
        Screen* screen;
        godot::ObjectID focus;  // focus owner when this screen was covered
    };

    Screen* instantiate(const godot::String& id);
    void open(Screen* screen, const godot::Dictionary& params);
    void close_top();
    void cover_top();
    void uncover_top();

    godot::Dictionary screens_;
    godot::String initial_screen_;
    double transition_seconds_ = 0.15;
    ui::NavStack nav_;
    std::vector<Entry> entries_;
};

}  // namespace montezuma::godot_binding
