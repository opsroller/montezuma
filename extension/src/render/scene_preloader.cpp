// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "scene_preloader.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "board/piece_library.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// get_dependencies() lists "uid::type::path" (or a bare path); the path is last.
String dependency_path(const String& entry) {
    const PackedStringArray parts = entry.split("::");
    return parts.is_empty() ? entry : parts[parts.size() - 1];
}

// Every resource file `path` pulls in, itself included, without loading any.
void collect_dependencies(const String& path, PackedStringArray& out) {
    if (out.has(path)) return;
    out.push_back(path);
    const PackedStringArray deps = ResourceLoader::get_singleton()->get_dependencies(path);
    for (const String& dep : deps) collect_dependencies(dependency_path(dep), out);
}

}  // namespace

void ScenePreloader::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_scene_path", "path"), &ScenePreloader::set_scene_path);
    ClassDB::bind_method(D_METHOD("get_scene_path"), &ScenePreloader::get_scene_path);
    ClassDB::bind_method(D_METHOD("set_primer_path", "path"), &ScenePreloader::set_primer_path);
    ClassDB::bind_method(D_METHOD("get_primer_path"), &ScenePreloader::get_primer_path);
    ClassDB::bind_method(D_METHOD("is_loaded"), &ScenePreloader::is_loaded);
    ClassDB::bind_method(D_METHOD("get_load_seconds"), &ScenePreloader::get_load_seconds);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene_path", PROPERTY_HINT_FILE, "*.tscn"), "set_scene_path",
                 "get_scene_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "primer_path", PROPERTY_HINT_FILE, "*.tscn"), "set_primer_path",
                 "get_primer_path");
    ADD_SIGNAL(MethodInfo("loaded", PropertyInfo(Variant::OBJECT, "scene")));
}

void ScenePreloader::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    start_usec_ = Time::get_singleton()->get_ticks_usec();
    if (!primer_path_.is_empty()) {
        const Ref<PackedScene> primer = ResourceLoader::get_singleton()->load(primer_path_);
        ERR_FAIL_COND_MSG(primer.is_null(), "ScenePreloader: primer scene does not load: " + primer_path_);
        primer_ = primer->instantiate();
        add_child(primer_);
    }
    ERR_FAIL_COND_MSG(scene_path_.is_empty(), "ScenePreloader: scene_path is not set.");
    request(scene_path_);
    request_theme();
    set_process(true);
}

void ScenePreloader::request(const String& path) {
    if (pending_.has(path)) return;
    const Error err = ResourceLoader::get_singleton()->load_threaded_request(path, "", true);
    if (err != OK) {
        ERR_PRINT(vformat("ScenePreloader: cannot request %s (error %d).", path, err));
        failed_ = true;
        return;
    }
    pending_.push_back(path);
}

// The theme's assets: the loads the board would otherwise make on the main
// thread as it builds.
void ScenePreloader::request_theme() {
    const String manifest_path = PieceLibrary::kManifestPath;
    ERR_FAIL_COND_MSG(!FileAccess::file_exists(manifest_path), "ScenePreloader: no asset manifest at " + manifest_path);
    const Dictionary manifest = JSON::parse_string(FileAccess::get_file_as_string(manifest_path));
    const Dictionary themes = manifest.get("themes", Dictionary());
    const Dictionary entries = themes.get(manifest.get("default_theme", ""), Dictionary());
    ERR_FAIL_COND_MSG(entries.is_empty(), "ScenePreloader: the asset manifest has no default theme.");
    PackedStringArray files;
    const Array paths = entries.values();
    for (int64_t i = 0; i < paths.size(); ++i) collect_dependencies(paths[i], files);
    for (const String& file : files) request(file);
}

void ScenePreloader::_process(double) {
    if (failed_) {
        set_process(false);
        ERR_PRINT("ScenePreloader: loading " + scene_path_ + " failed; see the errors above.");
        return;
    }
    ResourceLoader* loader = ResourceLoader::get_singleton();
    for (const String& path : pending_) {
        const ResourceLoader::ThreadLoadStatus status = loader->load_threaded_get_status(path);
        if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) return;
        if (status != ResourceLoader::THREAD_LOAD_LOADED) {
            set_process(false);
            ERR_PRINT(vformat("ScenePreloader: %s did not load (status %d).", path, status));
            return;
        }
    }
    finish();
}

void ScenePreloader::finish() {
    set_process(false);
    ResourceLoader* loader = ResourceLoader::get_singleton();
    // load_threaded_get hands each result over once; scene_path_ was requested first.
    for (const String& path : pending_) held_.push_back(loader->load_threaded_get(path));
    const Ref<PackedScene> scene = held_[0];
    ERR_FAIL_COND_MSG(scene.is_null(), "ScenePreloader: " + scene_path_ + " is not a scene.");
    if (primer_) {
        primer_->queue_free();
        primer_ = nullptr;
    }
    Node* node = scene->instantiate();
    add_child(node);
    loaded_ = true;
    load_seconds_ = static_cast<double>(Time::get_singleton()->get_ticks_usec() - start_usec_) / 1e6;
    UtilityFunctions::print("ScenePreloader: ", scene_path_, " and ", pending_.size() - 1, " theme resources loaded in ",
                            String::num(load_seconds_, 2), " s.");
    emit_signal("loaded", node);
}

}  // namespace montezuma::godot_binding
