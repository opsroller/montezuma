// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Brazier flame: additive, unshaded, flickering. Hot core facing the
// camera, orange edges, fading toward the tip. Vertex wobble grows with
// height so the base stays seated in the bowl. Works on every renderer.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 core_color : source_color = vec4(1.0, 0.94, 0.69, 1.0);
uniform vec4 edge_color : source_color = vec4(1.0, 0.42, 0.07, 1.0);
uniform float intensity : hint_range(0.0, 8.0) = 3.0;
// Height of the flame base and its length, in the mesh's local units.
uniform float base_height = 0.44;
uniform float flame_length = 0.65;

varying float height01;

void vertex() {
	height01 = clamp((VERTEX.y - base_height) / flame_length, 0.0, 1.0);
	float phase = VERTEX.x * 3.1 + VERTEX.z * 2.3;
	VERTEX.x += sin(TIME * 7.0 + phase) * 0.035 * height01;
	VERTEX.z += cos(TIME * 6.1 + phase * 1.3) * 0.035 * height01;
	VERTEX.y += sin(TIME * 9.0 + phase) * 0.03 * height01;
}

void fragment() {
	float facing = abs(dot(NORMAL, VIEW));
	vec3 color = mix(edge_color.rgb, core_color.rgb, pow(facing, 2.0) * (1.0 - height01 * 0.7));
	float flicker = 0.85 + 0.15 * sin(TIME * 13.0 + height01 * 4.0);
	float fade = (1.0 - height01 * height01) * (0.35 + 0.65 * facing);
	ALBEDO = color * intensity * flicker * fade;
}
