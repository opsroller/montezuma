// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Temple water for desktop (Forward+), drawn opaque. The channel is a known
// box (floor, inner and outer walls, the causeway's sides; the theme
// generator writes its size), so the shader follows the refracted view ray
// to the stone it reaches, samples that stone's colour there, and lets the
// water's own colour take over with the true path length (Beer-Lambert
// absorption). The stone gets slab joints, the walls' shadow and darker
// corners; caustic lines brighten the shallow floor. The engine lights
// the result and adds the reflections (sky, reflection probe and, where the
// tier has it, screen-space reflection) through a smooth dielectric
// specular. No screen or depth texture: on the benchmark Mac their copies
// cost several times the water's budget, and an opaque surface hides the
// floor under it instead of shading it twice. Mobile and web use
// SH_WaterSimple. Cost per pixel: 1 texture sample, 3 noise octaves, and
// 2 caustic layers where they can show.
shader_type spatial;
render_mode depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

#include "res://assets/materials/shaders/SH_Water.gdshaderinc"

// Colour the water scatters back where it is deep.
uniform vec4 water_color : source_color = vec4(0.114, 0.29, 0.251, 1.0);
// Light lost per metre of water, red, green, blue, 0 to 4 (negative counts
// as 0): red goes first, so deep water turns green-teal.
uniform vec3 absorption = vec3(1.1, 0.34, 0.28);
uniform float wave_scale : hint_range(0.2, 4.0, 0.01) = 1.4;
uniform float wave_strength : hint_range(0.0, 1.0, 0.01) = 0.18;
// Drift of the ripples, metres per second.
uniform float flow_speed : hint_range(0.0, 0.5, 0.001) = 0.05;
uniform float roughness : hint_range(0.0, 0.3, 0.001) = 0.04;
uniform float caustic_strength : hint_range(0.0, 4.0, 0.01) = 2.2;
uniform float caustic_scale : hint_range(0.2, 4.0, 0.01) = 1.3;
// Width of the soft line where the water meets stone, metres of water.
uniform float edge_softness : hint_range(0.0, 0.3, 0.001) = 0.08;
// The channel's stone (MI_StoneDark): its base colour map, tint and
// triplanar scale. Clamped sampler, wrapped by hand (see the Metal note in
// SH_Triplanar.gdshaderinc).
uniform sampler2D texture_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_disable;
uniform vec4 floor_tint : source_color = vec4(0.361, 0.322, 0.275, 1.0);
uniform float floor_scale = 0.9;
// The channel in world metres: a square ring between the half-widths
// channel_inner and channel_outer round the origin, floor at floor_height,
// and the causeway (|x| < causeway_half on the far side, -Z) across it.
uniform float channel_inner = 10.0;
uniform float channel_outer = 12.0;
uniform float floor_height = -2.5;
uniform float causeway_half = 1.5;
// Height of the channel's rim (the ground), and the direction towards the
// sun: the stone under the water is in shadow where the walls stand between.
uniform float channel_top = -2.42;
uniform vec3 sun_direction = vec3(-0.426, 0.669, 0.609);
// How bright stone the sun does not reach is, against sunlit floor.
uniform float shade_level : hint_range(0.0, 1.0, 0.01) = 0.25;
// How much light a corner, where floor meets wall, keeps, and how far from
// the corner it is back to full, metres.
uniform float corner_level : hint_range(0.0, 1.0, 0.01) = 0.35;
uniform float corner_width : hint_range(0.01, 1.0, 0.01) = 0.5;
// Floor slabs and wall blocks, metres (along, across), and how bright their
// joints are against the stone.
uniform vec2 floor_slab = vec2(0.9, 0.6);
uniform vec2 wall_block = vec2(0.6, 0.3);
uniform float joint_level : hint_range(0.0, 1.0, 0.01) = 0.25;

varying vec3 w_pos;

