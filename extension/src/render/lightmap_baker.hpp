// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {
class Button;
class Node;
}  // namespace godot

namespace montezuma::godot_binding {

// Editor-only driver for the lightmap bake step (tools/assets/bake_lightmaps.sh,
// docs/reference/board-themes.md#lighting). Godot bakes LightmapGI only from
// the editor's "Bake Lightmaps" button, so this plugin presses it: started as
//
//   godot -e --path game -- --bake-lightmaps=res://a.tscn[,res://b.tscn...]
//
// it waits for the editor to finish importing, opens each scene, selects the
// LightmapGI named "Lightmap" in it, bakes it (the editor writes the
// .lmbake and its textures next to the scene that owns the node), prints
// "LIGHTMAP_BAKE ok|fail <scene> <ms>" and quits with status 1 if any failed.
// A failed bake is retried once after the editor is idle again: an import
// the editor starts on its own while the bake runs makes the bake's import
// of its atlas fail ("reimport_files() recursively").
// Without the argument it does nothing.
class LightmapBaker : public godot::EditorPlugin {
    GDCLASS(LightmapBaker, godot::EditorPlugin)

public:
    void _ready() override;
    void _process(double delta) override;

protected:
    static void _bind_methods() {}

private:
    enum class Step { Idle, WaitImport, Open, Select, Bake, Baking, Done };

    void finish(bool ok, const godot::String& why);
    godot::Button* find_bake_button(godot::Node* node) const;

    godot::PackedStringArray scenes_;
    int index_ = 0;
    int failures_ = 0;
    bool retried_ = false;
    Step step_ = Step::Idle;
    int wait_frames_ = 0;
    double elapsed_ = 0.0;
    double step_elapsed_ = 0.0;
};

}  // namespace montezuma::godot_binding
