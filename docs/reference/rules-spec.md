<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Rules spec

The reference for how *Montezuma's* plays in its `classic` rule set: the
rules of *The Treasures of Montezuma* (developed by Visual Shape,
published by Alawar; PC 2006 [S8], iOS 2009 [S6]), rebuilt from public sources and written in our own words. Nothing
here comes from the original's files.

Companion pages:

- [Power Totems](rules-totems.md) (`TOT-*`), [power-ups](rules-power-ups.md)
  (`PWR-*`), [bonus games](rules-bonus-games.md) (`BG-*`).
- [Progression](progression.md): episodes, levels, gem goals, unlocks,
  upgrades, trophies and the intro level.
- [Sources](rules-sources.md): the evidence behind each rule.
- Why the rules are what they are, and every deviation from the original:
  [About the rules](../explanation/about-the-rules.md). How rule sets and
  themes make room for mini games:
  [About rule sets and themes](../explanation/about-rule-sets-and-themes.md).

## Certainty marks

Every rule is either **confirmed** (two independent sources, ids in
brackets such as `[S1, S4]`) or **[chosen]**: a deliberate decision where
the sources are silent or split, with its reason in the explanation page.
Nothing is left as a guess.

## Glossary

| Term | Meaning |
|---|---|
| Piece | One object in a board cell. Has a kind and may carry one gem or one power-up |
| Kind | The piece's colour, 0 to 6: `red`, `orange`, `yellow`, `green`, `blue`, `purple`, `white` |
| Run | A horizontal or vertical line of 3 or more matchable pieces of one kind |
| Move | One legal swap and everything it sets off until the board is still |
| Step | One clear-fall-refill round of a move. Step 0 clears the swap's own runs; step *k* is the *k*th cascade |
| Gem | A collectable carried by a piece. Collecting gems is the level goal |
| Power-up | A special effect carried by a piece, set off when that piece is cleared (see `PWR-*`) |
| Totem | One of seven per-kind powers that fires after two consecutive matching moves (see `TOT-*`) |
| Steel piece | A piece encased in steel: it cannot be swapped or matched |
| Chained piece | A piece that cannot be swapped but still matches |
| Score Frenzy | A timed state that doubles points |
| Golden star | The upgrade currency, earned by winning levels and passing bonus levels |
| Star sand | A second source of golden stars, gathered during Score Frenzy |

## Board (`BOARD-*`)

State: `width` × `height` cells (8 × 8 [chosen]), `kinds` = 7 [S2].

1. `BOARD-01` A cell holds one piece, or is briefly empty while pieces fall.
2. `BOARD-02` Pieces are drawn uniformly from the level's kinds by the
   level's seeded random generator.
3. `BOARD-03` A new board has no run on it and at least one legal move.
   It is regenerated until both hold (the core caps the retries).
4. `BOARD-04` A new board then places gems (`GEM-02`), chains from the
   level data (`OBS-05`) and no steel or power-ups.

## Moves (`MOVE-*`)

1. `MOVE-01` The player swaps two orthogonally adjacent pieces [S3, S4].
2. `MOVE-02` A swap is legal only if it makes at least one run. An illegal
   swap is undone, costs nothing and is not a move.
3. `MOVE-03` Steel and chained pieces cannot take part in a swap.
4. `MOVE-04` Input is ignored while a move resolves.

## Resolving a move (`RES-*`)

1. `RES-01` Find every run on the board. Steel pieces end a run.
2. `RES-02` Clear every cell in a run at once; a cell in two runs is
   cleared once. Add the cells of every power-up set off this step
   (`PWR-04`).
3. `RES-03` For each cleared piece, in this order: collect its gem
   (`GEM-03`), score it (`SCORE-01`), add it to the Frenzy bar (`FRZ-01`).
4. `RES-04` Pieces above fall straight down to fill gaps; new pieces enter
   from the top (`OBS-02` may make one steel; `GEM-04` may give one a gem).
5. `RES-05` Place power-ups earned this step (`PWR-01`).
6. `RES-06` Repeat from `RES-01`; each repeat is the next step. Stop when
   no run remains.
7. `RES-07` Then fire the totems this move charged (`TOT-03`); each totem's
   clear resolves from `RES-02` as further steps of the same move.
8. `RES-08` Then check the goal (`GOAL-02`). Then, if no legal move
   exists, reshuffle (`SHUF-02`).

