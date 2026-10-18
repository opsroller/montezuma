// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Brazier flame on GPU particles: a flipbook of our own fire simulation
// (tools/assets/fx/gen_fire.py) played with motion-vector blending, so
// 16 or 64 frames move as smoothly as 30 fps footage. Colour comes from
// the simulated temperature through a blackbody curve, brightness from
// the simulated emission, both in HDR for glow. Additive and unshaded:
// a flame emits light and lets the background through.
//
// Each particle is a quad facing the camera whose up axis follows world
// up as the camera sees it, so tongues rise from the bowl from any view;
// seen from above it shortens by `foreshorten`. Every particle plays the
// loop from its own offset; the particle's colour ramp fades it in and
// out (COLOR), so a respawn never pops.
// Parameters: docs/reference/fire.md.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, skip_vertex_transform;

// RGBA8, linear: R emission (gamma encoded), G temperature 0..1, B smoke.
uniform sampler2D flame_sheet : filter_linear_mipmap, repeat_disable;
// RG: motion to the next frame in tile UV, 0.5 + motion / (2 * motion_scale).
uniform sampler2D motion_sheet : filter_linear, repeat_disable;
// Temperatures (kelvin) that the sheet's 0 and 1 stand for.
uniform float temperature_min_k : hint_range(600.0, 2500.0) = 1100.0;
uniform float temperature_max_k : hint_range(1000.0, 4000.0) = 1900.0;
// Hotter gas outshines cooler gas by (T / hottest)^(4 x this).
uniform float temperature_brightness : hint_range(0.0, 4.0) = 1.5;
uniform float intensity : hint_range(0.0, 40.0) = 10.0;
// 0: the quad keeps its height seen from above; 1: it shortens as a real
// vertical sheet would.
uniform float foreshorten : hint_range(0.0, 1.0) = 0.45;
// Fades the flame where it meets opaque geometry, in metres.
uniform float soft_distance : hint_range(0.0, 1.0) = 0.12;
uniform sampler2D depth_texture : hint_depth_texture, filter_nearest;

// Written by gen_fire.py with the sheets: must match tools/assets/fx/fire.json.
group_uniforms generated;
// Tiles per row and column of both sheets (4 or 8).
uniform int sheet_grid : hint_range(1, 16) = 4;
// The sheet's motion_scale in fire.json.
uniform float motion_scale : hint_range(0.001, 0.5, 0.001) = 0.12;
uniform float emission_gamma : hint_range(0.1, 4.0) = 1.0;
// Length of the flipbook loop in seconds (64 frames at 30 fps = 16 at 7.5).
uniform float loop_seconds : hint_range(0.1, 10.0) = 2.1333;
group_uniforms;

varying float frame;

void vertex() {
	// Camera-facing quad, up along world up as seen from the camera.
	vec3 centre = (MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vec3 to_eye = normalize(-centre);
	vec3 world_up = normalize((VIEW_MATRIX * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
	vec3 up = world_up - to_eye * dot(world_up, to_eye);
	float seen = length(up);
	up = seen > 0.001 ? up / seen : vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, to_eye));
	// The particle's scale and its spin (ParticleProcessMaterial angle).
	float scale = length(MODEL_MATRIX[0].xyz);
	float spin = INSTANCE_CUSTOM.x;
	vec2 v = mat2(vec2(cos(spin), sin(spin)), vec2(-sin(spin), cos(spin))) * VERTEX.xy;
	v.y *= mix(1.0, seen, foreshorten);
	VERTEX = centre + (right * v.x + up * v.y) * scale;
	NORMAL = to_eye;
	// Each particle starts at its own place in the loop.
	frame = TIME / loop_seconds + fract(sin(float(INSTANCE_ID) * 12.9898) * 43758.5453);
}

// Colour of a blackbody at t kelvin in linear sRGB, brightest channel 1.
// Planckian locus in CIE 1960 uv (published 1985 rational fit), then XYZ
// to sRGB.
vec3 blackbody(float t) {
	float u = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t)
			/ (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);
	float v = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t)
			/ (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);
	float d = 2.0 * u - 8.0 * v + 4.0;
	float x = 3.0 * u / d;
	float y = 2.0 * v / d;
	vec3 xyz = vec3(x / y, 1.0, (1.0 - x - y) / y);
	vec3 rgb = mat3(vec3(3.2406, -0.9689, 0.0557), vec3(-1.5372, 1.8758, -0.2040),
			vec3(-0.4986, 0.0415, 1.0570)) * xyz;
	rgb = max(rgb, vec3(0.0));
	return rgb / max(max(rgb.r, rgb.g), max(rgb.b, 1e-4));
}

vec2 tile_uv(float index, vec2 uv) {
	float g = float(sheet_grid);
	vec2 cell = vec2(mod(index, g), floor(index / g));
	return (cell + clamp(uv, vec2(0.002), vec2(0.998))) / g;
}

// 1 inside the tile, falling to 0 over its outer 6%: warped lookups that
// leave the tile add nothing instead of smearing its edge.
float inside(vec2 uv) {
	vec2 e = smoothstep(vec2(0.0), vec2(0.06), uv) * smoothstep(vec2(1.0), vec2(0.94), uv);
	return e.x * e.y;
}

void fragment() {
	float frames = float(sheet_grid * sheet_grid);
	float f = fract(frame) * frames;
	float f0 = floor(f);
	float blend = f - f0;
	float f1 = mod(f0 + 1.0, frames);
	// Motion-vector blend: frame f0 pushed forward along its motion, frame
	// f1 pulled back along its own, cross-faded.
	vec2 m0 = (texture(motion_sheet, tile_uv(f0, UV)).rg * 2.0 - 1.0) * motion_scale;
	vec2 m1 = (texture(motion_sheet, tile_uv(f1, UV)).rg * 2.0 - 1.0) * motion_scale;
	vec2 uv_a = UV - m0 * blend;
	vec2 uv_b = UV + m1 * (1.0 - blend);
	vec4 a = texture(flame_sheet, tile_uv(f0, uv_a)) * inside(uv_a);
	vec4 b = texture(flame_sheet, tile_uv(f1, uv_b)) * inside(uv_b);
	// Fade the quad's own edges so no tile border ever shows as a line.
	vec4 s = mix(a, b, blend) * inside(UV);

	float emission = pow(s.r, 1.0 / emission_gamma);
	float kelvin = mix(temperature_min_k, temperature_max_k, s.g);
	// Relative to the hottest the sheet can be, so a min above max dims
	// instead of blowing out.
	float hot = pow(kelvin / max(temperature_min_k, temperature_max_k), temperature_brightness * 4.0);
	vec3 colour = blackbody(kelvin) * emission * hot * intensity;

	// Soft particle: fade where the quad cuts into the bowl or stone.
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
	ALBEDO = colour * COLOR.rgb * COLOR.a * soft;
}
