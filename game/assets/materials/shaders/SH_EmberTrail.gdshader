// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Ember with a trail: the particle's tube trail mesh, brightest at the
// head and fading along the trail, coloured by the particle's colour ramp
// (COLOR). Additive and unshaded, in HDR for glow. Shown on tiers with
// particle trails (quality:particle_trails). Parameters: docs/reference/fire.md.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, particle_trails;

uniform float intensity : hint_range(0.0, 40.0) = 8.0;

void fragment() {
	// UV.y runs along the trail; the tube's tip is at 0.
	float along = clamp(1.0 - UV.y, 0.0, 1.0);
	ALBEDO = COLOR.rgb * COLOR.a * along * along * intensity;
}
