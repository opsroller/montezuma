// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Dust mote drifting in the air beside the temple: a tiny camera-facing
// dot, lit, so it shows only where the sun (or a brazier) reaches it and
// stays dark in shadow, which makes the sun shafts read. Additive; each mote
// twinkles slowly as it turns. Desktop tiers only, with the volumetric fog.
// Parameters: docs/reference/board-themes.md#atmosphere.
shader_type spatial;
render_mode blend_add, depth_draw_never, cull_disabled, diffuse_lambert_wrap, specular_disabled, skip_vertex_transform;

uniform vec4 dust_color : source_color = vec4(1.0, 0.9, 0.75, 1.0);
uniform float intensity : hint_range(0.0, 8.0) = 1.0;
// How much each mote's brightness varies over time (0 steady, 1 blinks out).
uniform float twinkle : hint_range(0.0, 1.0) = 0.6;

varying float glint;

void vertex() {
	vec3 centre = (MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	float scale = length(MODEL_MATRIX[0].xyz);
	VERTEX = centre + vec3(VERTEX.xy * scale, 0.0);
	NORMAL = vec3(0.0, 0.0, 1.0);
	float phase = fract(sin(float(INSTANCE_ID) * 78.233) * 43758.5453) * 6.2832;
	glint = 1.0 - twinkle * (0.5 + 0.5 * sin(TIME * 0.9 + phase));
}

void fragment() {
	vec2 c = UV * 2.0 - 1.0;
	float dot_shape = exp(-5.0 * dot(c, c));
	ALBEDO = dust_color.rgb * COLOR.rgb * COLOR.a * dot_shape * glint * intensity;
	ROUGHNESS = 1.0;
}
