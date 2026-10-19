// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include <gdextension_interface.h>

#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "audio/register_audio.hpp"
#include "bench/register_bench.hpp"
#include "board/register_board.hpp"
#include "demo/register_demo.hpp"
#include "game/register_game.hpp"
#include "match3_board.hpp"
#include "match3_level.hpp"
#include "render/lightmap_baker.hpp"
#include "render/register_render.hpp"
#include "ui/register_ui.hpp"

using namespace godot;

namespace {

void initialize_montezuma(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        // Editor-only tooling: the lightmap bake step (tools/assets/bake_lightmaps.sh).
        GDREGISTER_CLASS(montezuma::godot_binding::LightmapBaker);
        EditorPlugins::add_by_type<montezuma::godot_binding::LightmapBaker>();
        return;
    }
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(montezuma::godot_binding::Match3Board);
    GDREGISTER_CLASS(montezuma::godot_binding::Match3Level);
    montezuma::godot_binding::register_board();
    montezuma::godot_binding::register_audio();
    montezuma::godot_binding::register_ui();
    montezuma::godot_binding::register_game();
    montezuma::godot_binding::register_render();
    montezuma::godot_binding::register_bench();
    montezuma::godot_binding::register_demo();
}

void uninitialize_montezuma(ModuleInitializationLevel) {}

}  // namespace

extern "C" {

GDExtensionBool GDE_EXPORT montezuma_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                                  GDExtensionClassLibraryPtr library,
                                                  GDExtensionInitialization* initialization) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_montezuma);
    init.register_terminator(uninitialize_montezuma);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}

}  // extern "C"
