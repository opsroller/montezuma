// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/variant/string.hpp>

#include "../screen.hpp"

namespace montezuma::godot_binding {

// Shows the project's single credits list (CREDITS.md) in the %Body
// RichTextLabel, converting its Markdown headings, bullets and links.
// Falls back to the credits.fallback string when the file is not packaged.
class CreditsScreen : public Screen {
    GDCLASS(CreditsScreen, Screen)

public:
    void on_open(const godot::Dictionary& params) override;

    void set_source_path(const godot::String& v) { source_path_ = v; }
    godot::String get_source_path() const { return source_path_; }

    // Markdown subset (#/## headings, - bullets, [text](https://...) links,
    // **bold**) to escaped BBCode. Public for tests.
    static godot::String markdown_to_bbcode(const godot::String& markdown);

protected:
    static void _bind_methods();

private:
    void on_meta_clicked(const godot::Variant& meta);

    godot::String source_path_ = "res://CREDITS.md";
};

}  // namespace montezuma::godot_binding
