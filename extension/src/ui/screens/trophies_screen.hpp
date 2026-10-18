// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/variant/packed_string_array.hpp>

#include "../screen.hpp"

namespace montezuma::godot_binding {

// One card per trophy id in %Grid, with name, description and state from
// the string table (trophy.<id>.name / .desc). Every trophy shows locked
// until the trophies package (WP-08) supplies earned ids.
class TrophiesScreen : public Screen {
    GDCLASS(TrophiesScreen, Screen)

public:
    void on_open(const godot::Dictionary& params) override;

    void set_trophy_ids(const godot::PackedStringArray& v) { trophy_ids_ = v; }
    godot::PackedStringArray get_trophy_ids() const { return trophy_ids_; }

protected:
    static void _bind_methods();

private:
    void rebuild();

    godot::PackedStringArray trophy_ids_;
};

}  // namespace montezuma::godot_binding
