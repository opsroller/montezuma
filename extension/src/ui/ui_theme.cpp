// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "ui_theme.hpp"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr int kMaxAliasDepth = 8;

// Follows a dotted path through nested groups to a token's $value,
// resolving "{group.token}" aliases. Returns nil when absent.
Variant resolve(const Dictionary& tokens, const String& path, int depth = 0) {
    if (depth > kMaxAliasDepth) return Variant();
    Variant node = tokens;
    const PackedStringArray parts = path.split(".");
    for (int64_t i = 0; i < parts.size(); ++i) {
        if (node.get_type() != Variant::DICTIONARY) return Variant();
        const Dictionary d = node;
        if (!d.has(parts[i])) return Variant();
        node = d[parts[i]];
    }
    if (node.get_type() == Variant::DICTIONARY && Dictionary(node).has("$value")) node = Dictionary(node)["$value"];
    if (node.get_type() == Variant::STRING) {
        const String s = node;
        if (s.begins_with("{") && s.ends_with("}")) return resolve(tokens, s.substr(1, s.length() - 2), depth + 1);
    }
    return node;
}

// Numbers, "16px" / "200ms" strings and DTCG {value, unit} objects.
bool as_number(const Variant& v, double& out) {
    switch (v.get_type()) {
        case Variant::INT:
        case Variant::FLOAT:
            out = v;
            return true;
        case Variant::STRING: {
            const String s = String(v).strip_edges();
            if (!s.is_valid_float() && !s.trim_suffix("px").trim_suffix("ms").is_valid_float()) {
                if (s.ends_with("s") && s.trim_suffix("s").is_valid_float()) {
                    out = s.trim_suffix("s").to_float() * 1000.0;  // seconds to ms
                    return true;
                }
                return false;
            }
            out = s.trim_suffix("px").trim_suffix("ms").to_float();
            return true;
        }
        case Variant::DICTIONARY: {
            const Dictionary d = v;
            if (!d.has("value")) return false;
            double n = 0.0;
            if (!as_number(d["value"], n)) return false;
            if (String(d.get("unit", "")) == "s") n *= 1000.0;
            out = n;
            return true;
        }
        default:
            return false;
    }
}

// "#rrggbb[aa]" strings and DTCG color objects ({colorSpace, components, alpha, hex}).
bool as_color(const Variant& v, Color& out) {
    if (v.get_type() == Variant::STRING) {
        const String s = v;
        if (!Color::html_is_valid(s)) return false;
        out = Color::html(s);
        return true;
    }
    if (v.get_type() != Variant::DICTIONARY) return false;
    const Dictionary d = v;
    const String space = d.get("colorSpace", "srgb");
    if (space == "srgb" && d.has("components")) {
        const Array c = d["components"];
        if (c.size() >= 3) {
            out = Color(c[0], c[1], c[2], d.get("alpha", 1.0));
            return true;
        }
    }
    if (d.has("hex") && Color::html_is_valid(d["hex"])) {
        out = Color::html(d["hex"]);
        out.a = d.get("alpha", 1.0);
        return true;
    }
    return false;
}

struct Palette {
    Color surface, surface_raised, text, text_muted, text_on_accent, accent, accent_pressed, focus, danger, scrim;
    double caption, body, heading, title;
    double space[7];
    double radius, focus_width;
    Ref<Font> body_font, display_font;
};

Ref<StyleBoxFlat> box(const Color& bg, double radius, double pad_x, double pad_y) {
    Ref<StyleBoxFlat> b;
    b.instantiate();
    b->set_bg_color(bg);
    b->set_corner_radius_all(static_cast<int32_t>(radius));
    b->set_content_margin(SIDE_LEFT, static_cast<float>(pad_x));
    b->set_content_margin(SIDE_RIGHT, static_cast<float>(pad_x));
    b->set_content_margin(SIDE_TOP, static_cast<float>(pad_y));
    b->set_content_margin(SIDE_BOTTOM, static_cast<float>(pad_y));
    return b;
}

Ref<StyleBoxFlat> focus_ring(const Palette& p) {
    Ref<StyleBoxFlat> b;
    b.instantiate();
    b->set_draw_center(false);
    b->set_border_color(p.focus);
    b->set_border_width_all(static_cast<int32_t>(p.focus_width));
    b->set_corner_radius_all(static_cast<int32_t>(p.radius + p.focus_width));
    b->set_expand_margin_all(static_cast<float>(p.focus_width + 1.0));
    return b;
}

Ref<Font> load_font(const String& path) {
    if (path.is_empty() || !ResourceLoader::get_singleton()->exists(path)) return {};
    return ResourceLoader::get_singleton()->load(path);
}

