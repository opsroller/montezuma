// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Match-burst spark: additive, unshaded, brightest where it faces the
// camera so tiny spheres read as glints. Fades with the particle's age
// (INSTANCE_CUSTOM.y is CPUParticles3D's lifetime fraction).
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, shadows_disabled;

uniform vec4 spark_color : source_color = vec4(1.0, 0.85, 0.47, 1.0);
uniform float intensity : hint_range(0.0, 10.0) = 4.0;

varying float age;

void vertex() {
	age = INSTANCE_CUSTOM.y;
}

void fragment() {
	float facing = pow(abs(dot(NORMAL, VIEW)), 1.5);
	ALBEDO = spark_color.rgb * intensity * facing * (1.0 - age);
}
