// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Ground mist around the temple for the volumetric fog, modelled on dawn
// mist in a jungle plaza: neutral white droplets that take their colour from
// the light, lying on the plaza in drifting banks with clear ground between
// them, thickest at the bottom and away from the platform, with a soft,
// uneven top. The banks come from large slow noise, the wisps on them from
// fine noise drawn out along the wind.
// Over the board the mist is a thin veil: drifting patches lying on the
// tiles, thin enough to see the tiles and pieces through, with clear tiles
// between them so it never covers the whole board, and no higher than
// veil_height, below the floating score text (readability rule 2).
// Godot lights fog by single scattering only, which leaves mist this
// thick far darker than real mist; the sun's light_volumetric_fog_energy
// stands in for the light it scatters many times. Desktop tiers only. Parameters:
// docs/reference/board-themes.md#atmosphere.
shader_type fog;

// Extinction per metre in the core of a bank.
uniform float density : hint_range(0.0, 8.0, 0.001) = 2.0;
// Mist is water: it scatters every colour alike, so this stays near white.
uniform vec4 albedo : source_color = vec4(0.92, 0.93, 0.94, 1.0);
// Half the side of the board, frame included, around the board centre, in
// metres; the tile tops are at y = 0.
uniform float board_half = 4.55;
// Height of the plaza floor the mist lies on, in world metres.
uniform float ground_y = -1.92;
// Tallest a bank gets above the floor, in metres; each bank's own top is
// this scaled by its noise. top_soft is how far its top fades out.
uniform float bank_height = 1.4;
uniform float top_soft = 0.6;
// Distance outside the board, in metres, over which the mist grows from
// `near_amount` of full thickness to all of it: thicker away from the
// platform, which is what reads as depth from above.
uniform float thicken_over = 7.0;
uniform float near_amount : hint_range(0.0, 1.0) = 0.35;
// Seamless noise. Banks: one tile spans bank_scale metres. Wisps: wisp_scale
// metres, stretched `wisp_stretch` times along the wind.
uniform sampler3D density_texture : filter_linear, repeat_enable, hint_default_white;
uniform float bank_scale = 16.0;
uniform float wisp_scale = 4.0;
uniform float wisp_stretch : hint_range(1.0, 8.0) = 3.0;
uniform float wisp_amount : hint_range(0.0, 1.0) = 0.55;
// Noise values mapped to no bank and a full bank: a narrow range gives
// clear ground between the banks.
uniform vec2 coverage = vec2(0.42, 0.68);
uniform vec3 wind = vec3(0.12, 0.0, -0.05);
// Godot's fog does not shadow itself, so light deep in a bank would be as
// bright as at its top. The lower part of each bank scatters this much of
// the light instead, which gives the banks their rounded, sunlit tops.
uniform float base_shade : hint_range(0.0, 1.0) = 0.55;
// The veil over the board: extinction per metre on the tiles, the height
// above the tiles where it has thinned to nothing, the size in metres of
// one tile of its noise (its patches are a few tiles across, with clear
// tiles between them) and the noise values mapped to no veil and full veil.
// It spreads veil_spread metres past the board edge, where it meets the banks.
uniform float veil_density : hint_range(0.0, 4.0, 0.001) = 0.9;
uniform float veil_height = 0.25;
uniform float veil_scale = 14.0;
uniform vec2 veil_coverage = vec2(0.5, 0.72);
uniform float veil_spread = 1.0;

void fog() {
	vec2 d = abs(WORLD_POSITION.xz) - vec2(board_half);
	float out_dist = max(d.x, d.y);
	float thicken = mix(near_amount, 1.0, smoothstep(0.0, max(thicken_over, 0.001), out_dist));

	// Banks: large noise, drifting with the wind, sets each bank's thickness.
	vec3 drift = wind * TIME;
	vec2 bank_uv = (WORLD_POSITION.xz - drift.xz) / bank_scale;
	float bank = texture(density_texture, vec3(bank_uv, 0.37)).r;
	bank = smoothstep(coverage.x, coverage.y, bank);
	float top = bank_height * bank * thicken;

	// Wisps: finer noise drawn out along the wind breaks the banks' surface
	// and their edges into streaks.
	vec3 q = WORLD_POSITION - drift * 1.6;
	vec3 w_dir = normalize(vec3(wind.x, 0.0, wind.z) + vec3(1e-4, 0.0, 0.0));
	q += w_dir * dot(q, w_dir) * (1.0 / wisp_stretch - 1.0);
	float wisp = texture(density_texture, q / wisp_scale).r;
	float h = WORLD_POSITION.y - ground_y;
	top *= mix(1.0, 0.4 + 1.2 * wisp, wisp_amount);

	// Thickest on the floor, thinning to nothing at the bank's top.
	float rel = h / max(top, 0.001);
	float profile = (1.0 - smoothstep(1.0 - top_soft / max(top, 0.001), 1.0, rel)) * step(0.0, h);
	profile *= mix(1.0, smoothstep(0.25, 0.75, wisp) * 1.4, wisp_amount * smoothstep(0.3, 1.0, rel));
	float banks = density * profile * step(0.001, top);

	// Veil: patches from mid-sized noise on its own slice, drifting with the
	// banks, lying on the tiles and thinning to nothing at veil_height.
	vec2 veil_uv = (WORLD_POSITION.xz - drift.xz) / veil_scale;
	float veil_patch = smoothstep(veil_coverage.x, veil_coverage.y, texture(density_texture, vec3(veil_uv, 0.71)).r);
	float vh = WORLD_POSITION.y / max(veil_height, 0.001);
	float veil_layer = (1.0 - smoothstep(0.0, 1.0, vh)) * step(-0.05, WORLD_POSITION.y);
	float over = 1.0 - smoothstep(0.0, max(veil_spread, 0.001), out_dist);
	float veil = veil_density * veil_patch * veil_layer * over * mix(0.6, 1.0, wisp);

	DENSITY = max(banks, veil);
	ALBEDO = albedo.rgb * mix(base_shade, 1.0, clamp(max(rel, step(0.001, veil)), 0.0, 1.0));
}
