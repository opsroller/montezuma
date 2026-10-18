// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "hud.hpp"


#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../nav_button.hpp"
#include "../ui_root.hpp"
#include "../ui_theme.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Contract signal -> handler method, connected when the game node has it.
struct Binding {
    const char* signal;
    const char* method;
};
constexpr Binding kBindings[] = {
    {"level_started", "on_level_started"},
    {"time_changed", "on_time_changed"},
    {"score_changed", "on_score_changed"},
    {"gems_changed", "on_gems_changed"},
    {"quota_changed", "on_quota_changed"},
    {"totem_changed", "on_totem_changed"},
    {"totem_charge_changed", "on_totem_charge_changed"},
    {"frenzy_changed", "on_frenzy_changed"},
    {"shuffles_changed", "on_shuffles_changed"},
    {"hint_cost_changed", "on_hint_cost_changed"},
    {"level_finished", "on_level_finished"},
};

// Placeholder piece colours (the art direction's kind.0..6) until they are UI tokens.
const char* const kKindFallback[] = {"#b3121f", "#e2661c", "#ffcc4d", "#2b9a57", "#2350c8", "#8a35c8", "#ecebf3"};

template <typename T>
T* unique(const Node* owner, const char* name) {
    return Object::cast_to<T>(owner->get_node_or_null(NodePath(String("%") + name)));
}

void clear_children(Node* node) {
    for (int32_t i = node->get_child_count() - 1; i >= 0; --i) {
        Node* child = node->get_child(i);
        node->remove_child(child);
        child->queue_free();
    }
}

Label* make_label(const String& text, const StringName& variation, bool translate) {
    auto* l = memnew(Label);
    l->set_text(text);
    l->set_theme_type_variation(variation);
    if (!translate) l->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
    return l;
}

String format(const String& key, const Dictionary& args, const Object* ctx) { return ctx->tr(key).format(args); }

}  // namespace

void Hud::_ready() {
    if (kind_names_.is_empty()) kind_names_ = PackedStringArray({"red", "orange", "yellow", "green", "blue", "purple", "white"});
    if (totem_ids_.is_empty()) totem_ids_ = PackedStringArray({"fire", "time", "gold", "harvest", "ward", "gift", "paint"});
    if (Engine::get_singleton()->is_editor_hint()) return;
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    hide();
    if (auto* b = unique<BaseButton>(this, "Shuffle")) b->connect("pressed", callable_mp(this, &Hud::send_command).bind("shuffle"));
    if (auto* b = unique<BaseButton>(this, "Hint")) b->connect("pressed", callable_mp(this, &Hud::send_command).bind("buy_hint"));
    get_tree()->connect("node_added", callable_mp(this, &Hud::on_node_added));
    if (Node* existing = get_tree()->get_first_node_in_group(game_group_)) bind_game(existing);
}

void Hud::_notification(int what) {
    if (what == NOTIFICATION_TRANSLATION_CHANGED && is_node_ready()) refresh_all();
}

void Hud::set_warning_seconds(double v) {
    warning_seconds_ = v;
    model_.set_warning_seconds(v);
}

Node* Hud::game() const { return Object::cast_to<Node>(ObjectDB::get_instance(game_)); }

void Hud::on_node_added(Node* node) {
    if (node && node->is_in_group(game_group_)) bind_game(node);
}

void Hud::bind_game(Node* node) {
    if (!node) return;
    if (Node* old = game()) {
        for (const auto& b : kBindings) {
            const Callable c(this, b.method);
            if (old->is_connected(b.signal, c)) old->disconnect(b.signal, c);
        }
    }
    game_ = ObjectID(node->get_instance_id());
    for (const auto& b : kBindings)
        if (node->has_signal(b.signal)) node->connect(b.signal, Callable(this, b.method));
}

void Hud::send_command(const String& method) {
    NavButton::play_cue(this, "ui_click");
    if (Node* g = game(); g && g->has_method(method)) g->call(method);
}

