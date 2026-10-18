// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Ember without a trail: a small glowing dot facing the camera, coloured by
// the particle's colour ramp (COLOR: from yellow-white to dull red as it
// cools, alpha to 0 as it dies). Additive and unshaded, in HDR for glow.
// The web tier draws these; others draw SH_EmberTrail. Parameters:
// docs/reference/fire.md.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, skip_vertex_transform;

uniform float intensity : hint_range(0.0, 40.0) = 8.0;

void vertex() {
	vec3 centre = (MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	float scale = length(MODEL_MATRIX[0].xyz);
	// View space: x right, y up on screen.
	VERTEX = centre + vec3(VERTEX.xy * scale, 0.0);
	NORMAL = vec3(0.0, 0.0, 1.0);
}

void fragment() {
	vec2 c = UV * 2.0 - 1.0;
	float dot_shape = exp(-4.0 * dot(c, c));
	ALBEDO = COLOR.rgb * COLOR.a * dot_shape * intensity;
}
