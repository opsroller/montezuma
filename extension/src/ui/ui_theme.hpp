// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace montezuma::godot_binding {

// Builds the game's Godot Theme from a design tokens file in the W3C Design
// Tokens Community Group format ($value / $type, nested groups, "{a.b}"
// aliases). Screens reference theme names, never raw values, so swapping
// the tokens file restyles every screen. Missing tokens fall back to the
// built-in placeholder values and are reported once as a warning.
class UiTheme : public godot::RefCounted {
    GDCLASS(UiTheme, godot::RefCounted)

public:
    // Parsed tokens, or an empty Dictionary (with an error) when the file is
    // missing or not valid JSON.
    static godot::Dictionary load_tokens(const godot::String& path);
    // text_scale multiplies every font size (the player's text-size setting).
    static godot::Ref<godot::Theme> build(const godot::Dictionary& tokens, double text_scale);

    // Token lookups by dotted path; exposed for screens that draw their own.
    static godot::Color token_color(const godot::Dictionary& tokens, const godot::String& path, const godot::Color& fallback);
    static double token_number(const godot::Dictionary& tokens, const godot::String& path, double fallback);
    static godot::String token_string(const godot::Dictionary& tokens, const godot::String& path, const godot::String& fallback);

protected:
    static void _bind_methods();
};

}  // namespace montezuma::godot_binding