void Hud::on_level_started(const Variant& level, const String& difficulty) {
    model_.reset();
    model_.set_warning_seconds(warning_seconds_);
    for (const char* box : {"Quotas", "Totems"})
        if (auto* c = unique<Container>(this, box)) clear_children(c);
    if (level.get_type() == Variant::DICTIONARY) {
        const Dictionary d = level;
        const Variant limits = d.get("time_limit", Variant());
        if (limits.get_type() == Variant::DICTIONARY) model_.set_time_limit(Dictionary(limits).get(difficulty, 0.0));
        const Variant goal = d.get("gem_goal", Variant());
        if (goal.get_type() == Variant::DICTIONARY) model_.set_gems(0, Dictionary(goal).get(difficulty, 0));
        const Variant quotas = d.get("quotas", Variant());
        if (quotas.get_type() == Variant::DICTIONARY) {
            const Dictionary q = quotas;
            const Array keys = q.keys();
            for (int64_t i = 0; i < keys.size(); ++i) {
                const int64_t kind = kind_names_.find(String(keys[i]));
                if (kind >= 0) model_.set_quota(static_cast<int>(kind), q[keys[i]]);
            }
        }
        // Meters for the level's totems, in kind order, before any charge arrives.
        const Variant unlocked = d.get("totems_unlocked", Variant());
        if (unlocked.get_type() == Variant::ARRAY) {
            const Array ids = unlocked;
            for (int64_t i = 0; i < totem_ids_.size(); ++i)
                if (ids.has(totem_ids_[i])) model_.set_totem(totem_ids_[i].utf8().get_data(), 0.0, 0);
        }
    }
    refresh_all();
    show();
}

void Hud::on_time_changed(double seconds_left) {
    if (model_.set_time(seconds_left)) refresh_clock();
}

void Hud::on_score_changed(int score) {
    if (model_.set_score(score)) refresh_score();
}

void Hud::on_gems_changed(int collected, int goal) {
    if (model_.set_gems(collected, goal)) refresh_gems();
}

void Hud::on_quota_changed(int kind, int remaining) {
    const bool is_new = !model_.quotas().count(kind);
    if (!model_.set_quota(kind, remaining)) return;
    if (is_new) {
        refresh_quotas();
        return;
    }
    if (auto* box = unique<Container>(this, "Quotas"))
        if (auto* count = Object::cast_to<Label>(box->get_node_or_null(NodePath("Kind" + String::num_int64(kind) + "/Count"))))
            count->set_text(remaining > 0 ? String::num_int64(remaining) : tr("hud.quota.done"));
}

void Hud::on_totem_changed(const String& totem_id, double charge, int uses_left) {
    if (model_.set_totem(totem_id.utf8().get_data(), charge, uses_left)) refresh_totem(totem_id);
}

void Hud::on_totem_charge_changed(const String& totem_id, double charge) {
    const auto& totems = model_.totems();
    const auto it = totems.find(totem_id.utf8().get_data());
    on_totem_changed(totem_id, charge, it == totems.end() ? 0 : it->second.uses_left);
}

void Hud::on_frenzy_changed(double bar, double seconds_left) {
    if (model_.set_frenzy(bar, seconds_left)) refresh_frenzy();
}

void Hud::on_shuffles_changed(int left) {
    if (model_.set_shuffles(left)) refresh_buttons();
}

void Hud::on_hint_cost_changed(int cost, bool affordable) {
    if (model_.set_hint(cost, affordable)) refresh_buttons();
}

void Hud::on_level_finished(int outcome, int score) {
    hide();
    for (Node* n = get_parent(); n; n = n->get_parent()) {
        if (auto* root = Object::cast_to<UiRoot>(n)) {
            root->show_results(outcome, score, String());
            return;
        }
    }
}

void Hud::refresh_all() {
    refresh_clock();
    refresh_score();
    refresh_gems();
    refresh_quotas();
    // Kind order first, then any id the game sends that this HUD does not list.
    for (int64_t i = 0; i < totem_ids_.size(); ++i) refresh_totem(totem_ids_[i]);
    for (const auto& [id, totem] : model_.totems()) refresh_totem(String::utf8(id.c_str()));
    refresh_frenzy();
    refresh_buttons();
}

