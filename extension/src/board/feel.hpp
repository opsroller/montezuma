// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

// Feel constants for the board view: every timing and size the player can
// feel lives here, so the feel pass (WP-40) tunes one file. Plain values
// with no Godot types, so the Godot-free board tests can read them too.
// Targets come from the "Feedback" section of docs/reference/rules-spec.md.

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

// Cascades (WP-13). Each step: matched pieces pop and shrink away, then
// pieces fall under gravity and new ones drop in from above the board.
inline constexpr double kClearPopSeconds = 0.05;
inline constexpr double kClearShrinkSeconds = 0.10;
inline constexpr double kClearPopScale = 1.25;
// Gravity in cells per second squared: one row takes about 0.13 s, the
// whole 8-row board about 0.37 s.
inline constexpr double kFallGravityCells = 120.0;
inline constexpr double kFallMinSeconds = 0.08;
// New pieces grow from this scale while they cover the first part of their fall.
inline constexpr double kSpawnStartScale = 0.3;
inline constexpr double kSpawnGrowFraction = 0.4;
// Pause between one cascade step landing and the next one clearing.
inline constexpr double kCascadeGapSeconds = 0.04;
// No move left: pieces glide to their shuffled cells.
inline constexpr double kShuffleSeconds = 0.35;
// No arrangement worked: old pieces shrink away, the new deal grows in.
inline constexpr double kRedealOutSeconds = 0.18;
inline constexpr double kRedealInSeconds = 0.22;

// Totem and power-up effects (WP-29). A firing totem gets a lead-in before
// its clear: fireballs fall, labels rise, a repaint morphs. Every transient
// effect ends by the end of the step it belongs to (visual target,
// readability rule 3); the planner clamps them to it.
inline constexpr double kTotemLeadSeconds = 0.32;
// Fireballs fall from this many cells above the board during the lead.
inline constexpr double kFireballDropCells = 5.0;
// A power-up going off: blasts, beams and flashes.
inline constexpr double kPowerUpBurstSeconds = 0.28;
// Floating text (+60 s, x2, +1500) rises this far while it fades.
inline constexpr double kLabelSeconds = 0.5;
inline constexpr double kLabelRiseCells = 0.9;
// A collected gem flies up and fades; steel shatters; a chain falls away.
inline constexpr double kGemFlySeconds = 0.3;
inline constexpr double kGemFlyCells = 1.5;
inline constexpr double kBreakSeconds = 0.25;
// A marker (power-up badge, gem, steel, chain) pops in or shrinks away.
inline constexpr double kMarkPopSeconds = 0.18;
inline constexpr double kMarkGoneSeconds = 0.12;
// A hint ring pulses this often while it shows.
inline constexpr double kHintPulseSeconds = 0.6;

// Selected piece grows and lifts so it reads against its neighbours.
inline constexpr double kSelectedScale = 1.18;
inline constexpr double kSelectedLiftCells = 0.12;

// Swipe (WP-14): a drag this far, as a fraction of one cell on screen, is a
// swap toward that neighbour; never less than the minimum, so a shaky tap on
// a dense screen stays a tap.
inline constexpr double kSwipeCellFraction = 0.3;
inline constexpr double kSwipeMinMillimetres = 2.5;

// Camera: near top-down, as the original's 2D board read.
inline constexpr double kCameraTiltDegrees = 15.0;
inline constexpr double kCameraFovDegrees = 40.0;
// Empty space kept around the board, as a fraction of the viewport, so it
// stays clear of platform safe areas and HUD edges.
inline constexpr double kCameraMargin = 0.08;

}  // namespace montezuma::godot_binding::feel
