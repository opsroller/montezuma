// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Shared code for SH_Water (desktop) and SH_WaterSimple (mobile, web):
// procedural ripples with their analytic slope, caustic lines and the
// absorption of light along a path through the water. Everything is
// computed, nothing is sampled, so the water adds no texture and no
// sampler slot (see the Metal note in SH_Triplanar.gdshaderinc).
//
// Units are metres and seconds in world space, Y up. The ripple field is a
// height h(x, z, t); wt_slope returns (dh/dx, dh/dz), and the surface
// normal is normalize(vec3(-dh/dx, 1, -dh/dz)).

// Integer hash of a lattice point to a gradient. Integer maths so it gives
// the same lattice on every GPU, unlike sin-based hashes, and no trig: the
// gradient is two hashed bytes mapped to -1..1 (not unit length, which
// gradient noise does not need).
vec2 wt_gradient(vec2 cell) {
	uvec2 q = uvec2(ivec2(cell) + ivec2(32768));
	q = q * uvec2(1664525u, 1013904223u);
	uint n = (q.x ^ (q.y >> 5u)) * 2654435761u;
	n ^= n >> 15u;
	return vec2(float(n & 255u), float((n >> 8u) & 255u)) * (2.0 / 255.0) - 1.0;
}

// Gradient noise with its derivative: returns (value, d/dx, d/dy), value
// roughly in -0.7..0.7. Quintic fade so the derivative is continuous.
vec3 wt_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = p - i;
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
	vec2 ga = wt_gradient(i);
	vec2 gb = wt_gradient(i + vec2(1.0, 0.0));
	vec2 gc = wt_gradient(i + vec2(0.0, 1.0));
	vec2 gd = wt_gradient(i + vec2(1.0, 1.0));
	float va = dot(ga, f);
	float vb = dot(gb, f - vec2(1.0, 0.0));
	float vc = dot(gc, f - vec2(0.0, 1.0));
	float vd = dot(gd, f - vec2(1.0, 1.0));
	float k1 = vb - va;
	float k2 = vc - va;
	float k3 = va - vb - vc + vd;
	float v = va + u.x * k1 + u.y * k2 + u.x * u.y * k3;
	vec2 d = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd)
			+ du * vec2(k1 + u.y * k3, k2 + u.x * k3);
	return vec3(v, d);
}

// Slope of the ripple field at world position p (x, z). Two long, slow
// swells set the direction the light dances in; noise octaves drifting
// with the flow break them up. octaves 1..3. scale > 0 grows the ripples
// (wavelength and height together, so the slope does not change with it);
// the slope is capped so no setting tips the surface past about 30 degrees.
vec2 wt_slope(vec2 p, float t, float scale, float strength, float flow, int octaves) {
	vec2 q = p / scale;
	vec2 s = vec2(0.0);
	// Swells: amplitude a, wave vector k, slope = a * k * cos(k.q - w t).
	const vec2 K1 = vec2(1.9, 0.7);
	const vec2 K2 = vec2(-0.6, 2.6);
	s += 0.018 * K1 * cos(dot(K1, q) - 1.1 * t);
	s += 0.012 * K2 * cos(dot(K2, q) - 1.4 * t);
	vec2 drift = vec2(0.8, 0.6) * flow * t / scale;
	float amp = 0.045;
	float freq = 3.1;
	for (int o = 0; o < 3; o++) {
		if (o >= octaves) {
			break;
		}
		vec2 off = drift * (1.0 + 0.6 * float(o)) + vec2(17.3, 4.1) * float(o);
		s += amp * freq * wt_noise(q * freq + off).yz;
		amp *= 0.5;
		freq *= 2.03;
	}
	s *= strength * 7.0;
	float m = length(s);
	return m > 0.6 ? s * (0.6 / m) : s;
}

vec3 wt_normal(vec2 slope) {
	return normalize(vec3(-slope.x, 1.0, -slope.y));
}

// Caustic lines on a floor point p (x, z): the ridges where two warped,
// counter-drifting noise fields cross zero, sharpened. 0 dark, about 1 on a
// line. layers 1 (mobile, web) or 2 (desktop).
float wt_caustics(vec2 p, float t, float scale, int layers) {
	vec2 q = p / scale;
	vec2 warp = vec2(wt_noise(q * 0.55 + vec2(0.0, 0.21 * t)).x, wt_noise(q * 0.55 + vec2(11.7, -0.19 * t)).x);
	float r = 1.0 - min(abs(wt_noise(q * 1.6 + warp * 1.4 + vec2(0.13 * t, 0.0)).x) * 3.2, 1.0);
	float c = r * r * r * r * r * r;
	if (layers > 1) {
		float r2 = 1.0 - min(abs(wt_noise(q * 2.3 - warp * 1.1 + vec2(0.0, -0.17 * t) + 5.0).x) * 3.6, 1.0);
		c = max(c, r2 * r2 * r2 * r2 * r2 * r2 * 0.7);
	}
	return c;
}

// Fraction of light left after a path of `len` metres (Beer-Lambert),
// per channel; absorption is per metre.
vec3 wt_transmittance(vec3 absorption, float len) {
	return exp(-max(absorption, vec3(0.0)) * max(len, 0.0));
}

// Schlick Fresnel for water (F0 = 0.02, IOR 1.33).
float wt_fresnel(float ndv) {
	float m = 1.0 - clamp(ndv, 0.0, 1.0);
	float m2 = m * m;
	return 0.02 + 0.98 * m2 * m2 * m;
}
