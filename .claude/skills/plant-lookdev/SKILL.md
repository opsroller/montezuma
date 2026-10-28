---
name: plant-lookdev
description: Build, render and judge the jungle plant library (tools/assets/plants.py) at 3840x2160. Use when changing a plant species, a leaf atlas, a band's colours or the shedding rules, or when asked how a plant or a band currently looks. Gives the command order, the traps that cost hours, and how to read the measurements.
---

# Plant look-dev loop

The library is judged by eye on rendered frames, never from the code.

These are plain commands on purpose: this machine bans run/build wrapper scripts
(CLAUDE.md, "Working on this machine" — `flock` is the only wrapper), so this page
is the recipe, not a script that hides it. Type them; don't wrap them.

## The order, and why it is this order

```sh
# 1. parameters.  MUST precede the generator: it writes themes/plants.json.
python3 tools/assets/plants.py textures

# 2. atlases.  ~3 min for 36 sets at 4096 (parallel); prints "up to date" and skips
#    when nothing changed, so it is cheap to leave in the loop.
.venv/bin/python tools/assets/gen_theme_textures.py tools/assets/themes/plants.json

# 3. meshes, materials, and the plant section of game/assets/library.json
.venv/bin/python tools/assets/plants.py build [--only heliconia,tree_fern]

# 4. pages.  closeups is the one to judge on; --empty writes each page again with the
#    plant removed, which is what the measurement needs for an exact foliage mask.
python3 tools/assets/plants.py closeups --empty [--only ...]
python3 tools/assets/plants.py lookdev

# 5. import.  REQUIRED after any mesh or texture change.
flock /home/claude/work/montezuma/.godot-gpu.lock godot --path game --import --headless

# 6. capture at the quality floor, 4 frames is plenty for a static page
flock /home/claude/work/montezuma/.godot-gpu.lock \
  gamescope --backend headless -W 3840 -H 2160 -- \
  godot --path game --rendering-driver vulkan \
  res://assets/lookdev/LD_Plant_HeliconiaClump.tscn \
  --write-movie /tmp/plantshots/h/f.png --fixed-fps 30 --quit-after 4 -- --start-screen=

# 7. look at it, as a sheet when there is more than one
.venv/bin/python tools/assets/contact_sheet.py /tmp/plantshots /tmp/plantshots/sheet.png

# 8. read the reference's figures back off the frame
.venv/bin/python tools/assets/measure_foliage.py /tmp/plantshots/h/f00000003.png \
  --against /tmp/plantshots/h_empty/f00000003.png
```

## Traps that cost hours

- **Render after import, not before.** Godot serves meshes from `.godot/imported`, so
  a rebuilt OBJ does not reach a frame until an import pass. Several rounds of "the fix
  did nothing" were this, including one where a whole 66-page batch rendered the
  previous meshes.
- **A plant rendering white or untextured**: check its `.import` for `valid=false`
  before anything else. Godot writes that when an import fails — here, when a run read
  a PNG still being written — and then skips the file on every later pass, silently and
  permanently. The fan palm was pure white for hours. `validate_assets.py` rejects it
  now.
- **Judge on a close-up, not a band row.** A row is as wide as the sum of its spreads,
  so the understory row's camera stands 55 m back and a 1 m blade lands on 40 pixels.
  Band rows are for comparing species against each other, nothing else.
- **Read the set as contact sheets.** Nearly every fault in this library was found by
  seeing nine species at once — a white palm, a grass tuft in 124 pieces of litter, moss
  the colour of butter, a leaf floating 7 m above its plant — and none of them by
  looking at one specimen at a time. Render the whole set before concluding anything.
- **Anything sized from `Species.height`/`spread` is sized from the species, not the
  variant.** The range covers every variant: monstera's runs to 12 m because one of
  three climbs. Measure the built geometry instead.
- **Do not poll a background job.** Launch it with `run_in_background` and wait for the
  completion notification. Hand-rolled `until ... sleep` waiters mostly burn their
  30-minute limit doing nothing, and one of them made me render stale textures because
  its condition was wrong.
- Every Godot run goes through `flock /home/claude/work/montezuma/.godot-gpu.lock`; the
  GPU is shared with other sessions.

## Reading the measurements

`measure_foliage.py` prints each figure against
`docs/reference/foliage-reference.md` and marks FAIL. Treat a FAIL as a question, not
a verdict, then look at the frame — the numbers cannot see folded cardboard. Two are
known to be about the page rather than the plant: `contact_shadow` depends on the
environment's ambient and sky fill, and `dead_area` counts the litter skirt the
close-up page deliberately frames.

## Where the time goes

36 atlases at 4096 is the only slow step, and it is parallel across sets with the PNG
encoder at compress_level 1 — about 3 minutes, from forty. Mesh building is seconds.
A 4K capture is ~15 s of which most is Godot starting up, so rendering the whole
66-page set is ~25 minutes and belongs in the background.