void button_like(Theme& t, const StringName& type, const Palette& p) {
    const double px = p.space[4], py = p.space[2];
    t.set_stylebox("normal", type, box(p.surface_raised, p.radius, px, py));
    t.set_stylebox("hover", type, box(p.surface_raised.lightened(0.12f), p.radius, px, py));
    t.set_stylebox("pressed", type, box(p.accent_pressed, p.radius, px, py));
    t.set_stylebox("hover_pressed", type, box(p.accent_pressed, p.radius, px, py));
    Color disabled = p.surface_raised;
    disabled.a = 0.5f;
    t.set_stylebox("disabled", type, box(disabled, p.radius, px, py));
    t.set_stylebox("focus", type, focus_ring(p));
    t.set_color("font_color", type, p.text);
    t.set_color("font_hover_color", type, p.text);
    t.set_color("font_focus_color", type, p.text);
    t.set_color("font_pressed_color", type, p.text_on_accent);
    t.set_color("font_hover_pressed_color", type, p.text_on_accent);
    t.set_color("font_disabled_color", type, p.text_muted);
    t.set_font_size("font_size", type, static_cast<int32_t>(p.body));
}

}  // namespace

Dictionary UiTheme::load_tokens(const String& path) {
    if (!FileAccess::file_exists(path)) {
        UtilityFunctions::push_error("UiTheme: tokens file not found: ", path);
        return {};
    }
    const Variant parsed = JSON::parse_string(FileAccess::get_file_as_string(path));
    if (parsed.get_type() != Variant::DICTIONARY) {
        UtilityFunctions::push_error("UiTheme: tokens file is not a JSON object: ", path);
        return {};
    }
    return parsed;
}

Color UiTheme::token_color(const Dictionary& tokens, const String& path, const Color& fallback) {
    Color c;
    return as_color(resolve(tokens, path), c) ? c : fallback;
}

double UiTheme::token_number(const Dictionary& tokens, const String& path, double fallback) {
    double n = 0.0;
    return as_number(resolve(tokens, path), n) && std::isfinite(n) ? n : fallback;
}

String UiTheme::token_string(const Dictionary& tokens, const String& path, const String& fallback) {
    const Variant v = resolve(tokens, path);
    return v.get_type() == Variant::STRING ? String(v) : fallback;
}

