// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Selected-cell glow: a pulsing square ring that traces the tile edge,
// with a faint inner wash. Drawn on a 1x1 plane lying on the board,
// additive and unshaded so it reads under any lighting. Works on every
// renderer; one texture-free pass over one cell.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 glow_color : source_color = vec4(1.0, 0.83, 0.42, 1.0);
uniform float intensity : hint_range(0.0, 8.0) = 2.2;
uniform float pulse_speed = 6.0;

void fragment() {
	vec2 p = UV * 2.0 - 1.0;
	float d = max(abs(p.x), abs(p.y));
	float ring = smoothstep(0.74, 0.88, d) * (1.0 - smoothstep(0.9, 0.99, d));
	float wash = (1.0 - d) * 0.18;
	float pulse = 0.7 + 0.3 * sin(TIME * pulse_speed);
	ALBEDO = glow_color.rgb * intensity * (ring + wash) * pulse;
}
