// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Temple water for mobile and web (Mobile and Compatibility renderers):
// blended over the real channel floor, no refraction and no screen or
// depth texture. The path through the water comes from the known floor
// height (floor_height, set by the theme generator), so depth colour and
// caustics need no depth buffer; reflections are the reflection probes and
// sky through the engine's own specular. Desktop uses SH_Water. Cost per
// pixel: 2 noise octaves, 1 caustic layer, no texture samples.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

#include "res://assets/materials/shaders/SH_Water.gdshaderinc"

uniform vec4 water_color : source_color = vec4(0.114, 0.29, 0.251, 1.0);
uniform vec3 absorption = vec3(1.6, 0.55, 0.45);
uniform float wave_scale : hint_range(0.2, 4.0, 0.01) = 1.0;
uniform float wave_strength : hint_range(0.0, 1.0, 0.01) = 0.35;
uniform float flow_speed : hint_range(0.0, 0.5, 0.001) = 0.05;
uniform float roughness : hint_range(0.0, 0.3, 0.001) = 0.04;
uniform float caustic_strength : hint_range(0.0, 4.0, 0.01) = 1.4;
uniform float caustic_scale : hint_range(0.2, 4.0, 0.01) = 1.3;
// World height of the channel floor under this water, metres.
uniform float floor_height = -2.5;
// Albedo the caustic light lands on, for the light it sends back.
uniform float floor_albedo : hint_range(0.0, 1.0, 0.01) = 0.3;

varying vec3 w_pos;

void vertex() {
	w_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 slope = wt_slope(w_pos.xz, TIME, wave_scale, wave_strength, flow_speed, 2);
	vec3 n = normalize((VIEW_MATRIX * vec4(wt_normal(slope), 0.0)).xyz);
	float ndv = clamp(dot(n, VIEW), 0.0, 1.0);

	// Path to the flat floor along the view ray, and where it lands.
	vec3 dir = normalize(w_pos - CAMERA_POSITION_WORLD);
	float depth = max(w_pos.y - floor_height, 0.0);
	float len = depth / max(-dir.y, 0.1);
	vec3 floor_w = w_pos + dir * len;
	vec3 trans = wt_transmittance(absorption, len);
	float opacity = clamp(1.0 - dot(trans, vec3(0.3333)), 0.0, 1.0);
	opacity = max(opacity, wt_fresnel(ndv));

	float caustic = wt_caustics(floor_w.xz, TIME, caustic_scale, 1) * smoothstep(0.02, 0.2, depth) * exp(-1.2 * depth);
	// Caustic light the floor sends back through the water: a share of the
	// floor's sunlit brightness, so the lines stay as subtle as on desktop,
	// where they scale the refracted floor colour.
	vec3 caustic_light = vec3(caustic_strength * caustic * floor_albedo * 0.35) * trans;

	ALBEDO = water_color.rgb;
	ALPHA = opacity;
	ROUGHNESS = roughness;
	METALLIC = 0.0;
	SPECULAR = 0.354;
	NORMAL = n;
	EMISSION = caustic_light;
}
