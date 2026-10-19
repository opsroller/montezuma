// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Light-shaft card: a soft additive beam along the sun's direction, the
// stand-in for volumetric fog shafts on tiers without it. The quad runs
// from the ground (UV.y = 1) toward the sun (UV.y = 0); it fades across its
// width, toward its sky end and where it meets the ground, and its streaks
// drift slowly. Unshaded. No depth-texture soft edge: ANGLE's Metal backend
// aborted drawing it on a virtual Mac GPU, so the ground fade is in UV. Parameters: docs/reference/board-themes.md#atmosphere.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled;

uniform vec4 shaft_color : source_color = vec4(1.0, 0.86, 0.62, 1.0);
uniform float intensity : hint_range(0.0, 4.0) = 0.25;

varying float phase;

void vertex() {
	vec2 origin = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xz;
	phase = fract(sin(dot(origin, vec2(12.9898, 78.233))) * 43758.5453) * 6.2832;
}

void fragment() {
	float across = sin(3.14159 * UV.x);
	across *= across;
	// Thin bright streaks inside the beam, drifting sideways.
	float streaks = 0.75 + 0.25 * sin(UV.x * 17.0 + phase + TIME * 0.15) * sin(UV.x * 7.0 - phase * 0.5);
	float along = smoothstep(0.0, 0.7, UV.y) * smoothstep(1.0, 0.85, UV.y);
	ALBEDO = shaft_color.rgb * across * streaks * along * intensity;
}
