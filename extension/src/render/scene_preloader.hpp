// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace montezuma::godot_binding {

// Loads a scene and the theme it shows on background threads, so Godot's
// pipeline precompilation runs off the main thread
// (docs/explanation/about-pipeline-compilation.md):
// 1. adds the primer scene, which turns on every rendering feature the game
//    uses before any asset loads (Godot only precompiles the features it has
//    already seen);
// 2. requests `scene_path` and every theme asset the manifest lists, with
//    their dependencies, through ResourceLoader.load_threaded_request
//    (sub-threads on);
// 3. when every request has loaded, keeps the resources referenced (so
//    later synchronous loads are cache hits), frees the primer, adds the
//    scene as its child and emits `loaded`.
// A request that fails stops the load with an error naming the path.
class ScenePreloader : public godot::Node {
    GDCLASS(ScenePreloader, godot::Node)

public:
    void _ready() override;
    void _process(double delta) override;

    void set_scene_path(const godot::String& v) { scene_path_ = v; }
    godot::String get_scene_path() const { return scene_path_; }
    void set_primer_path(const godot::String& v) { primer_path_ = v; }
    godot::String get_primer_path() const { return primer_path_; }

    bool is_loaded() const { return loaded_; }
    // Seconds from _ready to `loaded`.
    double get_load_seconds() const { return load_seconds_; }

protected:
    static void _bind_methods();

private:
    void request(const godot::String& path);
    void request_theme();
    void finish();

    godot::String scene_path_;
    godot::String primer_path_;
    godot::PackedStringArray pending_;
    godot::Array held_;
    godot::Node* primer_ = nullptr;
    bool loaded_ = false;
    bool failed_ = false;
    uint64_t start_usec_ = 0;
    double load_seconds_ = 0.0;
};

}  // namespace montezuma::godot_binding
