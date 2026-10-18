// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/variant/string.hpp>

#include "../screen.hpp"

namespace montezuma::godot_binding {

// Episode picker and level grid. Levels come from the LevelData files in
// res://data/levels/e<N>/ (placeholder_levels per episode until they exist).
// Each level is a NavButton with action "play". Scene nodes: %EpisodeTitle,
// %Prev, %Next, %Levels (a container, flowing so large text wraps).
//
// Unlocks are a stub until the progress package (WP-08) lands: only
// e1-l01 is open, or every level with the --unlock-all user argument.
class EpisodeMapScreen : public Screen {
    GDCLASS(EpisodeMapScreen, Screen)

public:
    void on_open(const godot::Dictionary& params) override;
    void _notification(int what);
    // First open level, so a controller lands on something playable.
    void focus_default() override;

    void set_episodes(int v) { episodes_ = v; }
    int get_episodes() const { return episodes_; }
    void set_placeholder_levels(int v) { placeholder_levels_ = v; }
    int get_placeholder_levels() const { return placeholder_levels_; }

    // Number of l*.json files for an episode, or placeholder_levels when none.
    int level_count(int episode) const;

protected:
    static void _bind_methods();

private:
    void show_episode(int episode);
    void step(int delta);
    bool is_unlocked(const godot::String& level_id) const;

    int episodes_ = 5;
    int placeholder_levels_ = 10;
    int episode_ = 1;
};

}  // namespace montezuma::godot_binding
