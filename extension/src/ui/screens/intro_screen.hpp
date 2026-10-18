// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../screen.hpp"

namespace montezuma::godot_binding {

// Story pages before the first level: one string key per page in %Body,
// with %Next (connected to next_page in the scene) and a skip NavButton.
// After the last page it performs finish_action / finish_target.
class IntroScreen : public Screen {
    GDCLASS(IntroScreen, Screen)

public:
    void on_open(const godot::Dictionary& params) override;
    void next_page();

    void set_pages(const godot::PackedStringArray& v) { pages_ = v; }
    godot::PackedStringArray get_pages() const { return pages_; }
    void set_finish_action(const godot::String& v) { finish_action_ = v; }
    godot::String get_finish_action() const { return finish_action_; }
    void set_finish_target(const godot::String& v) { finish_target_ = v; }
    godot::String get_finish_target() const { return finish_target_; }

protected:
    static void _bind_methods();

private:
    void show_page();

    godot::PackedStringArray pages_;
    godot::String finish_action_ = "replace";
    godot::String finish_target_ = "episode_map";
    int page_ = 0;
};

}  // namespace montezuma::godot_binding
