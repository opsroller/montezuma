# Gameplay Rules Spec

This is the reference for how the game plays. It describes the rules of the
original *The Treasures of Montezuma* (Alawar, 2009 iOS release) so the 3D
remake plays the same way. Nothing here comes from the original's files.
Every asset in this repository is new.

Items marked **[verify]** come from store listings or memory of the
original and still need checking against a playthrough.

## Feel targets

The 3D version keeps the original's feel:

- Close to top-down camera, so the board reads the way the 2D board did.
- Snappy swaps. A legal swap resolves at once, and an illegal swap bounces
  back.
- Cascades chain quickly, and each link in the chain gets a rising sound cue.
- Aztec temple mood: carved stone, gold, jungle ambience.

## Board

- 8×8 grid **[verify]**. Each cell holds one artifact or is empty while
  pieces fall.
- 7 artifact kinds **[verify]**: red, orange, yellow, green, blue, purple,
  white.
- A new board never starts with a match already on it, and always has at
  least one legal move.

## Moves

- The player swaps two orthogonally adjacent pieces.
- The swap is legal only if it creates at least one line of 3 or more of the
  same kind. Otherwise it is undone.

## Resolving a move

1. Find every horizontal and vertical run of 3 or more of the same kind.
2. Clear all matched cells at once. A cell that belongs to two runs counts
   once.
3. Pieces above fall to fill gaps, and new random pieces drop in from the
   top.
4. Repeat from step 1 until nothing matches. Each repeat is one **cascade**
   step.
5. If no legal move remains, shuffle the board until one exists and no match
   is showing.

## Scoring **[verify]**

- Each cleared piece scores 10 × cascade step (1 for the swap's own match,
  2 for the first cascade, and so on).
- A match of 4 or more grants a bonus pickup.

## Level goal

- Each level sets a quota per artifact kind (for example 10 red, 8 blue) and
  a time limit.
- Clearing a piece of a kind counts toward that kind's quota.
- The level is won when every quota is met before the timer runs out. If the
  timer runs out first, the level is lost.
- Two difficulty modes: Relaxed (more time) and Normal **[verify]**.

## Power Totems **[verify]**

- Six totems, unlocked as the episodes go on.
- A totem charges when the player makes **consecutive matches of the same
  artifact kind**. A match of a different kind resets the streak.
- When the streak reaches the totem's threshold, the totem fires a board
  effect, such as clearing a row, destroying random pieces, or adding time.

## Bonuses **[verify]**

Six to eight bonuses, earned from big matches and kept until used, such as a
hammer (clear one piece), lightning (clear a line), and extra time. They can
be upgraded between levels.

## Structure

- 5 episodes and 41 to 53 levels (the store listings disagree) **[verify]**.
- 4 bonus mini-games with 8 bonus levels.
- 9 trophies for achievements such as long cascades or fast clears.

## Implementation notes

Board logic lives in `core/` as plain GDScript with no scene or rendering
dependencies, so it can run headless in tests. The 3D layer watches the board
and animates its changes.
