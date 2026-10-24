// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

namespace godot {
class Node;
}

namespace montezuma::godot_binding {

// Hooks a theme's global illumination up to the board it lights
// (docs/reference/board-themes.md#lighting):
// - each LightmapGI under `lighting` loads the bake its
//   "light_data_path" metadata names, when that build output exists
//   (tools/assets/bake_lightmaps.cmake), and re-resolves its meshes, which
//   it finds by path from itself, so call again after the board is rebuilt;
// A missing bake leaves the theme lit by its real-time lights only.
void attach_baked_lighting(godot::Node* lighting, godot::Node* scene);

}  // namespace montezuma::godot_binding