Ref<Theme> UiTheme::build(const Dictionary& tokens, double text_scale) {
    if (!std::isfinite(text_scale) || text_scale <= 0.0) text_scale = 1.0;
    const auto& d = tokens;
    Palette p;
    // Fallbacks are the placeholder palette, so a partial tokens file still
    // yields a usable, contrast-checked theme.
    p.surface = token_color(d, "color.surface", Color::html("#1d1813"));
    p.surface_raised = token_color(d, "color.surface-raised", Color::html("#3a3027"));
    p.text = token_color(d, "color.text", Color::html("#f5ecd9"));
    p.text_muted = token_color(d, "color.text-muted", Color::html("#c8b9a0"));
    p.text_on_accent = token_color(d, "color.text-on-accent", Color::html("#1d1813"));
    p.accent = token_color(d, "color.accent", Color::html("#e0a83a"));
    p.accent_pressed = token_color(d, "color.accent-pressed", Color::html("#f2c35f"));
    p.focus = token_color(d, "color.focus", Color::html("#7fd4ff"));
    p.danger = token_color(d, "color.danger", Color::html("#ff8a7a"));
    p.scrim = token_color(d, "color.scrim", Color(0.0f, 0.0f, 0.0f, 0.6f));
    p.caption = std::round(token_number(d, "font.size.caption", 16) * text_scale);
    p.body = std::round(token_number(d, "font.size.body", 22) * text_scale);
    // Large type is already readable: headings grow at half and titles at a
    // quarter of the body rate, so 200% body text still fits the screen.
    p.heading = std::round(token_number(d, "font.size.heading", 32) * (1.0 + (text_scale - 1.0) * 0.5));
    p.title = std::round(token_number(d, "font.size.title", 56) * (1.0 + (text_scale - 1.0) * 0.25));
    const double space_fallback[7] = {0, 4, 8, 12, 16, 24, 40};
    for (int i = 0; i < 7; ++i) p.space[i] = token_number(d, "space." + String::num_int64(i), space_fallback[i]);
    p.radius = token_number(d, "radius.control", 8);
    p.focus_width = std::max(2.0, token_number(d, "border.focus-width", 3));
    p.body_font = load_font(token_string(d, "font.family.body", ""));
    p.display_font = load_font(token_string(d, "font.family.display", ""));

    Ref<Theme> t;
    t.instantiate();
    t->set_default_font_size(static_cast<int32_t>(p.body));
    if (p.body_font.is_valid()) t->set_default_font(p.body_font);

    t->set_color("font_color", "Label", p.text);
    t->set_font_size("font_size", "Label", static_cast<int32_t>(p.body));
    struct Variation { const char* name; double size; Color color; bool display; };
    const Variation labels[] = {
        {"TitleLabel", p.title, p.accent, true},
        {"HeadingLabel", p.heading, p.text, true},
        {"CaptionLabel", p.caption, p.text_muted, false},
        {"DangerLabel", p.body, p.danger, false},
    };
    for (const auto& v : labels) {
        t->set_type_variation(v.name, "Label");
        t->set_font_size("font_size", v.name, static_cast<int32_t>(v.size));
        t->set_color("font_color", v.name, v.color);
        if (v.display && p.display_font.is_valid()) t->set_font("font", v.name, p.display_font);
    }

    for (const char* type : {"Button", "OptionButton", "CheckButton", "CheckBox", "MenuButton"}) button_like(**t, type, p);
    // Primary action: accent fill, dark text.
    t->set_type_variation("PrimaryButton", "Button");
    t->set_stylebox("normal", "PrimaryButton", box(p.accent, p.radius, p.space[4], p.space[2]));
    t->set_stylebox("hover", "PrimaryButton", box(p.accent.lightened(0.15f), p.radius, p.space[4], p.space[2]));
    t->set_color("font_color", "PrimaryButton", p.text_on_accent);
    t->set_color("font_hover_color", "PrimaryButton", p.text_on_accent);
    t->set_color("font_focus_color", "PrimaryButton", p.text_on_accent);

    t->set_stylebox("panel", "PanelContainer", box(p.surface, p.radius * 2, p.space[5], p.space[5]));
    t->set_stylebox("panel", "Panel", box(p.surface, p.radius * 2, 0, 0));
    t->set_type_variation("CardPanel", "PanelContainer");
    t->set_stylebox("panel", "CardPanel", box(p.surface_raised, p.radius, p.space[3], p.space[3]));
    t->set_stylebox("focus", "CardPanel", focus_ring(p));
    t->set_type_variation("ScrimPanel", "Panel");
    t->set_stylebox("panel", "ScrimPanel", box(p.scrim, 0, 0, 0));

    t->set_stylebox("background", "ProgressBar", box(p.surface_raised, p.radius, 0, 0));
    t->set_stylebox("fill", "ProgressBar", box(p.accent, p.radius, 0, 0));
    t->set_color("font_color", "ProgressBar", p.text);

    Ref<StyleBoxFlat> track = box(p.surface_raised, p.radius, 0, p.space[1]);
    t->set_stylebox("slider", "HSlider", track);
    t->set_stylebox("grabber_area", "HSlider", box(p.accent, p.radius, 0, p.space[1]));
    t->set_stylebox("grabber_area_highlight", "HSlider", box(p.accent_pressed, p.radius, 0, p.space[1]));
    t->set_stylebox("focus", "HSlider", focus_ring(p));

    t->set_color("default_color", "RichTextLabel", p.text);
    t->set_font_size("normal_font_size", "RichTextLabel", static_cast<int32_t>(p.body));
    t->set_font_size("bold_font_size", "RichTextLabel", static_cast<int32_t>(p.body));
    t->set_stylebox("focus", "RichTextLabel", focus_ring(p));
    t->set_stylebox("normal", "RichTextLabel", box(Color(0, 0, 0, 0), 0, p.space[2], p.space[2]));

    t->set_stylebox("panel", "PopupMenu", box(p.surface_raised, p.radius, p.space[2], p.space[2]));
    t->set_stylebox("hover", "PopupMenu", box(p.accent, p.radius, 0, 0));
    t->set_color("font_color", "PopupMenu", p.text);
    t->set_color("font_hover_color", "PopupMenu", p.text_on_accent);
    t->set_font_size("font_size", "PopupMenu", static_cast<int32_t>(p.body));
    t->set_color("font_color", "TooltipLabel", p.text);
    t->set_stylebox("panel", "TooltipPanel", box(p.surface_raised, p.radius, p.space[2], p.space[1]));

    const int32_t gap = static_cast<int32_t>(p.space[3]);
    t->set_constant("separation", "VBoxContainer", gap);
    t->set_constant("separation", "HBoxContainer", gap);
    t->set_constant("h_separation", "GridContainer", gap);
    t->set_constant("v_separation", "GridContainer", gap);
    t->set_constant("h_separation", "HFlowContainer", gap);
    t->set_constant("v_separation", "HFlowContainer", gap);
    t->set_type_variation("ScreenMargin", "MarginContainer");
    for (const char* side : {"margin_left", "margin_right", "margin_top", "margin_bottom"})
        t->set_constant(side, "ScreenMargin", static_cast<int32_t>(p.space[6]));
    return t;
}

void UiTheme::_bind_methods() {
    ClassDB::bind_static_method("UiTheme", D_METHOD("load_tokens", "path"), &UiTheme::load_tokens);
    ClassDB::bind_static_method("UiTheme", D_METHOD("build", "tokens", "text_scale"), &UiTheme::build);
    ClassDB::bind_static_method("UiTheme", D_METHOD("token_color", "tokens", "path", "fallback"), &UiTheme::token_color);
    ClassDB::bind_static_method("UiTheme", D_METHOD("token_number", "tokens", "path", "fallback"), &UiTheme::token_number);
    ClassDB::bind_static_method("UiTheme", D_METHOD("token_string", "tokens", "path", "fallback"), &UiTheme::token_string);
}

}  // namespace montezuma::godot_binding
