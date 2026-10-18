// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "nav_button.hpp"

#include <algorithm>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "screen.hpp"
#include "ui_root.hpp"

using namespace godot;

namespace montezuma::godot_binding {

void NavButton::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    for (int64_t i = 0; i < hidden_on_.size(); ++i) {
        if (OS::get_singleton()->has_feature(hidden_on_[i])) {
            hide();
            set_focus_mode(FOCUS_NONE);
            break;
        }
    }
    Vector2 min = get_custom_minimum_size();
    set_custom_minimum_size(Vector2(min.x, std::max(min.y, kMinHeight)));
    connect("pressed", callable_mp(this, &NavButton::on_pressed));
}

void NavButton::play_cue(Node* from, const String& cue) {
    if (!from || !from->is_inside_tree()) return;
    Node* audio = from->get_tree()->get_root()->get_node_or_null(NodePath("GameAudio"));
    if (audio && audio->has_method("play")) audio->call("play", cue);
}

void NavButton::on_pressed() {
    play_cue(this, action_ == "back" ? "ui_back" : "ui_click");
    for (Node* n = get_parent(); n; n = n->get_parent()) {
        // Deferred: the action may free this button's screen.
        if (Object::cast_to<Screen>(n) || Object::cast_to<UiRoot>(n)) {
            n->call_deferred("perform", action_, target_, params_);
            return;
        }
    }
}

void NavButton::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_action", "action"), &NavButton::set_action);
    ClassDB::bind_method(D_METHOD("get_action"), &NavButton::get_action);
    ClassDB::bind_method(D_METHOD("set_target", "target"), &NavButton::set_target);
    ClassDB::bind_method(D_METHOD("get_target"), &NavButton::get_target);
    ClassDB::bind_method(D_METHOD("set_params", "params"), &NavButton::set_params);
    ClassDB::bind_method(D_METHOD("get_params"), &NavButton::get_params);
    ClassDB::bind_method(D_METHOD("set_hidden_on", "features"), &NavButton::set_hidden_on);
    ClassDB::bind_method(D_METHOD("get_hidden_on"), &NavButton::get_hidden_on);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "action", PROPERTY_HINT_ENUM_SUGGESTION,
                              "push,replace,reset,back,play,pause,resume,restart,quit_level,quit,url"),
                 "set_action", "get_action");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "target"), "set_target", "get_target");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "params"), "set_params", "get_params");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "hidden_on"), "set_hidden_on", "get_hidden_on");
}

}  // namespace montezuma::godot_binding
