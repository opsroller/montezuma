// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Transparent faceted gem for desktop: the scene behind is refracted through
// the facets with a little dispersion (red, green and blue bend by different
// amounts), absorbed by the body colour over a path that lengthens toward
// the silhouette, and topped by the engine's Fresnel reflection. Mobile and
// web use the base StandardMaterial3D of the same name (opaque, reflection
// probe only). Cost: 1 to 3 projections x 3 samples, 4 screen and 3 depth samples;
// draws in the transparent pass because it reads the screen texture.
// Brilliance fakes the light a real cut returns by internal reflection.
shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_back, diffuse_burley, specular_schlick_ggx;

#include "res://assets/materials/shaders/SH_Triplanar.gdshaderinc"

uniform vec4 albedo : source_color = vec4(1.0);
uniform sampler2D texture_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D texture_normal : hint_roughness_normal, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D texture_orm : hint_default_white, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D screen_texture : hint_screen_texture, repeat_disable, filter_linear;
uniform sampler2D depth_texture : hint_depth_texture, repeat_disable, filter_nearest;
uniform float triplanar_scale : hint_range(0.01, 8.0) = 1.0;
uniform float triplanar_sharpness : hint_range(1.0, 16.0) = 4.0;
uniform float normal_scale : hint_range(0.0, 4.0) = 1.0;
// Index of refraction (ruby 1.76, quartz 1.54) and its spread from red to blue.
uniform float ior : hint_range(1.0, 2.5, 0.01) = 1.76;
uniform float dispersion : hint_range(0.0, 0.1, 0.001) = 0.02;
// Screen-space bend of the refracted view, as a fraction of the screen.
uniform float refraction_strength : hint_range(0.0, 0.5, 0.001) = 0.08;
// Path length through the body in units of the colour's own depth.
uniform float thickness : hint_range(0.0, 4.0, 0.01) = 1.0;
// Light the cut returns by internal reflection: oblique facets glow, the
// table stays darker, so the facets read even over a flat background.
uniform float brilliance : hint_range(0.0, 2.0, 0.01) = 1.0;
uniform vec4 emission : source_color = vec4(0.0, 0.0, 0.0, 1.0);
uniform float emission_energy : hint_range(0.0, 16.0, 0.01) = 0.0;

void vertex() {
	tri_pos = VERTEX;
	tri_normal = NORMAL;
}

vec3 behind(vec2 uv, float frag_depth) {
	// Keep only what lies behind this fragment; anything in front would smear in.
	// Godot 4.3+ uses reversed Z on every renderer: a smaller depth is farther.
	float d = textureLod(depth_texture, uv, 0.0).r;
	return d < frag_depth ? textureLod(screen_texture, uv, 0.0).rgb : vec3(-1.0);
}

void fragment() {
	TriUV t = tri_uv(triplanar_scale, triplanar_sharpness);
	vec4 orm = tri_sample(texture_orm, t);
	vec3 body = albedo.rgb * tri_sample(texture_albedo, t).rgb;
	vec3 n_obj = tri_normal_object(texture_normal, t, normal_scale);
	vec3 n = normalize(mat3(VIEW_MATRIX) * (MODEL_NORMAL_MATRIX * n_obj));
	float ndv = clamp(dot(n, VIEW), 0.0, 1.0);

	vec3 seen = vec3(0.0);
	vec3 fallback = textureLod(screen_texture, SCREEN_UV, 0.0).rgb;
	for (int c = 0; c < 3; c++) {
		float eta = 1.0 / (ior + (float(c) - 1.0) * dispersion);
		vec3 r = refract(-VIEW, n, eta);
		vec2 uv = clamp(SCREEN_UV + vec2(r.x, -r.y) * refraction_strength, vec2(0.0), vec2(1.0));
		vec3 s = behind(uv, FRAGCOORD.z);
		seen[c] = s.r < 0.0 ? fallback[c] : s[c];
	}
	float path = thickness * (1.0 + 2.0 * (1.0 - ndv));
	vec3 transmitted = seen * pow(max(body, vec3(0.002)), vec3(path));
	transmitted *= mix(1.0, 0.45 + 1.6 * (1.0 - ndv), brilliance);

	float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
	float fresnel = f0 + (1.0 - f0) * pow(1.0 - ndv, 5.0);
	ALBEDO = body * 0.05;
	AO = orm.r;
	ROUGHNESS = orm.g;
	METALLIC = 0.0;
	SPECULAR = clamp(sqrt(f0 / 0.16), 0.0, 1.0);
	NORMAL = n;
	EMISSION = transmitted * (1.0 - fresnel) * EXPOSURE + emission.rgb * emission_energy;
}
