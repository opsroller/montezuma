// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Object-space triplanar mapping shared by the PBR shaders. Our meshes have
// no UVs, so every texture is projected along the three object axes and
// blended by the surface normal. The vertex stage writes tri_pos and
// tri_normal; the fragment stage builds a TriUV, samples with tri_sample and
// turns the normal map into a view-space normal with tri_normal_view.
//
// Projection layout (u right, v down, image up = -v):
//   X faces: u along -Z * sign(n.x), up = +Y
//   Y faces: u along +X, up = -Z (the board's tile tops)
//   Z faces: u along +X * sign(n.z), up = +Y

varying vec3 tri_pos;
varying vec3 tri_normal;

struct TriUV {
	vec2 x;
	vec2 y;
	vec2 z;
	vec3 w;
	vec3 n;
};

TriUV tri_uv(float scale, float sharpness) {
	TriUV t;
	t.n = normalize(tri_normal);
	vec3 w = pow(abs(t.n), vec3(sharpness));
	t.w = w / dot(w, vec3(1.0));
	vec3 p = tri_pos * scale;
	float sx = t.n.x < 0.0 ? -1.0 : 1.0;
	float sz = t.n.z < 0.0 ? -1.0 : 1.0;
	t.x = vec2(-p.z * sx, -p.y);
	t.y = vec2(p.x, p.z);
	t.z = vec2(p.x * sz, -p.y);
	return t;
}

vec4 tri_sample(sampler2D s, TriUV t) {
	return texture(s, t.x) * t.w.x + texture(s, t.y) * t.w.y + texture(s, t.z) * t.w.z;
}

// Tangent-space normal from an RG normal map (Godot may store only RG).
vec3 tri_unpack(vec4 c, float scale) {
	vec2 xy = (c.xy * 2.0 - 1.0) * scale;
	return vec3(xy, sqrt(max(0.0, 1.0 - dot(xy, xy))));
}

// Blend the three projected normal maps into one object-space normal.
vec3 tri_normal_object(sampler2D s, TriUV t, float scale) {
	float sx = t.n.x < 0.0 ? -1.0 : 1.0;
	float sz = t.n.z < 0.0 ? -1.0 : 1.0;
	vec3 a = tri_unpack(texture(s, t.x), scale);
	vec3 b = tri_unpack(texture(s, t.y), scale);
	vec3 c = tri_unpack(texture(s, t.z), scale);
	vec3 nx = a.x * vec3(0.0, 0.0, -sx) + a.y * vec3(0.0, 1.0, 0.0) + a.z * t.n;
	vec3 ny = b.x * vec3(1.0, 0.0, 0.0) + b.y * vec3(0.0, 0.0, -1.0) + b.z * t.n;
	vec3 nz = c.x * vec3(sz, 0.0, 0.0) + c.y * vec3(0.0, 1.0, 0.0) + c.z * t.n;
	return normalize(nx * t.w.x + ny * t.w.y + nz * t.w.z);
}

// Steep parallax with linear refinement on the Y projection only: the game
// camera looks almost straight down, so tile and frame tops carry the depth.
// view_obj points from the surface to the camera in object space; depth is
// in texture units (metres times the triplanar scale). White height = top.
vec2 tri_parallax_y(sampler2D height, vec2 uv, vec3 view_obj, float depth, int min_layers, int max_layers) {
	float vy = max(view_obj.y, 0.2);
	float layers = mix(float(max_layers), float(min_layers), vy);
	float layer_step = 1.0 / layers;
	vec2 delta = -view_obj.xz / vy * depth * layer_step;
	vec2 gx = dFdx(uv);
	vec2 gy = dFdy(uv);
	vec2 cur = uv;
	float layer = 0.0;
	float d = 1.0 - textureGrad(height, cur, gx, gy).r;
	for (int i = 0; i < 64; i++) {
		if (layer >= d) {
			break;
		}
		cur += delta;
		layer += layer_step;
		d = 1.0 - textureGrad(height, cur, gx, gy).r;
	}
	vec2 prev = cur - delta;
	float after = d - layer;
	float before = (1.0 - textureGrad(height, prev, gx, gy).r) - layer + layer_step;
	return mix(cur, prev, clamp(after / (after - before + 1e-5), 0.0, 1.0));
}