## Gems (`GEM-*`)

State: `gems_on_board`, `gems_max` (5 at start; 6, 7, 8 with upgrades
[S1]), `gems_collected` (starts 0).

1. `GEM-01` A gem sits on a plain piece: not steel, no power-up, no other
   gem [chosen].
2. `GEM-02` A new board places `gems_max` gems on distinct random plain
   pieces.
3. `GEM-03` When a piece with a gem is cleared, by a run or by any effect,
   the gem is collected: `gems_collected` +1 [S3, S4, S5].
4. `GEM-04` After each refill, while `gems_on_board < gems_max`, put a gem
   on a random new plain piece from that refill. Leftover shortfall waits
   for the next refill [chosen].
5. `GEM-05` Gems fall with their pieces and survive shuffles.

## Goal and timer (`GOAL-*`)

State: `gem_goal` (from the level and difficulty), `time_left` seconds.

1. `GOAL-01` The level is won when `gems_collected >= gem_goal` before
   time runs out, and lost when `time_left` reaches 0 first
   [S3, S4, S5].
2. `GOAL-02` The goal is checked when a move has fully resolved. A move
   that started before time ran out finishes, and wins if it reaches the
   goal [chosen].
3. `GOAL-03` `time_left` starts at the level's base clock (270 s unless
   the level data sets `base_clock`) plus the player's timer tier bonus
   ([progression](progression.md#upgrades): +30 s a tier). The bonus
   applies on every level, so a timer upgrade never loses its value. A
   level's base clock covers a human's pace by the
   [clock rule](balance.md#clock) [S1, S4, chosen].
4. `GOAL-04` Time counts down in real time while the board is shown,
   including while moves resolve. It stops while paused and while a
   result or upgrade screen is open.
5. `GOAL-05` `time_left` has no upper cap. Time effects add to it.
6. `GOAL-06` Variant: a level may set per-kind `quotas` instead of, or as
   well as, `gem_goal` (used by other rule sets; see
   [about rule sets](../explanation/about-rule-sets-and-themes.md)). A
   quota counts cleared pieces of its kind; all quotas and the gem goal
   must be met.

## Difficulty (`DIFF-*`)

1. `DIFF-01` Two modes: `normal` (the original's Adventure) and `hard`
   [S3].
2. `DIFF-02` `hard` unlocks when the last normal level is won [S3].
3. `DIFF-03` `hard` raises the gem goal: the level's `gem_goal.hard`,
   by default `normal × 1.5` rounded up to a multiple of 5 [chosen].
   Timer and everything else are the same.

## Scoring (`SCORE-*`)

1. `SCORE-01` Each cleared piece scores `10 × (step + 1)` [chosen].
2. `SCORE-02` Each gem collected scores 50 more [chosen].
3. `SCORE-03` During Score Frenzy all points are doubled [S2].
4. `SCORE-04` A multiplier power-up multiplies the whole move's points
   (`PWR-09`); it applies after the Frenzy doubling.
5. `SCORE-05` Score is per level and only feeds trophies and best scores;
   it never decides a win.

## Score Frenzy (`FRZ-*`)

State: `frenzy_bar` 0..60, `frenzy_left` seconds.

1. `FRZ-01` Each cleared piece adds 1 to the bar [S2]. Unlocked at e1-l04;
   before that the bar is hidden.
2. `FRZ-02` When the bar reaches 60 [chosen], Frenzy starts at the end
   of that step and lasts the Frenzy tier's duration: 15 s base; 20, 25,
   30 s with upgrades [S1]. Points from later steps are doubled.
3. `FRZ-03` During Frenzy the bar does not fill; it empties to 0 when
   Frenzy ends. A new Frenzy cannot start during one; the gold totem's
   Frenzy (`TOT-12`) restarts the duration instead.
4. `FRZ-04` During Frenzy each gem collected also adds 1 star sand
   (`PROG-06`) [chosen].
5. `FRZ-05` Frenzy time counts down with the level timer and stops with it.

## Obstacles (`OBS-*`)

1. `OBS-01` A steel piece has a kind but cannot be swapped and is never
   part of a run [S1, S8].
2. `OBS-02` Each new piece from a refill is steel with probability
   `steel_rate` from the level data (0 by default), while fewer than 8
   steel pieces are on the board [chosen].
3. `OBS-03` Steel is removed only by effects: fire, TNT, lightning,
   color_clear from tier 2 (its kind only) and the ward totem. A removed
   steel piece scores like any piece and fills the Frenzy bar.
4. `OBS-04` Steel pieces fall like other pieces and carry no gem or
   power-up.
5. `OBS-05` A chained piece is placed by the level data
   (`chained_cells`). It cannot be swapped, falls normally and matches
   normally, but holds: any clear that reaches it (a run, a power-up, a
   totem) breaks its chain instead and leaves the piece in place. The
   piece scores nothing then, keeps its gem and power-up (which does not
   go off), and from then on plays as a plain piece, so the next clear
   takes it. A run made only of chained pieces clears nothing and earns
   no power-up (`PWR-01`); their chains break and, now plain, they match
   in the next step, which scores as a further step (`SCORE-01`). A run
   with some pieces held still counts at its full length for power-ups
   and totem charges [S1, chosen].
6. `OBS-06` Shuffles leave steel and chained pieces where they are.

## Shuffles and hints (`SHUF-*`)

1. `SHUF-01` The player may shuffle the board twice per level, free
   [S3].
2. `SHUF-02` When no legal move exists after a move, the board reshuffles
   by itself and the timer loses 5 s [S4, chosen amount].
3. `SHUF-03` A shuffle rearranges the movable pieces (with their gems and
   power-ups) so that no run shows and a legal move exists.
4. `SHUF-04` The player may buy a hint for 300 points [S3, chosen cost]:
   one legal move is highlighted until the next move. Not available when
   the score is below 300.
5. `SHUF-05` On e1-l01 and e1-l02 only, after 8 s without a move, one
   legal move is highlighted free (onboarding) [chosen].

## Order inside one moment

When several things happen in one step, they happen in this order:
clears (runs plus power-ups), gem collection, scoring, Frenzy bar, fall,
refill, steel, new gems, new power-ups. Totems fire after the last
cascade. The goal check comes last, then the automatic reshuffle.

## Edge cases

- A run of 3 that includes a power-up piece sets it off; a power-up's
  area that includes another power-up sets that one off too in the same
  step (`PWR-04`).
- A move can set off any number of steps; the core caps cascades at a
  safe bound and reports a stuck board instead of looping.
- If the board has no plain piece, gems and power-ups that cannot be
  placed wait for the next refill.
- The timer reaching 0 while a totem's or power-up's effect resolves
  does not end the level until the move finishes (`GOAL-02`).

## Feedback

Every event has a sound cue (see the audio contract) and a visual. Feel
targets:

- Close to top-down camera, so the board reads the way the 2D board did.
- A legal swap starts resolving at once; an illegal swap bounces back.
- Cascades chain quickly; each step gets a rising cue (`cascade_1` to
  `cascade_8`).
- A collected gem flies to the goal counter; a totem lights up when
  charged once and fires visibly on its own before the next totem.
- Aztec temple mood: carved stone, gold, jungle ambience.

Exact timings are WP-40's.

## Acceptance checks

| Rule | Given | When | Then |
|---|---|---|---|
| `MOVE-02` | Two adjacent pieces whose swap makes no run | they are swapped | the board is unchanged and no move is counted |
| `MOVE-03` | A steel piece next to a piece that would complete a run | they are swapped | the swap is refused |
| `GEM-03` | A gem on a piece in a run | the move resolves | `gems_collected` +1 and score +50 more |
| `GEM-04` | `gems_max` 5, a move collects 2 gems | the board refills | 5 gems are on the board, both new ones on new pieces |
| `GOAL-01` | `gem_goal` 10, 9 collected | a move collects 1 | the level is won after the move resolves |
| `GOAL-02` | 0.5 s left, a 2 s move that reaches the goal | time hits 0 mid-move | the level is won |
| `SHUF-02` | A board with no legal move after a move | the move resolves | the board reshuffles and `time_left` drops by 5 |
| `FRZ-02` | Bar at 58 | a move's step 0 clears 3 pieces, step 1 clears 3 | step 0 scores 30, step 1 scores 120 (doubled) and Frenzy is on |
| `OBS-01` | Steel red between two red pairs | — | no run is found |
| `DIFF-03` | Normal goal 25, no hard value in data | hard is played | goal is 40 |

## Not yet decided

None. Everything above is confirmed or chosen; playtests (WP-40) and the
balance simulator (WP-22) may tune the chosen numbers.
