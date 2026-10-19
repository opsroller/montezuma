// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Temple water for desktop (Forward+): the channel floor seen through the
// surface, bent by the ripples (refraction) and fading to the water's own
// colour with depth (absorption over the true path length, from the depth
// texture), caustic lines on that floor, and the sky reflected through the
// engine's own specular. No screen-space reflection march: on the
// benchmark Mac it cost several times the water's whole budget. Mobile and
// web use SH_WaterSimple (no refraction). Cost per pixel: 2 depth and 1
// screen sample; 3 noise octaves, and 2 caustic layers where they can
// show; no material textures.
shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_back, diffuse_burley, specular_schlick_ggx;

#include "res://assets/materials/shaders/SH_Water.gdshaderinc"

uniform sampler2D screen_texture : hint_screen_texture, repeat_disable, filter_linear;
uniform sampler2D depth_texture : hint_depth_texture, repeat_disable, filter_nearest;

// Colour the water scatters back where it is deep, lit by the scene.
uniform vec4 water_color : source_color = vec4(0.114, 0.29, 0.251, 1.0);
// Light lost per metre of water, red, green, blue, 0 to 4 (negative counts
// as 0): red goes first, so deep water turns green-teal.
uniform vec3 absorption = vec3(1.1, 0.34, 0.28);
uniform float wave_scale : hint_range(0.2, 4.0, 0.01) = 1.4;
uniform float wave_strength : hint_range(0.0, 1.0, 0.01) = 0.18;
// Drift of the ripples, metres per second.
uniform float flow_speed : hint_range(0.0, 0.5, 0.001) = 0.05;
uniform float roughness : hint_range(0.0, 0.3, 0.001) = 0.04;
// Screen-space bend of the floor seen through the surface, per metre of depth.
uniform float refraction_strength : hint_range(0.0, 0.15, 0.001) = 0.04;
uniform float caustic_strength : hint_range(0.0, 4.0, 0.01) = 2.2;
uniform float caustic_scale : hint_range(0.2, 4.0, 0.01) = 1.3;
// Width of the soft line where the water meets stone, metres of water.
uniform float edge_softness : hint_range(0.0, 0.3, 0.001) = 0.08;

varying vec3 w_pos;

void vertex() {
	w_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

vec3 view_pos(vec2 uv, float depth, mat4 inv_proj) {
	vec4 v = inv_proj * vec4(uv * 2.0 - 1.0, depth, 1.0);
	return v.xyz / v.w;
}

void fragment() {
	vec2 slope = wt_slope(w_pos.xz, TIME, wave_scale, wave_strength, flow_speed, 3);
	vec3 n_w = wt_normal(slope);
	vec3 n = normalize((VIEW_MATRIX * vec4(n_w, 0.0)).xyz);
	float ndv = clamp(dot(n, VIEW), 0.0, 1.0);
	float fresnel = wt_fresnel(ndv);

	// Floor behind the surface. Reversed Z: a smaller depth is farther.
	float d0 = textureLod(depth_texture, SCREEN_UV, 0.0).r;
	vec3 floor0 = view_pos(SCREEN_UV, d0, INV_PROJECTION_MATRIX);
	float len0 = length(floor0 - VERTEX);
	// Bend by the ripples only, not by the camera's tilt over a flat surface.
	vec3 bend = n - normalize((VIEW_MATRIX * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
	vec2 uv = SCREEN_UV + vec2(bend.x, -bend.y) * refraction_strength * clamp(len0, 0.0, 1.5);
	uv = clamp(uv, vec2(0.0), vec2(1.0));
	float d = textureLod(depth_texture, uv, 0.0).r;
	if (d >= FRAGCOORD.z) {
		// The bent ray found something in front of the water: use the straight one.
		uv = SCREEN_UV;
		d = d0;
	}
	vec3 floor_v = view_pos(uv, d, INV_PROJECTION_MATRIX);
	float len = length(floor_v - VERTEX);
	vec3 floor_w = (INV_VIEW_MATRIX * vec4(floor_v, 1.0)).xyz;
	vec3 seen = textureLod(screen_texture, uv, 0.0).rgb;

	// Caustics: brighter where the floor lies a little under the surface,
	// fading in from the edge and out with depth.
	float below = max(w_pos.y - floor_w.y, 0.0);
	float fade = smoothstep(0.02, 0.2, below) * exp(-1.2 * below);
	float caustic = 0.0;
	if (fade * caustic_strength > 0.01) {
		caustic = wt_caustics(floor_w.xz, TIME, caustic_scale, 2);
	}
	seen *= 1.0 + caustic_strength * caustic * fade;

	vec3 trans = wt_transmittance(absorption, len);
	vec3 transmitted = seen * trans * (1.0 - fresnel);
	float edge = 1.0 - smoothstep(0.0, max(edge_softness, 0.001), len);

	ALBEDO = water_color.rgb * (1.0 - dot(trans, vec3(0.3333))) + vec3(0.35) * edge * caustic;
	ROUGHNESS = roughness;
	METALLIC = 0.0;
	// F0 0.02 is Godot's specular 0.354.
	SPECULAR = 0.354;
	NORMAL = n;
	EMISSION = transmitted * EXPOSURE;
}