void vertex() {
	w_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

// Distance along d from p to the wall at |coordinate| = edge, with a side
// value s = coordinate and a direction value v = d's component; huge when
// the ray moves away from that wall.
float wall_t(float s, float v, float edge, bool outward) {
	float m = sign(s) * v;
	if (outward ? m <= 0.0 : m >= 0.0) {
		return 1e6;
	}
	return (edge - abs(s)) / m;
}

// How far point p lies inside the channel's square ring (negative outside),
// metres; the causeway's sides count as walls.
float in_channel(vec3 p) {
	float r = max(abs(p.x), abs(p.z));
	float m = min(r - channel_inner, channel_outer - r);
	if (p.z < 0.0 && -p.z >= abs(p.x)) {
		m = min(m, abs(p.x) - causeway_half);
	}
	return m;
}

// The channel's stone has no joints in its map, so draw them: slabs of the
// given size in metres, each row offset by half a slab, with dark joints
// (softened by the pixel footprint fw) and a slight tone per slab.
float slab_tone(vec2 p, vec2 size, float fw) {
	vec2 q = p / size;
	q.x += 0.5 * floor(q.y);
	vec2 f = min(fract(q), 1.0 - fract(q)) * size;
	float joint = smoothstep(0.006, 0.018 + fw, min(f.x, f.y));
	float h = fract(sin(dot(floor(q), vec2(127.1, 311.7))) * 43758.5453);
	return mix(joint_level, 1.0, joint) * (0.86 + 0.28 * h);
}

void fragment() {
	vec2 slope = wt_slope(w_pos.xz, TIME, wave_scale, wave_strength, flow_speed, 3);
	vec3 n_w = wt_normal(slope);
	vec3 n = normalize((VIEW_MATRIX * vec4(n_w, 0.0)).xyz);
	float fresnel = wt_fresnel(clamp(dot(n, VIEW), 0.0, 1.0));

	// The view ray bent into the water (IOR 1.33), then the first stone it
	// meets: the floor, a channel wall or a causeway side.
	vec3 d = normalize(w_pos - CAMERA_POSITION_WORLD);
	vec3 rd = refract(d, n_w, 0.75);
	rd = rd.y < -0.05 ? rd : normalize(vec3(d.x, min(d.y, -0.05), d.z));
	float t = (floor_height - w_pos.y) / rd.y;
	int face = 1;  // 0: wall across X, 1: floor, 2: wall across Z
	float tx = 1e6;
	float tz = 1e6;
	if (abs(w_pos.x) >= channel_inner) {
		// East and west: walls across X, and the outer wall across Z at the corners.
		tx = wall_t(w_pos.x, rd.x, channel_outer, true);
		float ti = wall_t(w_pos.x, rd.x, channel_inner, false);
		// The inner wall stops at the corners, where the channel turns.
		if (abs(w_pos.z + rd.z * ti) < channel_inner) {
			tx = min(tx, ti);
		}
		tz = wall_t(w_pos.z, rd.z, channel_outer, true);
	} else {
		// Near and far: walls across Z, and the causeway's sides across X.
		tz = wall_t(w_pos.z, rd.z, channel_outer, true);
		float ti = wall_t(w_pos.z, rd.z, channel_inner, false);
		if (abs(w_pos.x + rd.x * ti) < channel_inner) {
			tz = min(tz, ti);
		}
		if (w_pos.z < 0.0) {
			tx = wall_t(w_pos.x, rd.x, causeway_half, false);
		}
	}
	if (tx < t) {
		t = tx;
		face = 0;
	}
	if (tz < t) {
		t = tz;
		face = 2;
	}
	t = max(t, 0.0);
	vec3 hit = w_pos + rd * t;

	// The stone there, mapped like SH_TriplanarPBR's projection for that face.
	vec2 uv = face == 1 ? hit.xz : (face == 0 ? vec2(hit.z * sign(rd.x), -hit.y) : vec2(-hit.x * sign(rd.z), -hit.y));
	float fw = length(fwidth(uv));
	float slabs = slab_tone(uv, face == 1 ? floor_slab : wall_block, fw);
	uv *= floor_scale;
	vec3 stone = floor_tint.rgb * slabs * textureGrad(texture_albedo, fract(uv), dFdx(uv), dFdy(uv)).rgb;

	// Caustics: brighter where the floor lies a little under the surface,
	// fading in from the edge and out with depth.
	float below = max(w_pos.y - hit.y, 0.0);
	float fade = smoothstep(0.02, 0.2, below) * exp(-1.2 * below);
	float caustic = 0.0;
	if (fade * caustic_strength > 0.01) {
		caustic = wt_caustics(hit.xz, TIME, caustic_scale, 2);
	}
	stone *= 1.0 + caustic_strength * caustic * fade;

	// Light on that stone: the sun reaches it only where its path clears the
	// walls' tops, and falls on a wall at the wall's own angle (relative to
	// the floor, which the engine lights through the surface); stone in a
	// corner gets less sky.
	vec3 to_sun = normalize(sun_direction);
	vec3 face_n = face == 1 ? vec3(0.0, 1.0, 0.0) : (face == 0 ? vec3(-sign(rd.x), 0.0, 0.0) : vec3(0.0, 0.0, -sign(rd.z)));
	float direct = min(max(dot(face_n, to_sun), 0.0) / max(to_sun.y, 0.1), 1.5);
	vec3 rim = hit + to_sun * (max(channel_top - hit.y, 0.0) / max(to_sun.y, 0.05));
	direct *= smoothstep(-0.04, 0.04, in_channel(rim));
	float corner = face == 1 ? in_channel(hit) : hit.y - floor_height;
	stone *= mix(shade_level, 1.0, direct) * mix(corner_level, 1.0, smoothstep(0.0, corner_width, corner));

	vec3 trans = wt_transmittance(absorption, t);
	float edge = 1.0 - smoothstep(0.0, max(edge_softness, 0.001), t);
	vec3 water = stone * trans + water_color.rgb * (1.0 - dot(trans, vec3(0.3333))) + vec3(0.35) * edge * caustic;

	// What the surface reflects is the engine's specular, so the light that
	// goes in is what it does not reflect.
	ALBEDO = min(water * (1.0 - fresnel), vec3(1.0));
	// Widen the highlight where the ripples change faster than a pixel, so the
	// sun's glints do not alias into sparkles.
	vec3 dn = fwidth(n_w);
	float a2 = pow(roughness, 4.0) + min(2.0 * dot(dn, dn), 0.18);
	ROUGHNESS = pow(a2, 0.25);
	METALLIC = 0.0;
	// F0 0.02 is Godot's specular 0.354.
	SPECULAR = 0.354;
	NORMAL = n;
}
