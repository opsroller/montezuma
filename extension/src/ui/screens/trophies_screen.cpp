// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "trophies_screen.hpp"

#include <godot_cpp/classes/container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

Label* label(const String& text, const StringName& variation) {
    auto* l = memnew(Label);
    l->set_text(text);
    l->set_theme_type_variation(variation);
    l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    return l;
}

}  // namespace

void TrophiesScreen::on_open(const Dictionary& params) {
    rebuild();
    Screen::on_open(params);
}

void TrophiesScreen::rebuild() {
    auto* grid = Object::cast_to<Container>(get_node_or_null(NodePath("%Grid")));
    if (!grid) return;
    for (int32_t i = grid->get_child_count() - 1; i >= 0; --i) {
        Node* child = grid->get_child(i);
        grid->remove_child(child);
        child->queue_free();
    }
    for (int64_t i = 0; i < trophy_ids_.size(); ++i) {
        const String key = "trophy." + trophy_ids_[i];
        auto* card = memnew(PanelContainer);
        card->set_theme_type_variation("CardPanel");
        // Focusable so a controller can walk (and scroll) the list.
        card->set_focus_mode(FOCUS_ALL);
        card->set_custom_minimum_size(Vector2(340, 0));
        card->set_h_size_flags(SIZE_EXPAND_FILL);
        auto* column = memnew(VBoxContainer);
        column->add_child(label(key + String(".name"), ""));
        column->add_child(label(key + String(".desc"), "CaptionLabel"));
        column->add_child(label("trophies.locked", "CaptionLabel"));
        card->add_child(column);
        grid->add_child(card);
    }
}

void TrophiesScreen::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_trophy_ids", "ids"), &TrophiesScreen::set_trophy_ids);
    ClassDB::bind_method(D_METHOD("get_trophy_ids"), &TrophiesScreen::get_trophy_ids);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "trophy_ids"), "set_trophy_ids", "get_trophy_ids");
}

}  // namespace montezuma::godot_binding
