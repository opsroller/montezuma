// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "screen_stack.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "screen.hpp"

using namespace godot;

namespace montezuma::godot_binding {

void ScreenStack::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    // Empty space passes clicks through to the game; screens stop them.
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    if (!initial_screen_.is_empty()) reset_to(initial_screen_, {});
}

void ScreenStack::_unhandled_input(const Ref<InputEvent>& event) {
    if (entries_.empty() || !event.is_valid()) return;
    if (event->is_action_pressed("ui_cancel")) {
        top()->back();
        get_viewport()->set_input_as_handled();
    }
}

Screen* ScreenStack::instantiate(const String& id) {
    if (id.is_empty() || !screens_.has(id)) {
        UtilityFunctions::push_error("ScreenStack: unknown screen id: ", id);
        return nullptr;
    }
    const String path = screens_[id];
    Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(path);
    if (scene.is_null()) {
        UtilityFunctions::push_error("ScreenStack: cannot load screen scene: ", path);
        return nullptr;
    }
    Node* node = scene->instantiate();
    auto* screen = Object::cast_to<Screen>(node);
    if (!screen) {
        UtilityFunctions::push_error("ScreenStack: scene root is not a Screen: ", path);
        if (node) node->queue_free();
        return nullptr;
    }
    if (screen->get_screen_id().is_empty()) screen->set_screen_id(id);
    return screen;
}

void ScreenStack::open(Screen* screen, const Dictionary& params) {
    add_child(screen);
    entries_.push_back({screen, ObjectID()});
    if (transition_seconds_ > 0.0) {
        screen->set_modulate(Color(1, 1, 1, 0));
        // Owned by the screen: freed with it, so it never outlives its target.
        screen->create_tween()->tween_property(screen, "modulate:a", 1.0, transition_seconds_);
    }
    screen->on_open(params);
    emit_signal("screen_opened", screen->get_screen_id());
}

void ScreenStack::close_top() {
    Entry e = entries_.back();
    entries_.pop_back();
    const String id = e.screen->get_screen_id();
    e.screen->on_close();
    remove_child(e.screen);
    e.screen->queue_free();
    emit_signal("screen_closed", id);
}

void ScreenStack::cover_top() {
    if (entries_.empty()) return;
    Entry& e = entries_.back();
    Control* focus = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
    e.focus = focus ? ObjectID(focus->get_instance_id()) : ObjectID();
    e.screen->on_cover();
}

void ScreenStack::uncover_top() {
    if (entries_.empty()) return;
    Entry& e = entries_.back();
    e.screen->set_inert(false);
    e.screen->show();
    e.screen->on_uncover();
    auto* focus = Object::cast_to<Control>(ObjectDB::get_instance(e.focus));
    if (focus && e.screen->is_ancestor_of(focus) && focus->is_visible_in_tree())
        focus->call_deferred("grab_focus");
    else
        e.screen->call_deferred("focus_default");
}

bool ScreenStack::push(const String& id, const Dictionary& params) {
    Screen* screen = instantiate(id);
    if (!screen) return false;
    cover_top();
    if (!entries_.empty()) {
        Screen* below = entries_.back().screen;
        // A modal keeps what it covers visible but inert; a screen hides it.
        if (screen->is_modal())
            below->set_inert(true);
        else
            below->hide();
    }
    nav_.push(id.utf8().get_data());
    open(screen, params);
    return true;
}

bool ScreenStack::replace(const String& id, const Dictionary& params) {
    Screen* screen = instantiate(id);
    if (!screen) return false;
    if (!entries_.empty()) close_top();
    nav_.replace(id.utf8().get_data());
    open(screen, params);
    return true;
}

bool ScreenStack::reset_to(const String& id, const Dictionary& params) {
    Screen* screen = instantiate(id);
    if (!screen) return false;
    while (!entries_.empty()) close_top();
    nav_.reset(id.utf8().get_data());
    open(screen, params);
    return true;
}

bool ScreenStack::back() {
    // A modal alone on the stack sits over gameplay (pause): back returns to play.
    if (entries_.size() == 1 && top()->is_modal()) {
        clear();
        return true;
    }
    if (!nav_.pop()) return false;
    close_top();
    uncover_top();
    return true;
}

void ScreenStack::clear() {
    if (entries_.empty()) return;
    while (!entries_.empty()) close_top();
    nav_ = ui::NavStack();
    emit_signal("emptied");
}

void ScreenStack::perform(const String& action, const String& target, const Dictionary& params) {
    if (action == "push")
        push(target, params);
    else if (action == "replace")
        replace(target, params);
    else if (action == "reset")
        reset_to(target, params);
    else if (action == "back")
        back();
    else
        emit_signal("action_requested", action, target, params);
}

String ScreenStack::top_id() const { return String::utf8(nav_.top().c_str()); }

void ScreenStack::_bind_methods() {
    ClassDB::bind_method(D_METHOD("push", "id", "params"), &ScreenStack::push, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("replace", "id", "params"), &ScreenStack::replace, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("reset_to", "id", "params"), &ScreenStack::reset_to, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("back"), &ScreenStack::back);
    ClassDB::bind_method(D_METHOD("clear"), &ScreenStack::clear);
    ClassDB::bind_method(D_METHOD("perform", "action", "target", "params"), &ScreenStack::perform, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("top_id"), &ScreenStack::top_id);
    ClassDB::bind_method(D_METHOD("depth"), &ScreenStack::depth);
    ClassDB::bind_method(D_METHOD("is_empty"), &ScreenStack::is_empty);

    ClassDB::bind_method(D_METHOD("set_screens", "screens"), &ScreenStack::set_screens);
    ClassDB::bind_method(D_METHOD("get_screens"), &ScreenStack::get_screens);
    ClassDB::bind_method(D_METHOD("set_initial_screen", "id"), &ScreenStack::set_initial_screen);
    ClassDB::bind_method(D_METHOD("get_initial_screen"), &ScreenStack::get_initial_screen);
    ClassDB::bind_method(D_METHOD("set_transition_seconds", "seconds"), &ScreenStack::set_transition_seconds);
    ClassDB::bind_method(D_METHOD("get_transition_seconds"), &ScreenStack::get_transition_seconds);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "screens"), "set_screens", "get_screens");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "initial_screen"), "set_initial_screen", "get_initial_screen");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "transition_seconds"), "set_transition_seconds", "get_transition_seconds");

    ADD_SIGNAL(MethodInfo("screen_opened", PropertyInfo(Variant::STRING, "id")));
    ADD_SIGNAL(MethodInfo("screen_closed", PropertyInfo(Variant::STRING, "id")));
    ADD_SIGNAL(MethodInfo("emptied"));
    ADD_SIGNAL(MethodInfo("action_requested", PropertyInfo(Variant::STRING, "action"), PropertyInfo(Variant::STRING, "target"),
                          PropertyInfo(Variant::DICTIONARY, "params")));
}

}  // namespace montezuma::godot_binding
