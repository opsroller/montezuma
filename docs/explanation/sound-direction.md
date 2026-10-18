# Sound direction

Why Montezuma's sounds the way it does.

## One temple, one palette

Every sound is played by a small set of imagined Mesoamerican
instruments: struck jade and stone bars, a two-tone log drum, a hide
drum, seed-pod rattles, a clay flute and a conch horn. Sharing
instruments makes effects and music sound like they come from the same
place, the way the board's stone and gold do on screen. The palette is
an evocation, not a reconstruction: none of it imitates a recording, a
specific historical instrument sample or any other game.

## Sound tells you what happened

A match-3 board moves faster than the eye follows, so the ear carries
the score:

- A match is a single clean chime in D minor pentatonic. Every cascade
  step plays the next note up the same scale and adds sparkle, so a long
  chain is heard climbing even when you are not watching it.
- Frequent sounds (swap, landing, menu ticks) are short, dry and quiet,
  with several variations so the hundredth one does not grate.
- Warnings use the log drum, which nothing else in a level plays alone,
  so the time warning cuts through music at a lower level than a louder
  sound would need.
- Big moments (totem fire, level won, trophy) open with the conch, so
  the horn always means "something important happened".

## Generated, not recorded

The sounds are synthesised by Python scripts committed in `tools/audio/`.
That keeps every file traceable to code we wrote (our provenance rule),
lets anyone rebuild or tweak them with free tools, and keeps the source
GPL like the rest of the game. The cost is a slightly stylised sound;
that suits a game whose look is also stylised rather than photographic.

Nothing from *The Treasures of Montezuma*'s audio is used, sampled or
imitated. Its sound cues informed only which events need a sound.
