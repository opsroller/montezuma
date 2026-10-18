// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "intro_screen.hpp"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace montezuma::godot_binding {

void IntroScreen::on_open(const Dictionary& params) {
    page_ = 0;
    show_page();
    Screen::on_open(params);
}

void IntroScreen::next_page() {
    if (page_ + 1 >= pages_.size()) {
        perform(finish_action_, finish_target_, {});
        return;
    }
    ++page_;
    show_page();
}

void IntroScreen::show_page() {
    if (auto* body = Object::cast_to<Label>(get_node_or_null(NodePath("%Body"))))
        body->set_text(page_ < pages_.size() ? pages_[page_] : String());
    if (auto* progress = Object::cast_to<Label>(get_node_or_null(NodePath("%Page")))) {
        Dictionary args;
        args["page"] = page_ + 1;
        args["count"] = pages_.size();
        progress->set_text(tr("intro.page").format(args));
    }
}

void IntroScreen::_bind_methods() {
    ClassDB::bind_method(D_METHOD("next_page"), &IntroScreen::next_page);
    ClassDB::bind_method(D_METHOD("set_pages", "keys"), &IntroScreen::set_pages);
    ClassDB::bind_method(D_METHOD("get_pages"), &IntroScreen::get_pages);
    ClassDB::bind_method(D_METHOD("set_finish_action", "action"), &IntroScreen::set_finish_action);
    ClassDB::bind_method(D_METHOD("get_finish_action"), &IntroScreen::get_finish_action);
    ClassDB::bind_method(D_METHOD("set_finish_target", "target"), &IntroScreen::set_finish_target);
    ClassDB::bind_method(D_METHOD("get_finish_target"), &IntroScreen::get_finish_target);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "pages"), "set_pages", "get_pages");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "finish_action"), "set_finish_action", "get_finish_action");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "finish_target"), "set_finish_target", "get_finish_target");
}

}  // namespace montezuma::godot_binding
