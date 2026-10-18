// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "credits_screen.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Text from the file must never be read as BBCode.
String escape(const String& s) { return s.replace("[", "[lb]"); }

// Escapes a line and converts [text](https://url) links and **bold**.
String inline_markup(const String& line) {
    String out;
    int64_t i = 0;
    const int64_t n = line.length();
    bool bold = false;
    while (i < n) {
        if (line[i] == '[') {
            const int64_t close = line.find("](", i);
            const int64_t end = close >= 0 ? line.find(")", close) : -1;
            if (close > i && end > close) {
                const String text = line.substr(i + 1, close - i - 1);
                const String url = line.substr(close + 2, end - close - 2);
                if (url.begins_with("https://") && !url.contains("]") && !url.contains("[")) {
                    out += "[url=" + url + "]" + escape(text) + "[/url]";
                    i = end + 1;
                    continue;
                }
            }
            out += "[lb]";
            ++i;
            continue;
        }
        if (line[i] == '*' && i + 1 < n && line[i + 1] == '*') {
            out += bold ? "[/b]" : "[b]";
            bold = !bold;
            i += 2;
            continue;
        }
        out += String::chr(line[i]);
        ++i;
    }
    if (bold) out += "[/b]";
    return out;
}

}  // namespace

String CreditsScreen::markdown_to_bbcode(const String& markdown) {
    String out;
    const PackedStringArray lines = markdown.replace("\r", "").split("\n");
    for (int64_t i = 0; i < lines.size(); ++i) {
        const String line = lines[i].strip_edges(false, true);
        if (line.begins_with("<!--")) continue;
        if (line.begins_with("## "))
            out += "\n[b]" + inline_markup(line.substr(3)) + "[/b]\n";
        else if (line.begins_with("# "))
            out += "[font_size=" + String::num_int64(40) + "][b]" + inline_markup(line.substr(2)) + "[/b][/font_size]\n";
        else if (line.begins_with("- ") || line.begins_with("* "))
            out += String::utf8("• ") + inline_markup(line.substr(2)) + "\n";
        else
            out += inline_markup(line) + "\n";
    }
    return out.strip_edges();
}

void CreditsScreen::on_open(const Dictionary& params) {
    auto* body = Object::cast_to<RichTextLabel>(get_node_or_null(NodePath("%Body")));
    if (body) {
        body->set_use_bbcode(true);
        if (FileAccess::file_exists(source_path_))
            body->set_text(markdown_to_bbcode(FileAccess::get_file_as_string(source_path_)));
        else
            body->set_text(escape(tr("credits.fallback")));
        if (!body->is_connected("meta_clicked", callable_mp(this, &CreditsScreen::on_meta_clicked)))
            body->connect("meta_clicked", callable_mp(this, &CreditsScreen::on_meta_clicked));
    }
    Screen::on_open(params);
}

void CreditsScreen::on_meta_clicked(const Variant& meta) {
    const String url = meta;
    if (url.begins_with("https://")) OS::get_singleton()->shell_open(url);
}

void CreditsScreen::_bind_methods() {
    ClassDB::bind_static_method("CreditsScreen", D_METHOD("markdown_to_bbcode", "markdown"), &CreditsScreen::markdown_to_bbcode);
    ClassDB::bind_method(D_METHOD("set_source_path", "path"), &CreditsScreen::set_source_path);
    ClassDB::bind_method(D_METHOD("get_source_path"), &CreditsScreen::get_source_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.md"), "set_source_path", "get_source_path");
}

}  // namespace montezuma::godot_binding
