// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Light-shaft card: a soft additive beam along the sun's direction, the
// stand-in for volumetric fog shafts on tiers without it. The quad runs
// from the ground (UV.y = 1) toward the sun (UV.y = 0); it fades across its
// width, toward its sky end and where it meets the ground, and its streaks
// drift slowly. Unshaded. Parameters: docs/reference/board-themes.md#atmosphere.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled;

uniform vec4 shaft_color : source_color = vec4(1.0, 0.86, 0.62, 1.0);
uniform float intensity : hint_range(0.0, 4.0) = 0.25;
// Metres over which the beam fades in above the surface it lands on.
uniform float soft_distance : hint_range(0.0, 4.0) = 0.6;
uniform sampler2D depth_texture : hint_depth_texture, filter_nearest;

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
	float along = smoothstep(0.0, 0.7, UV.y) * smoothstep(1.0, 0.92, UV.y);
	float soft = 1.0;
	if (soft_distance > 0.0) {
		float depth = texture(depth_texture, SCREEN_UV).r;
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
		depth = depth * 2.0 - 1.0;
#endif
		vec4 scene = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, depth, 1.0);
		scene.xyz /= scene.w;
		soft = clamp((VERTEX.z - scene.z) / soft_distance, 0.0, 1.0);
	}
	ALBEDO = shaft_color.rgb * across * streaks * along * soft * intensity;
}