void Hud::refresh_clock() {
    if (auto* clock = unique<Label>(this, "Clock")) {
        clock->set_text(String::utf8(model_.clock().c_str()));
        // Low time: danger colour plus the game's time_warning cue, never colour alone.
        clock->set_theme_type_variation(model_.time_warning() ? "DangerLabel" : "HeadingLabel");
    }
    if (auto* bar = unique<ProgressBar>(this, "TimeBar")) bar->set_value(model_.time_fraction() * 100.0);
}

void Hud::refresh_score() {
    Dictionary args;
    args["score"] = model_.score();
    if (auto* score = unique<Label>(this, "Score")) score->set_text(format("hud.score", args, this));
}

void Hud::refresh_gems() {
    Dictionary args;
    args["collected"] = model_.gems();
    args["goal"] = model_.gem_goal();
    if (auto* gems = unique<Label>(this, "Gems")) {
        gems->set_visible(model_.gem_goal() > 0);
        gems->set_text(format("hud.gems", args, this));
    }
}

void Hud::refresh_quotas() {
    auto* box = unique<Container>(this, "Quotas");
    if (!box) return;
    clear_children(box);
    if (auto* panel = unique<CanvasItem>(this, "QuotaPanel")) panel->set_visible(!model_.quotas().empty());
    Dictionary tokens;
    for (Node* n = get_parent(); n; n = n->get_parent())
        if (auto* root = Object::cast_to<UiRoot>(n)) {
            tokens = root->get_tokens();
            break;
        }
    for (const auto& [kind, remaining] : model_.quotas()) {
        const String name = kind >= 0 && kind < kind_names_.size() ? kind_names_[kind] : String::num_int64(kind);
        auto* row = memnew(HBoxContainer);
        row->set_name("Kind" + String::num_int64(kind));
        auto* swatch = memnew(ColorRect);
        const Color fallback = kind >= 0 && kind < 7 ? Color::html(kKindFallback[kind]) : Color(1, 1, 1);
        swatch->set_color(UiTheme::token_color(tokens, "color.kind." + name, fallback));
        swatch->set_custom_minimum_size(Vector2(24, 24));
        swatch->set_v_size_flags(SIZE_SHRINK_CENTER);
        row->add_child(swatch);
        // The kind's name in words, so quotas never rely on colour alone.
        Label* label = make_label("kind." + name, "", true);
        label->set_h_size_flags(SIZE_EXPAND_FILL);
        row->add_child(label);
        Label* count = make_label(remaining > 0 ? String::num_int64(remaining) : tr("hud.quota.done"), "", false);
        count->set_name("Count");
        row->add_child(count);
        box->add_child(row);
    }
}

void Hud::refresh_totem(const String& id) {
    auto* box = unique<Container>(this, "Totems");
    const auto it = model_.totems().find(id.utf8().get_data());
    if (!box || it == model_.totems().end()) return;
    const String row_name = "Totem_" + id;
    Node* row = box->get_node_or_null(NodePath(row_name));
    // Rows are made once per totem and then only updated: charge changes often.
    if (!row) {
        auto* column = memnew(VBoxContainer);
        column->set_name(row_name);
        column->add_theme_constant_override("separation", 2);
        auto* head = memnew(HBoxContainer);
        head->set_name("Head");
        Label* name = make_label("totem." + id, "CaptionLabel", true);
        name->set_h_size_flags(SIZE_EXPAND_FILL);
        head->add_child(name);
        Label* state = make_label("", "CaptionLabel", false);
        state->set_name("State");
        head->add_child(state);
        column->add_child(head);
        auto* bar = memnew(ProgressBar);
        bar->set_name("Bar");
        bar->set_show_percentage(false);
        bar->set_custom_minimum_size(Vector2(0, 10));
        column->add_child(bar);
        box->add_child(column);
        row = column;
    }
    if (auto* bar = Object::cast_to<ProgressBar>(row->get_node_or_null(NodePath("Bar")))) bar->set_value(it->second.charge * 100.0);
    // Ready and uses left in words, not only a full bar.
    if (auto* state = Object::cast_to<Label>(row->get_node_or_null(NodePath("Head/State")))) {
        Dictionary args;
        args["uses"] = it->second.uses_left;
        state->set_text(format(it->second.charge >= 1.0 ? "hud.totem.ready" : "hud.totem.uses", args, this));
    }
}

