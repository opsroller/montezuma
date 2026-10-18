// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Smoke above a brazier: the smoke channel of the fire flipbook
// (SH_Fire's sheet, B) on slow, growing particles. Lit, so the fire light
// below warms its underside and the sun greys it; light from behind shows
// through it. Alpha-blended. The particle's colour ramp fades it in and
// out (COLOR.a). Parameters: docs/reference/fire.md.
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled, shadows_disabled, diffuse_lambert_wrap, specular_disabled, skip_vertex_transform;

uniform sampler2D flame_sheet : filter_linear_mipmap, repeat_disable;
uniform int sheet_grid : hint_range(1, 16) = 4;
uniform vec4 smoke_color : source_color = vec4(0.23, 0.21, 0.2, 1.0);
uniform float opacity : hint_range(0.0, 1.0) = 0.5;
// Light passing through the smoke from behind.
uniform float translucency : hint_range(0.0, 1.0) = 0.5;
uniform float soft_distance : hint_range(0.0, 2.0) = 0.3;
uniform sampler2D depth_texture : hint_depth_texture, filter_nearest;

// Length of the flipbook loop in seconds (64 frames at 30 fps = 16 at 7.5).
uniform float loop_seconds = 2.1333;

varying float frame;

void vertex() {
	// Camera-facing quad, up along world up as seen from the camera.
	vec3 centre = (MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vec3 to_eye = normalize(-centre);
	vec3 world_up = normalize((VIEW_MATRIX * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
	vec3 up = world_up - to_eye * dot(world_up, to_eye);
	up = length(up) > 0.001 ? normalize(up) : vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, to_eye));
	float scale = length(MODEL_MATRIX[0].xyz);
	float spin = INSTANCE_CUSTOM.x;
	vec2 v = mat2(vec2(cos(spin), sin(spin)), vec2(-sin(spin), cos(spin))) * VERTEX.xy;
	VERTEX = centre + (right * v.x + up * v.y) * scale;
	NORMAL = to_eye;
	// Each particle starts at its own place in the loop.
	frame = TIME / loop_seconds + fract(sin(float(INSTANCE_ID) * 12.9898) * 43758.5453);
}

void fragment() {
	float g = float(sheet_grid);
	float index = floor(fract(frame) * g * g);
	vec2 uv = (vec2(mod(index, g), floor(index / g)) + clamp(UV, vec2(0.002), vec2(0.998))) / g;
	float density = texture(flame_sheet, uv).b;
	// Round the sprite off so no tile edge ever shows.
	vec2 c = UV * 2.0 - 1.0;
	float edge = clamp(1.0 - dot(c, c), 0.0, 1.0);

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
	ALBEDO = smoke_color.rgb * COLOR.rgb;
	BACKLIGHT = smoke_color.rgb * translucency;
	ROUGHNESS = 1.0;
	ALPHA = clamp(density * edge * opacity * COLOR.a * soft, 0.0, 1.0);
}
