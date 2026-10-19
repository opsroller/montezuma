// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Dusty air around the temple for the volumetric fog: low, drifting noise
// that the sun lights and the temple, pillars and ferns shadow into shafts.
// Density is exactly zero inside the square of half-side clear_half around
// the board centre (the board plus a clearance) and fades in over `fade`
// metres outside it, so no fog ever lies over the board (readability rule
// 2). It also thins out toward `height_top`. Desktop tiers only. Parameters:
// docs/reference/board-themes.md#atmosphere.
shader_type fog;

uniform float density : hint_range(0.0, 1.0, 0.0001) = 0.07;
uniform vec4 albedo : source_color = vec4(0.93, 0.86, 0.75, 1.0);
// Half the side of the fog-free square around the board centre, in metres.
uniform float clear_half = 6.15;
uniform float fade = 1.2;
uniform float height_top = 1.2;
uniform float top_fade = 1.6;
// Seamless noise; one tile spans noise_scale metres and drifts with the wind.
uniform sampler3D density_texture : filter_linear, repeat_enable, hint_default_white;
uniform float noise_scale = 9.0;
uniform vec3 wind = vec3(0.12, 0.0, -0.05);
// Unit vector toward the sun. The noise is stretched `shaft_stretch` times
// along it, so denser and thinner air lines up into shafts.
uniform vec3 sun_direction = vec3(-0.426, 0.669, 0.609);
uniform float shaft_stretch : hint_range(1.0, 16.0) = 6.0;
// Noise values mapped to no air and full density: a narrower range gives
// clearer gaps between the shafts.
uniform vec2 contrast = vec2(0.3, 0.75);

void fog() {
	vec2 d = abs(WORLD_POSITION.xz) - vec2(clear_half);
	float outside = clamp(max(d.x, d.y) / max(fade, 0.001), 0.0, 1.0);
	float top = clamp((height_top - WORLD_POSITION.y) / max(top_fade, 0.001), 0.0, 1.0);
	vec3 q = WORLD_POSITION - wind * TIME;
	float along = dot(q, sun_direction);
	q += sun_direction * along * (1.0 / shaft_stretch - 1.0);
	float noise = smoothstep(contrast.x, contrast.y, texture(density_texture, q / noise_scale).r);
	DENSITY = density * outside * top * noise;
	ALBEDO = albedo.rgb;
}
