<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About the game scene

Explanation for anyone changing how a level is played on screen: why
`Match3Game` is a thin layer over the core session, and why a move is
shown the way it is. Facts and signal tables are in
[Match3Game](../reference/match3-game.md).

## The session decides, the scene shows

The rules live in `montezuma::LevelSession`, which runs without Godot and
is what the balance simulator plays. The game scene only carries
commands in (a swap from the view, a shuffle or hint from the HUD, the
frame's time) and carries the session's report out (signals for the HUD,
a result for the view, cues for audio). It never decides whether a move
is legal, how many gems a clear is worth, or when the level ends.

That keeps the level that the simulator balanced and the level a player
plays the same level. It also means the scene can be replaced, for the
web intro level or a mini game, without touching a rule.

## Why the board is mirrored

The [board view](about-the-board-view.md) draws a `Match3Board` and, after
each animation, checks every cell against it. The session owns its own
board, with steel, gems, chains and power-ups on top. Rather than teach
the view about the session, the scene copies the session's board into a
`Match3Board` before each animation. The view stays exactly as it was for
free play, and the check it already makes now proves the scene showed the
session's state.

## Why one move is one animation

A swap can set off cascades, then one or more totems whose clears cascade
too, then an automatic reshuffle. The session resolves all of it at once;
the view animates a list of steps. Joining the swap's steps, each totem's
steps and the shuffle into one result lets the existing animator play the
whole move in order, with one `animation_finished` at the end and no
queue to keep in step with the board. The step reports line up one to one
with those steps (a core test checks it), so the sound for each step is
played on the frame the view shows it.

Effects the view cannot show yet, such as a paint totem's repaint or a
gem placed by harvest, still change the board; the view snaps to the
final state at the end of the move. WP-29 adds their animations.

## Why the result waits for the animation

The session knows a level is won the moment the winning swap resolves,
but the player has not seen it yet. `level_finished` waits for the board
to stop, so the winning cascade plays out before the results screen
covers it. Time no longer counts once the session has ended the level.