void Hud::refresh_frenzy() {
    if (auto* bar = unique<ProgressBar>(this, "FrenzyBar")) bar->set_value(model_.frenzy_bar() * 100.0);
    if (auto* label = unique<Label>(this, "Frenzy")) {
        Dictionary args;
        args["seconds"] = model_.frenzy_seconds();
        label->set_text(model_.frenzy_on() ? format("hud.frenzy.on", args, this) : tr("hud.frenzy"));
        label->set_theme_type_variation(model_.frenzy_on() ? "HeadingLabel" : "CaptionLabel");
    }
}

void Hud::refresh_buttons() {
    if (auto* shuffle = unique<Button>(this, "Shuffle")) {
        Dictionary args;
        args["left"] = model_.shuffles();
        shuffle->set_text(format("hud.shuffle", args, this));
        shuffle->set_disabled(model_.shuffles() <= 0);
    }
    if (auto* hint = unique<Button>(this, "Hint")) {
        Dictionary args;
        args["cost"] = model_.hint_cost();
        hint->set_text(format("hud.hint", args, this));
        hint->set_disabled(!model_.hint_affordable());
    }
}

void Hud::_bind_methods() {
    ClassDB::bind_method(D_METHOD("bind_game", "game"), &Hud::bind_game);
    ClassDB::bind_method(D_METHOD("on_level_started", "level", "difficulty"), &Hud::on_level_started);
    ClassDB::bind_method(D_METHOD("on_time_changed", "seconds_left"), &Hud::on_time_changed);
    ClassDB::bind_method(D_METHOD("on_score_changed", "score"), &Hud::on_score_changed);
    ClassDB::bind_method(D_METHOD("on_gems_changed", "collected", "goal"), &Hud::on_gems_changed);
    ClassDB::bind_method(D_METHOD("on_quota_changed", "kind", "remaining"), &Hud::on_quota_changed);
    ClassDB::bind_method(D_METHOD("on_totem_changed", "totem_id", "charge", "uses_left"), &Hud::on_totem_changed);
    ClassDB::bind_method(D_METHOD("on_totem_charge_changed", "totem_id", "charge"), &Hud::on_totem_charge_changed);
    ClassDB::bind_method(D_METHOD("on_frenzy_changed", "bar", "seconds_left"), &Hud::on_frenzy_changed);
    ClassDB::bind_method(D_METHOD("on_shuffles_changed", "left"), &Hud::on_shuffles_changed);
    ClassDB::bind_method(D_METHOD("on_hint_cost_changed", "cost", "affordable"), &Hud::on_hint_cost_changed);
    ClassDB::bind_method(D_METHOD("on_level_finished", "outcome", "score"), &Hud::on_level_finished);

    ClassDB::bind_method(D_METHOD("set_game_group", "group"), &Hud::set_game_group);
    ClassDB::bind_method(D_METHOD("get_game_group"), &Hud::get_game_group);
    ClassDB::bind_method(D_METHOD("set_warning_seconds", "seconds"), &Hud::set_warning_seconds);
    ClassDB::bind_method(D_METHOD("get_warning_seconds"), &Hud::get_warning_seconds);
    ClassDB::bind_method(D_METHOD("set_kind_names", "names"), &Hud::set_kind_names);
    ClassDB::bind_method(D_METHOD("get_kind_names"), &Hud::get_kind_names);
    ClassDB::bind_method(D_METHOD("set_totem_ids", "ids"), &Hud::set_totem_ids);
    ClassDB::bind_method(D_METHOD("get_totem_ids"), &Hud::get_totem_ids);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "game_group"), "set_game_group", "get_game_group");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "warning_seconds"), "set_warning_seconds", "get_warning_seconds");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "kind_names"), "set_kind_names", "get_kind_names");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "totem_ids"), "set_totem_ids", "get_totem_ids");
}

}  // namespace montezuma::godot_binding
