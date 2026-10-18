// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Heat haze above a brazier: a camera-facing quad that redraws the screen
// behind it shifted by rising, swirling noise, strongest just above the
// flame and fading out at its edges. Drawn before the flame (material
// render_priority -1), so the flame stays sharp. Desktop tiers only
// (quality:heat_haze); it reads the screen texture. Parameters:
// docs/reference/fire.md.
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, skip_vertex_transform;

uniform sampler2D screen_texture : hint_screen_texture, filter_linear_mipmap;
uniform sampler2D noise : filter_linear_mipmap, repeat_enable;
// Largest screen shift, as a fraction of the screen.
uniform float strength : hint_range(0.0, 0.05) = 0.006;
// How fast the shimmer rises, in noise tiles per second.
uniform float speed : hint_range(0.0, 4.0) = 0.8;

void vertex() {
	vec3 centre = (MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vec3 to_eye = normalize(-centre);
	vec3 world_up = normalize((VIEW_MATRIX * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
	vec3 up = world_up - to_eye * dot(world_up, to_eye);
	up = length(up) > 0.001 ? normalize(up) : vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, to_eye));
	vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), 1.0);
	VERTEX = centre + right * VERTEX.x * scale.x + up * VERTEX.y * scale.y;
	NORMAL = to_eye;
}

void fragment() {
	vec2 n1 = texture(noise, UV * vec2(1.0, 1.5) + vec2(0.0, TIME * speed)).rg;
	vec2 n2 = texture(noise, UV * vec2(2.1, 2.7) + vec2(0.37, TIME * speed * 1.7)).rg;
	vec2 shift = (n1 + n2 - 1.0);
	// Strongest low in the quad (just above the flame), gone at the edges.
	float fade_x = smoothstep(0.0, 0.3, UV.x) * smoothstep(1.0, 0.7, UV.x);
	float fade_y = smoothstep(1.0, 0.75, UV.y) * smoothstep(0.0, 0.6, UV.y);
	float mask = fade_x * fade_y;
	ALBEDO = textureLod(screen_texture, SCREEN_UV + shift * strength * mask, 0.0).rgb;
	ALPHA = mask;
}
