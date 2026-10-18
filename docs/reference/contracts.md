# Contracts

The interfaces between work packages. They let packages run in parallel:
each side builds against this file, with stubs, before the other side
lands. Each section has an owner (see `docs/reference/work-packages.md`); to change
one, message the owner (coordinate skill), and the owner lands it as a
`feat(contracts):` commit.

Status: **proposed** until the package that implements it merges;
**agreed** when both sides have confirmed it before that.

Each contract gives the C++ shape (namespace `montezuma`) and, where Godot
sees it, the surface the binding exposes (`Dictionary`, `Array`,
`Vector2i`). The binding converts one to the other; both sides build
against both.

Coordinates: `x` runs left to right and `y` top to bottom, from 0; pieces
fall toward higher `y`. The 3D view maps `y` to depth or height, never
the reverse.

## MoveResult events (owner: gameplay-programmer, WP-01) — implemented

Implemented in `core/include/montezuma/board.hpp` (field-by-field
reference, ordering rules and the replay recipe:
[move-result.md](move-result.md)). Every legal swap, and every effect
through `Board::clear_cells`, reports cell-level events so the board view
animates without knowing the rules. The count fields the binding already
uses are unchanged.

```cpp
namespace montezuma {
struct Cell  { int x, y; };                      // x left to right, y top to bottom, from 0
struct Fall  { Cell from, to; int kind; };       // to.x == from.x, to.y > from.y
struct Spawn { Cell to; int kind; int drop; };   // starts at y = -drop, drop >= 1
struct Run   { std::vector<Cell> cells; int kind; bool horizontal() const; };

struct CascadeStep {
    std::vector<Cell>  cleared;                  // row-major, each cell once
    std::vector<Fall>  falls;                    // by column left to right, bottom to top
    std::vector<Spawn> spawns;                   // by column left to right, bottom to top
    std::vector<Run>   runs;                     // horizontal runs, then vertical; empty for clear_cells step 0
};

struct MoveResult {
    std::vector<std::map<int, int>> steps;       // per step: kind -> pieces cleared (== cascades[i].cleared)
    int  matched_kind = -1;                      // drives Power Totem streaks; -1 for clear_cells
    bool shuffled = false;
    bool stuck = false;                          // no playable board could be made; board unchanged
    std::optional<std::array<Cell, 2>> swap;     // as passed to try_swap; empty for clear_cells
    std::vector<CascadeStep> cascades;           // step 0 is the swap's own match (or the cleared cells)
    std::vector<std::pair<Cell, Cell>> shuffle_map;  // {from, to} for each moved piece, by `to` row-major
    bool redealt = false;                        // shuffle fell back to a fresh deal; shuffle_map empty
    int  score() const;                          // 10 x pieces x (step + 1)
    std::map<int, int> cleared_by_kind() const;
};

std::optional<MoveResult> Board::try_swap(Cell a, Cell b);                 // empty if illegal
std::optional<MoveResult> Board::clear_cells(const std::vector<Cell>&);    // empty if no valid cell
std::vector<Run> Board::find_runs() const;
}
```

Differences from the proposal, and why:

- `swap` is `std::optional`: `clear_cells` results have no swap, and an
  empty optional says so without a sentinel cell.
- `redealt` is new: when the pieces cannot be rearranged the shuffle deals
  a new board, which no `{from, to}` map can describe. The view reads
  `Board::cells()` for the new layout.
- `steps` and `stuck` stay for the binding and `LevelState`.

