// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// Feel constants for the board view: every timing and size the player can
// feel lives here, so the feel pass (WP-40) tunes one file. Plain values
// with no Godot types, so the Godot-free board tests can read them too.
// Targets come from the "Feel targets" section of docs/rules-spec.md.

namespace montezuma::godot_binding::feel {

// Swaps are snappy: a legal swap resolves at once.
inline constexpr double kSwapSeconds = 0.12;
// An illegal swap moves the pair toward each other and back, each leg this long.
inline constexpr double kIllegalLegSeconds = 0.09;
// How far (in cells) an illegal swap travels before bouncing back.
inline constexpr double kIllegalReach = 0.45;
// Changed cells drop in from this many cells above and settle in this time.
inline constexpr double kSettleSeconds = 0.18;
inline constexpr double kSettleDropCells = 1.5;

// Selected piece grows and lifts so it reads against its neighbours.
inline constexpr double kSelectedScale = 1.18;
inline constexpr double kSelectedLiftCells = 0.12;

// Camera: near top-down, as the original's 2D board read.
inline constexpr double kCameraTiltDegrees = 15.0;
inline constexpr double kCameraFovDegrees = 40.0;
// Empty space kept around the board, as a fraction of the viewport, so it
// stays clear of platform safe areas and HUD edges.
inline constexpr double kCameraMargin = 0.08;

}  // namespace montezuma::godot_binding::feel
