// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "screen.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "screen_stack.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Plain char constants: a namespace-scope StringName would be built before Godot loads.
const char* const kSavedFocusMode = "_ui_saved_focus_mode";

Control* first_focusable(Node* node) {
    for (int64_t i = 0; i < node->get_child_count(); ++i) {
        Node* child = node->get_child(i);
        auto* c = Object::cast_to<Control>(child);
        if (c && !c->is_visible_in_tree()) continue;
        if (c && c->get_focus_mode() == Control::FOCUS_ALL) return c;
        if (Control* found = first_focusable(child)) return found;
    }
    return nullptr;
}

void set_inert_recursive(Node* node, bool inert) {
    for (int64_t i = 0; i < node->get_child_count(); ++i) {
        Node* child = node->get_child(i);
        if (auto* c = Object::cast_to<Control>(child)) {
            if (inert && !c->has_meta(kSavedFocusMode)) {
                c->set_meta(kSavedFocusMode, static_cast<int>(c->get_focus_mode()));
                c->set_focus_mode(Control::FOCUS_NONE);
            } else if (!inert && c->has_meta(kSavedFocusMode)) {
                c->set_focus_mode(static_cast<Control::FocusMode>(static_cast<int>(c->get_meta(kSavedFocusMode))));
                c->remove_meta(kSavedFocusMode);
            }
        }
        set_inert_recursive(child, inert);
    }
}

}  // namespace

void Screen::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    // A screen swallows pointer input so it never reaches the board below.
    set_mouse_filter(MOUSE_FILTER_STOP);
}

void Screen::on_open(const Dictionary& params) {
    params_ = params;
    if (pauses_game_) get_tree()->set_pause(true);
    call_deferred("focus_default");
}

void Screen::on_cover() {}

void Screen::on_uncover() {}

void Screen::on_close() {
    if (pauses_game_) get_tree()->set_pause(false);
}

ScreenStack* Screen::stack() const {
    for (Node* n = get_parent(); n; n = n->get_parent())
        if (auto* s = Object::cast_to<ScreenStack>(n)) return s;
    return nullptr;
}

void Screen::perform(const String& action, const String& target, const Dictionary& params) {
    if (ScreenStack* s = stack()) s->perform(action, target, params);
}

void Screen::back() {
    switch (back_behavior_) {
        case BACK_POP:
            perform("back", "", {});
            break;
        case BACK_ACTION:
            perform(back_action_, back_target_, {});
            break;
        case BACK_BLOCKED:
            break;
    }
}

void Screen::focus_default() {
    if (!is_inside_tree() || !is_visible_in_tree()) return;
    Control* target = nullptr;
    if (!default_focus_.is_empty()) target = Object::cast_to<Control>(get_node_or_null(default_focus_));
    if (!target || !target->is_visible_in_tree() || target->get_focus_mode() == FOCUS_NONE) target = first_focusable(this);
    if (target) target->grab_focus();
}

void Screen::set_inert(bool inert) { set_inert_recursive(this, inert); }

void Screen::_bind_methods() {
    ClassDB::bind_method(D_METHOD("perform", "action", "target", "params"), &Screen::perform, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("back"), &Screen::back);
    ClassDB::bind_method(D_METHOD("focus_default"), &Screen::focus_default);
    ClassDB::bind_method(D_METHOD("get_params"), &Screen::get_params);

    ClassDB::bind_method(D_METHOD("set_screen_id", "id"), &Screen::set_screen_id);
    ClassDB::bind_method(D_METHOD("get_screen_id"), &Screen::get_screen_id);
    ClassDB::bind_method(D_METHOD("set_default_focus", "path"), &Screen::set_default_focus);
    ClassDB::bind_method(D_METHOD("get_default_focus"), &Screen::get_default_focus);
    ClassDB::bind_method(D_METHOD("set_modal", "modal"), &Screen::set_modal);
    ClassDB::bind_method(D_METHOD("is_modal"), &Screen::is_modal);
    ClassDB::bind_method(D_METHOD("set_pauses_game", "pauses"), &Screen::set_pauses_game);
    ClassDB::bind_method(D_METHOD("get_pauses_game"), &Screen::get_pauses_game);
    ClassDB::bind_method(D_METHOD("set_back_behavior", "behavior"), &Screen::set_back_behavior);
    ClassDB::bind_method(D_METHOD("get_back_behavior"), &Screen::get_back_behavior);
    ClassDB::bind_method(D_METHOD("set_back_action", "action"), &Screen::set_back_action);
    ClassDB::bind_method(D_METHOD("get_back_action"), &Screen::get_back_action);
    ClassDB::bind_method(D_METHOD("set_back_target", "target"), &Screen::set_back_target);
    ClassDB::bind_method(D_METHOD("get_back_target"), &Screen::get_back_target);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "screen_id"), "set_screen_id", "get_screen_id");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "default_focus"), "set_default_focus", "get_default_focus");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "modal"), "set_modal", "is_modal");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pauses_game"), "set_pauses_game", "get_pauses_game");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "back_behavior", PROPERTY_HINT_ENUM, "Pop,Blocked,Action"), "set_back_behavior",
                 "get_back_behavior");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "back_action"), "set_back_action", "get_back_action");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "back_target"), "set_back_target", "get_back_target");

    BIND_ENUM_CONSTANT(BACK_POP);
    BIND_ENUM_CONSTANT(BACK_BLOCKED);
    BIND_ENUM_CONSTANT(BACK_ACTION);
}

}  // namespace montezuma::godot_binding