`Match3Board` (GDExtension) exposes the same as a `Dictionary` with keys
matching the field names, `Vector2i` cells and plain `Array`s, from
`try_swap(a, b)` and from `clear_cells(cells)` (no `swap` key). The key table
is in [move-result.md](move-result.md#for-the-godot-binding).

## LevelData (owner: gameplay-programmer, WP-02; content: game-designer) — proposed

JSON at `game/data/levels/e<episode>/l<NN>.json`, parsed by `montezuma::LevelData::parse(std::string_view)`;
the engine reads the file and passes the text in.

| Field | Type | Notes |
|---|---|---|
| `id` | string | `e1-l03` |
| `episode`, `index` | int | 1-based |
| `board` | `{width, height, kinds}` | defaults 8, 8, 7 |
| `gem_goal` | `{normal: int, hard: int}` | gems to collect; a level needs `gem_goal` or `quotas` |
| `quotas` | `{kind_name: int}` | optional; kind names below. With `gem_goal`, both must be met (`GOAL-06`) |
| `base_clock` | `{normal: float, hard: float}` | optional, seconds, default 270; the level starts at `base_clock + 30 × timer tier` (tiers 0 to 4, `GOAL-03`). Replaces `time_limit`, which is rejected |
| `totems_unlocked` | `[totem_id]` | available on this level; the tier comes from player progress (WP-08) |
| `powerups_unlocked` | `[power-up id]` | optional, as above |
| `steel_rate` | float | optional, 0 to 1, default 0 (`OBS-02`) |
| `blocked_cells` | `[[x, y]]` | optional |
| `chained_cells` | `[[x, y]]` | optional (`OBS-05`) |
| `rules` | string | optional, default `classic` |
| `theme` | string | optional, default `temple` |
| `seed` | int | for sim and tests |

Difficulties: `normal`, `hard`.

Kind names, in index order: `red`, `orange`, `yellow`, `green`, `blue`,
`purple`, `white`.

## LevelSession (owner: gameplay-programmer, WP-22) — implemented

`montezuma::LevelSession(LevelData, Difficulty, Upgrades, seed)` plays one
level by the full rules. Commands: `swap(a, b)` -> `optional<MoveReport>`,
`tick(dt)` -> `optional<TickReport>`, `shuffle()` -> `optional<ShuffleReport>`,
`buy_hint()` -> `optional<Swap>`; `legal_moves()` for bots and hints.
Reports list every step in display order. Fields and defaults:
[level-session.md](level-session.md).

## Game signals (owner: graphics-programmer, WP-10/WP-26) — implemented

`Match3Game` (C++ node in `extension/src/game/`, placed in `game/scenes/game/game.tscn`)
owns the board and level state and emits, for the HUD and audio:

```
level_started(level: LevelData, difficulty: String)
quota_changed(kind: int, remaining: int)
time_changed(seconds_left: float)
score_changed(score: int)
streak_changed(kind: int, length: int)
totem_charge_changed(totem_id: String, charge: float)   # 0..1
bonus_changed(bonus_id: String, count: int)
level_finished(outcome: int, score: int)                # LevelState.Outcome
```

Also emitted for the HUD (consumer: ui-developer, WP-18) — proposed:

```
gems_changed(collected: int, goal: int)                 # goal 0 hides the counter
totem_changed(totem_id: String, charge: float, uses_left: int)   # charge 0..1
frenzy_changed(bar: float, seconds_left: float)         # bar 0..1; seconds_left > 0 while Frenzy is on
shuffles_changed(left: int)
hint_cost_changed(cost: int, affordable: bool)
```

Commands the HUD calls on `Match3Game` (no arguments): `shuffle()` and
`buy_hint()`. The HUD finds the node through group `match3_game`, which
must be set in the scene (`node_added` fires before `_ready`).

**Implemented** in WP-28 (`extension/src/game/match3_game.*`), plus
`hint_shown(a: Vector2i, b: Vector2i)` for the view to highlight a bought
or free hint. `level_started` passes the LevelData as a `Dictionary`.
`streak_changed` and `bonus_changed` are not emitted: the rules have no
streak apart from totem charges and no held power-ups. Signal timing and
cues: [Match3Game](match3-game.md).

`BoardView3D` API: `show_board(board: Match3Board)`, `play(result: Dictionary)`
(emits `animation_finished` when done), `play_illegal(a, b)`, signals `swap_requested(a: Vector2i,
b: Vector2i)` and `animation_finished`.
**Implemented** in WP-12 (`extension/src/board/`), plus `is_busy()`,
`cell_at_screen(pos)` and a `layout_changed` signal; WP-29 adds the
optional effect keys in `play`'s result (`lead`, `repaints`, `fx` per
cascade, `marks`), `show_marks(marks)`, `show_hints(pairs, seconds)` and
`play_effect(label, cells)` (labels as the benchmark's `bench_event`:
`totem:<id>`, `powerup:<id>`); see [Board view](board-view.md#effects).

## Asset manifest (owner: technical-artist, WP-09-11) — proposed

`game/assets/manifest.json` maps logical names to scenes, per theme, so
mini games can swap the whole look without code changes:

```json
{
  "default_theme": "temple",
  "themes": { "temple": { "artifact_0": "res://assets/models/temple/artifacts/PF_Artifact_Ruby.tscn" } }
}
```

Each value is a `res://` path to a `PackedScene`. `BoardView3D` takes a
`theme` property (empty means `default_theme`), reads the manifest once and
falls back to a placeholder primitive for any name the theme lacks.
**Implemented** for `artifact_*`, `board`, `environment`, `cell_highlight`
and `fx_match` in `extension/src/board/piece_library.cpp`; see
[Board view](board-view.md#piecelibrary-c-only).

Names, theme-neutral: `artifact_0` … `artifact_6` (by kind index; origin at
the cell centre on the board plane), `board` (replaces the slab; tile tops
at y = 0), `environment` (backdrop plus its own `WorldEnvironment`, sun and
lights, so the scene supplies none), `cell_highlight` (shown at the
selected cell), `fx_match` (one-shot `CPUParticles3D` at each cleared cell,
freed after `lifetime`), `fx_cascade`, `totem_<id>`, `bonus_<id>` (the
badge on a piece carrying that power-up), and, read by the board view
since WP-29: `marker_gem`, `marker_steel`, `marker_chain`,
`fx_totem_<id>`, `fx_power_up_<id>`, `fx_gem_collect`, `fx_steel_break`,
`fx_chain_break`, `fx_power_up_placed`, `fx_hint`.
Scale, folders and rules: [Board themes](board-themes.md).

## Audio cues (owner: sound-designer, WP-15 to 17) — proposed

Played by name with `GameAudio::play(name)` (C++:
`GameAudio::get_singleton()`; scenes: the `/root/GameAudio` autoload); a
missing cue warns and is silent. Volume per bus with
`set_volume(bus, linear)`. Full API and when each cue plays:
[audio.md](audio.md#audio-system).

`swap`, `swap_illegal`, `match`, `cascade_1` … `cascade_8`, `piece_land`,
`gem_collect`, `steel_break`, `chain_break`, `bonus_earned` (a power-up
appears on the board), `bonus_use_<power-up id>`, `totem_charge`,
`totem_fire_<totem id>`, `quota_done`, `time_warning`, `level_won`,
`level_lost`, `ui_click`, `ui_back`, `trophy`, `music_menu`,
`music_play_<episode>`, `ambience_jungle`.

Ids are the settled totem and power-up ids below. Files, levels and
status per cue: [audio.md](audio.md).

## Totem and power-up ids (owner: game-designer, WP-03) — settled

Rules: [rules-totems.md](rules-totems.md), [rules-power-ups.md](rules-power-ups.md).

Totems, one per kind, in kind order: `fire` (red), `time` (orange),
`gold` (yellow), `harvest` (green), `ward` (blue), `gift` (purple),
`paint` (white).

Power-ups (the original's bonuses): `tnt`, `color_clear`, `lightning`,
`clock`, `multiplier`, `hint`.

Bonus games: `bg_accuracy`, `bg_memory`, `bg_reaction`, `bg_attention`.

Rule sets: `classic`. Themes: `temple`.

These replace the placeholders `row_clear`, `random_blast`, `color_bomb`
(totems) and `hammer`, `extra_time`, `shuffle`, `dynamite`, `freeze`
(bonuses). Asset and audio names follow: `totem_<id>`, `bonus_<id>`,
`totem_fire_<id>`, `bonus_use_<id>` use these ids.

The LevelData fields above were agreed with the LevelData owner and land with WP-05 (#7).

## Quality tiers (owner: graphics-programmer, WP-45) — implemented

`/root/QualityTiers` (C++: `QualityTiers::get_singleton()`). Settings
screen (ui-developer): the key `quality_tier`, values `auto` plus
`get_available_tiers()`, read with `get_choice()`, written with
`set_choice(value)`, which saves and applies at once; hide it when only
one tier is available (implemented: the Graphics quality row). Content (technical-artist): node groups
`quality:<feature id>`, `quality_fallback:<feature id>`,
`quality_shadow_light`, `quality_fog_light`; texture variants through
`resolve_variant(path)`; material variants are swapped by `QualityTiers`
on every mesh, so content names the base material. Benchmark (WP-46): `-- --quality=<tier>` and
`get_active_features()`. Ids, groups and settings:
[quality-tiers.md](quality-tiers.md).

## UI requests (owner: ui-developer, WP-19) — proposed

`UiRoot` (autoload at `/root/UiRoot`, `extension/src/ui/ui_root.*`) is the
menus' side of the hand-off to play. Details in `docs/reference/ui.md`.

```
# UiRoot emits, for the game scene:
play_requested(level_id: String, difficulty: String)   # "e1-l03", "normal" | "hard"
level_quit_requested()                                  # left from the pause menu
# The game scene calls:
show_results(outcome: int, score: int, level_id: String = "")   # LevelState.Outcome
open_pause()
```

The UI pauses the tree (`get_tree().paused`) while the pause menu is open;
board and game nodes keep the default pausable process mode. During play
ui_cancel and `ui_pause` (P, controller Start) open the pause menu. UI
sounds are played with `GameAudio.play("ui_click")` and `"ui_back"` when
the autoload exists. Volumes go to audio buses `Music` and `SFX`.

## Web demo hand-off (owner: gameplay-programmer, WP-21/WP-34; consumer: frontend-web-developer, WP-35) — agreed

Agreed by both sides on 2026-10-18. The site's side is described in
`docs/reference/website.md`.

**Files.** The Godot 4.7.2 single-threaded web export, named `montezuma`
(`montezuma.js`, `montezuma.wasm`, `montezuma.side.wasm`, `montezuma.pck`,
the audio worklet scripts) and the game's extension library
`libmontezuma.web.template_release.wasm32.nothreads.wasm`, all in one
folder: the `montezuma-<version>-web.zip` artifact of the `web` workflow
(see [Build pipeline](build-pipeline.md#web-demo)). The site does not use
the export's own HTML page.

**Loading.** On the visitor's click, the page loads `play/montezuma.js`
and starts it with Godot's `Engine` API:

```js
const lib = "libmontezuma.web.template_release.wasm32.nothreads.wasm";
const engine = new Engine({
  executable: "play/montezuma",
  canvas,                      // created by the page inside #stage; needs an id
  canvasResizePolicy: 0,       // the page sizes the canvas
  ensureCrossOriginIsolationHeaders: false,
  onProgress(current, total) { /* updates the <progress> bar */ },
});
await engine.preloadFile(`play/${lib}`, lib);  // the game's extension
await engine.startGame();
```

The extension must be preloaded under its bare file name: Godot opens it
by that name from the game's file system. Without it the game starts
with no game classes and logs `Can't open dynamic library`. (Godot's
`gdextensionLibs` setting does not work from a subfolder: it fetches the
file relative to the page and opens it by the same path.) The canvas
needs an `id`; Godot selects it with `#<id>`. Amended in WP-21
(2026-10-18) after running the export in a browser.

**Events, game to page.** Dispatched on `window` as `CustomEvent`s (from
`JavaScriptBridge`). Unknown events and extra fields are ignored.

| Event | `detail` | When |
|---|---|---|
| `montezuma:level-completed` | `{ v: 1, level: "<level id>", score: <int> }` | Once, when the intro level is won |
| `montezuma:ready` (optional) | `{ v: 1, version: "<game version>" }` | First frame shown |

**Events, page to game.** None.
