<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About rule sets and themes

*Montezuma's* is an umbrella title: the classic campaign first, later
mini games that reuse the same board with other rules and other looks.
This page explains how the design leaves room for that without building
it now.

## The idea

A mini game is the classic game with two things swapped:

- a **rule set**: which goal, which totems and power-ups, which timer and
  Frenzy numbers, which obstacles;
- a **theme**: which pieces, board, sounds and music.

Both are named by an id in each level file (`rules`, `theme`), defaulting
to `classic` and `temple`. Nothing else in a level file changes.

## Why data, not code paths

The rules pages already put every number in a table: totem tiers,
power-up tiers, timer, gems on board, Frenzy, scoring. A rule set is one
named bundle of those tables, so a variant is a new data file plus, only
when a genuinely new mechanic is needed, a new rule. The core then runs
any rule set the same way, and the simulator can balance a variant with
the same bots. Rejected: a flag per variant inside the rules code, which
grows a branch for every mini game and makes each one harder to test.

## What a rule set can change

| Part | Classic | Examples of a variant |
|---|---|---|
| Goal | gems (`GOAL-01`) | colour quotas (`GOAL-06`), a score target, clear all steel |
| Clock | timer tiers | a fixed 60 s sprint, or moves instead of time |
| Totems | seven, by colour | none, or a different effect per colour |
| Power-ups | six, from runs of 4+ | a reduced set, or all from the start |
| Obstacles | chains and steel | none, or more |
| Board | 8 × 8, 7 kinds | other sizes and fewer kinds (LevelData already allows 3 to 64 cells a side and 3 to 7 kinds) |

The original itself had such variants once the campaign was over: an
arcade mode of 60 seconds with every totem and power-up (S7), and an
endless mode (S5). Both are good first mini games because they need no
new rules, only new numbers.

## What a theme can change

Pieces, board and environment models, materials, sounds and music, all
looked up by the logical names in the asset manifest and audio cue
contracts. A theme keeps the kind count and order, so a rule set and a
theme combine freely.

## What this asks of other packages now

- Level files carry `rules` and `theme` (optional, with the defaults).
- The core keeps tuning tables swappable (the totem table already is).
- The asset manifest and audio cues are looked up through the theme id,
  with `temple` as the fallback.

Building any mini game is later work, one package each.
