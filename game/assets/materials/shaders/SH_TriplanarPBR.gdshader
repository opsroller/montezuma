// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Carved stone for desktop: a PBR texture set (base colour, normal, ORM,
// height) mapped triplanar in object space, with parallax occlusion on
// upward faces so chisel marks, pits and cracks keep their depth as the
// camera moves. Mobile and web use the base StandardMaterial3D of the same
// name instead (same textures, no parallax). Cost: 3 projections x 3
// samples, plus 8 to 24 height samples on upward faces.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

#include "res://assets/materials/shaders/SH_Triplanar.gdshaderinc"

uniform vec4 albedo : source_color = vec4(1.0);
uniform sampler2D texture_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D texture_normal : hint_roughness_normal, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D texture_orm : hint_default_white, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D texture_height : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform float triplanar_scale : hint_range(0.01, 8.0) = 1.0;
uniform float triplanar_sharpness : hint_range(1.0, 16.0) = 4.0;
uniform float normal_scale : hint_range(0.0, 4.0) = 1.0;
uniform float specular : hint_range(0.0, 1.0) = 0.5;
// Deepest carving in metres; 0 turns parallax off.
uniform float height_depth : hint_range(0.0, 0.1, 0.001) = 0.03;
uniform int parallax_min_layers : hint_range(1, 64) = 8;
uniform int parallax_max_layers : hint_range(1, 64) = 24;

varying vec3 camera_obj;

void vertex() {
	tri_pos = VERTEX;
	tri_normal = NORMAL;
	camera_obj = (inverse(MODEL_MATRIX) * vec4(CAMERA_POSITION_WORLD, 1.0)).xyz;
}

void fragment() {
	TriUV t = tri_uv(triplanar_scale, triplanar_sharpness);
	if (height_depth > 0.0 && t.n.y > 0.0 && t.w.y > 0.01) {
		vec3 view_obj = normalize(camera_obj - tri_pos);
		t.y = tri_parallax_y(texture_height, t.y, t.gy, view_obj, height_depth * triplanar_scale,
				parallax_min_layers, parallax_max_layers);
	}
	vec4 orm = tri_sample(texture_orm, t);
	ALBEDO = albedo.rgb * tri_sample(texture_albedo, t).rgb;
	AO = orm.r;
	ROUGHNESS = orm.g;
	METALLIC = orm.b;
	SPECULAR = specular;
	vec3 n = tri_normal_object(texture_normal, t, normal_scale);
	NORMAL = normalize(mat3(VIEW_MATRIX) * (MODEL_NORMAL_MATRIX * n));
}
