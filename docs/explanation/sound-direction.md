# Sound direction

*Explanation, for anyone making or changing the game's sounds: why
Montezuma's sounds the way it does. The facts are in the
[audio reference](../reference/audio.md); the steps are in
[how to regenerate or add sounds](../how-to/regenerate-or-add-sounds.md).*

## One temple, one palette

Every musical sound, the score cues and the music alike, is played by
a small set of imagined Mesoamerican instruments: struck jade and stone
bars, a two-tone log drum, a hide drum, seed-pod rattles, a clay flute
and a conch horn. Sharing instruments makes effects and music sound like
they come from the same place, the way the board's stone and gold do on
screen. Sounds of objects touching are not instruments; they are
modelled on the objects themselves (see
[Physical models for contact sounds](#physical-models-for-contact-sounds)).
The palette is an evocation, not a reconstruction: none of it imitates
a recording, a specific historical instrument sample or any other game.

## Sound tells you what happened

A match-3 board moves faster than the eye follows, so the ear carries
the score:

- A match is a single clean chime in D minor pentatonic. Every cascade
  step plays the next note up the same scale and adds sparkle, so a long
  chain is heard climbing even when you are not watching it.
- Frequent sounds (swap, landing, menu ticks) are short, nearly dry and quiet,
  with several variations so the hundredth one does not grate.
- Warnings use the log drum, which nothing else in a level plays alone,
  so the time warning cuts through music at a lower level than a louder
  sound would need.
- Big moments (totem fire, level won, trophy) open with the conch, so
  the horn always means "something important happened".

## Music that leaves room

The music is played by the same instruments: clay flute melody over a
log-drum ostinato, a wooden bass, a conch drone, and from episode 1 on
a hide drum and rattles. Each episode raises the tempo and adds layers,
so later episodes feel more urgent without a different style. Melodies
sit around the flute's middle register and the music is mixed well
under the effects, so the chimes that carry the score always cut
through. Each loop has two tunes over 32 bars, long enough that a level
rarely hears the same phrase twice.

The tunes are composed by a seeded random walk over each piece's scale
with fixed phrase rules (a motif, its answer, a contrast, a return home),
not by imitating any existing music.

## Code names events, the table decides sounds

Game code calls a cue by what happened (`match`, `level_won`), never a
file. Levels, variations, randomisation, voice limits and priority live
in one data file, so tuning a sound never touches C++, and code can call
a cue before its sound exists: an unknown cue warns once and stays
silent. A fixed pool of voices with priority stealing means a long
cascade can never starve a time warning, and important cues duck the
music by a few dB so they read without being loud.

## Generated, not recorded

The sounds are synthesised by Python scripts committed in `tools/audio/`.
That keeps every file traceable to code we wrote (our provenance rule),
lets anyone rebuild or tweak them with free tools, and keeps the source
GPL like the rest of the game. Synthesis does not have to mean a
stylised, toy-like sound. The game's
[visual target](../reference/visual-target.md) is real-looking stone,
gold and metal, so the sounds of things touching each other are built
from models of how those materials vibrate (below). The musical cues,
the chimes, drums and horns that carry the score, stay instruments on
purpose: they are signals, not objects in the scene.

Nothing from *The Treasures of Montezuma*'s audio is used, sampled or
imitated. Its sound cues informed only which events need a sound.

## Physical models for contact sounds

A stone piece sliding, landing or bumping, a steel band smashed, a
chain snapping and a charge blowing the board apart should sound like those objects, because the board shows
them as real objects. A few fixed sine tones read as "a sound effect";
a real knock on stone has dozens of partials that each ring and fade at
their own rate. So these cues are built the way the sound is made in
the world:

- **Modal impacts.** An object rings at a set of resonant modes whose
  spacing, decay and brightness depend on its material: stone modes are
  inharmonic and die fast, metal modes are denser and ring long. A
  strike is a short contact force through those modes; a softer, longer
  contact sounds duller, a hard short one brighter. Brittle fracture
  adds a burst of crack noise.
- **Stick-slip friction.** Sliding is a stream of tiny catches and
  releases whose rate follows the speed, so a swap speeds up and slows
  down the way a hand pushes a piece, instead of a fixed noise sweep.
- **Debris.** Fragments of random size fall and bounce to rest; small
  pieces ring higher and bounce faster, which is what makes a smash
  sound like it breaks into parts.
- **A stone hall.** Every contact sound goes through the same kind of
  room: early reflections off near walls, then a tail whose highs die
  before its lows. The hall is sized to each event, small and short for
  a landing, large and long for the blast, so all of them sound like
  the same temple.

Each design is also cut to its length budget, and the sharpest ones
(the illegal swap, the steel break and the blast) have their highest
transient peaks rounded off, as a recording chain would, so the realism
costs nothing in loudness or voice time. Because the models are seeded, each variation
is a different strike on a slightly different object, and the files
still rebuild exactly from the script.
