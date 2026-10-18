// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "../model/hud_model.hpp"

namespace montezuma::godot_binding {

// The in-level HUD: score, gem counter, timer, quotas (only on levels that
// have them), the seven totem meters, the Score Frenzy bar, and the
// shuffle and hint buttons. A read-only view of the game: it binds by
// signal name (the Game signals contract) to the node in group game_group
// as soon as one enters the tree, redraws only when a shown value changes,
// and sends the commands shuffle() and buy_hint() back to that node.
// Scene nodes: %Score, %Gems, %Clock, %TimeBar, %QuotaPanel, %Quotas,
// %Totems, %FrenzyBar, %Frenzy, %Shuffle, %Hint.
class Hud : public godot::Control {
    GDCLASS(Hud, godot::Control)

public:
    void _ready() override;

    // Connects to a game node's signals (any it has), replacing a previous one.
    void bind_game(godot::Node* game);

    // Game signal handlers, bound so tests and stubs can drive the HUD.
    void on_level_started(const godot::Variant& level, const godot::String& difficulty);
    void on_time_changed(double seconds_left);
    void on_score_changed(int score);
    void on_gems_changed(int collected, int goal);
    void on_quota_changed(int kind, int remaining);
    void on_totem_changed(const godot::String& totem_id, double charge, int uses_left);
    void on_totem_charge_changed(const godot::String& totem_id, double charge);
    void on_frenzy_changed(double bar, double seconds_left);
    void on_shuffles_changed(int left);
    void on_hint_cost_changed(int cost, bool affordable);
    void on_level_finished(int outcome, int score);

    void set_game_group(const godot::String& v) { game_group_ = v; }
    godot::String get_game_group() const { return game_group_; }
    void set_warning_seconds(double v);
    double get_warning_seconds() const { return warning_seconds_; }
    void set_kind_names(const godot::PackedStringArray& v) { kind_names_ = v; }
    godot::PackedStringArray get_kind_names() const { return kind_names_; }
    void set_totem_ids(const godot::PackedStringArray& v) { totem_ids_ = v; }
    godot::PackedStringArray get_totem_ids() const { return totem_ids_; }

protected:
    static void _bind_methods();
    void _notification(int what);

private:
    void on_node_added(godot::Node* node);
    void send_command(const godot::String& method);
    void refresh_all();
    void refresh_clock();
    void refresh_score();
    void refresh_gems();
    void refresh_quotas();
    void refresh_totem(const godot::String& id);
    void refresh_frenzy();
    void refresh_buttons();
    godot::Node* game() const;

    ui::HudModel model_;
    godot::ObjectID game_;
    godot::String game_group_ = "match3_game";
    double warning_seconds_ = ui::HudModel::kDefaultWarningSeconds;
    godot::PackedStringArray kind_names_;
    godot::PackedStringArray totem_ids_;
};

}  // namespace montezuma::godot_binding
