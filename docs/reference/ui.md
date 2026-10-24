# UI reference

Facts about the in-game UI: classes, screens, actions, settings, theme
tokens and strings. Owner: ui-developer (WP-18, WP-19). Code in
`extension/src/ui/`, scenes and data in `game/ui/`. Why it is built this
way: [UI architecture](../explanation/ui-architecture.md).

## Scene tree

`game/ui/ui_root.tscn` is the autoload `UiRoot` (`/root/UiRoot`), a
`CanvasLayer` (layer 10) drawn over whatever main scene runs.

```
UiRoot (UiRoot)
└─ Scaled (Control)                 fills the window; Godot's stretch settings scale it
   └─ SafeArea (MarginContainer)    inset by the phone's safe area
      ├─ Hud (Hud)                  game/ui/hud/hud.tscn; shown during a level
      └─ ScreenStack (ScreenStack)  the open screens, bottom to top
```

While the stack is empty the game underneath has the screen and the input.
The HUD sits below the stack, so pause and results draw over it.

## Classes (GDExtension, `montezuma::godot_binding`)

| Class | Base | Role |
|---|---|---|
| `UiRoot` | `CanvasLayer` | Scale and safe area, theme, settings, routes game actions |
| `ScreenStack` | `Control` | The only navigation owner: open, close, back, focus restore |
| `Screen` | `Control` | Root of every screen scene |
| `NavButton` | `Button` | A button whose action is data set in the scene |
| `UiTheme` | `RefCounted` | Builds the Godot `Theme` from a tokens file |
| `Hud` | `Control` | The in-level HUD, a read-only view of the game signals |
| `SettingsScreen`, `CreditsScreen`, `EpisodeMapScreen`, `ResultsScreen`, `TrophiesScreen`, `IntroScreen` | `Screen` | Screens that bind data |

### UiRoot

| Member | Kind | Notes |
|---|---|---|
| `play_requested(level_id: String, difficulty: String)` | signal | Menus closed; the game scene should load the level. `difficulty` is `normal` or `hard` (LevelData `gem_goal` and `base_clock` keys) |
| `level_quit_requested()` | signal | The player left the level from the pause menu |
| `settings_changed(key: String)` | signal | After a setting is applied |
| `show_results(outcome: int, score: int, level_id := "")` | method | Opens results. `outcome` is `LevelState::Outcome` (1 won, 2 lost); empty `level_id` means the level last played |
| `open_pause()` | method | Opens pause when no menu is open |
| `get_setting(key)`, `set_setting(key, value)`, `save_settings()` | methods | See Settings |
| `perform(action, target, params := {})` | method | Same routing as a NavButton |
| `get_stack()`, `get_tokens()`, `get_current_level()` | methods | |
| `reference_size` | property | Default `Vector2(1280, 720)`; kept for scenes that set it, but nothing reads it since the UI fills the window and Godot's stretch settings scale it |
| `tokens_path` | property | Default `res://ui/theme/placeholder.tokens.json` |
| `settings_path` | property | Default `user://settings.cfg` |

### ScreenStack

| Member | Notes |
|---|---|
| `screens: Dictionary` | Screen id → scene path. Set in `ui_root.tscn` |
| `initial_screen: String` | Opened on ready (`title`) |
| `transition_seconds: float` | Fade-in; 0 with reduced motion. Set by UiRoot from `motion.duration.normal` |
| `push(id, params)`, `replace(id, params)`, `reset_to(id, params)`, `back()`, `clear()` | Navigation. `back()` never closes the root screen, except a lone modal (pause over play) |
| `perform(action, target, params)` | `push`, `replace`, `reset`, `back` here; anything else is emitted |
| `top_id()`, `depth()`, `is_empty()` | |
| `screen_opened(id)`, `screen_closed(id)`, `emptied()`, `action_requested(action, target, params)` | Signals |

ui_cancel goes to the top screen's back behaviour. Covering a screen
remembers the focused control; uncovering restores it.

### Screen properties

| Property | Default | Notes |
|---|---|---|
| `screen_id` | stack id | |
| `default_focus` | empty | NodePath of the control focused on open; else the first focusable one |
| `modal` | false | Keeps the screen below visible but inert instead of hiding it |
| `pauses_game` | false | Pauses the tree while open |
| `back_behavior` | Pop | Pop, Blocked, or Action (`back_action` + `back_target`) |

### NavButton

Properties `action`, `target`, `params`, `hidden_on` (OS feature tags,
e.g. `web`, `ios`, `android`). Minimum height 48 reference pixels. Plays
`ui_click` (or `ui_back` for `back`) through the `GameAudio` autoload when
it exists.

| Action | Target | Handled by | Effect |
|---|---|---|---|
| `push` | screen id | ScreenStack | Open on top |
| `replace` | screen id | ScreenStack | Replace the top screen |
| `reset` | screen id | ScreenStack | Close all, open this one |
| `back` | — | ScreenStack | Close the top screen |
| `play` | level id | UiRoot | Close menus, emit `play_requested` |
| `restart` | — | UiRoot | `play` the current level again |
| `resume` | — | UiRoot | Close pause |
| `quit_level` | screen id (default `episode_map`) | UiRoot | Emit `level_quit_requested`, reset to the screen |
| `quit` | — | UiRoot | Quit the app through `GameAudio.quit()`, which stops every sound first (button hidden on web and mobile) |
| `url` | `https://` URL | UiRoot | Open in the browser |

### Hud

`game/ui/hud/hud.tscn`, instanced in `ui_root.tscn`. Hidden until
`level_started`; hidden again on `level_finished` (which opens results)
and on `quit_level`. It ignores the mouse except on its buttons, so taps
reach the board.

It binds to the first node in group `game_group` (also any that enters
the tree later; set the group in the scene, since `node_added` fires
before `_ready`) and connects each of these
[Game signals](contracts.md#game-signals-owner-graphics-programmer-wp-10wp-26--implemented)
that node has. A missing signal is skipped without an error:

| Game signal | HUD shows |
|---|---|
| `level_started(level, difficulty)` | Resets; reads `time_limit[difficulty]`, `gem_goal[difficulty]`, `quotas`, `totems_unlocked` from the LevelData |
| `time_changed(seconds_left)` | `%Clock` (m:ss) and `%TimeBar`; `DangerLabel` at or under `warning_seconds` |
| `score_changed(score)` | `%Score` |
| `gems_changed(collected, goal)` | `%Gems`; hidden while the goal is 0 |
| `quota_changed(kind, remaining)` | One row per kind in `%Quotas`: colour swatch, kind name, count or `hud.quota.done`; `%QuotaPanel` only on levels with quotas |
| `totem_changed(id, charge, uses_left)`, `totem_charge_changed(id, charge)` | A meter per totem in `%Totems`, in `totem_ids` order, with `hud.totem.ready` or `hud.totem.uses` in words |
| `frenzy_changed(bar, seconds_left)` | `%FrenzyBar` and `%Frenzy` (`hud.frenzy.on` while on) |
| `shuffles_changed(left)` | `%Shuffle` text; disabled at 0 |
| `hint_cost_changed(cost, affordable)` | `%Hint` text; disabled when not affordable |
| `level_finished(outcome, score)` | Hides; calls `UiRoot.show_results` |

Commands back to the game: `%Shuffle` calls `shuffle()`, `%Hint` calls
`buy_hint()` on the bound node, if it has them. `%Pause` is a NavButton
with action `pause`.

| Property | Default | Notes |
|---|---|---|
| `game_group` | `match3_game` | Group of the node to bind to |
| `warning_seconds` | 10 | Low-time threshold for the clock |
| `kind_names` | `red` … `white` | Index order of piece kinds (contracts) |
| `totem_ids` | `fire`, `time`, `gold`, `harvest`, `ward`, `gift`, `paint` | Meter order |

Quota swatches use token `color.kind.<name>`, with placeholder colours
until the tokens define them. The display state lives in the Godot-free
`ui::HudModel` (`extension/src/ui/model/hud_model.*`), covered by
`ui_model_tests`.

## Screens

| Id | Scene (`game/ui/screens/`) | Class | Params | Opened from | Back |
|---|---|---|---|---|---|
| `title` | `title/title.tscn` | Screen | — | start | blocked |
| `intro` | `intro/intro.tscn` | IntroScreen | — | title Play | pop; Next/Skip replace with `episode_map` |
| `episode_map` | `episode_map/episode_map.tscn` | EpisodeMapScreen | `episode` (1) | intro, results, pause | reset to `title` |
| `pause` | `pause/pause.tscn` | Screen (modal, pauses) | — | ui_cancel or `ui_pause` in play | resume play |
| `results` | `results/results.tscn` | ResultsScreen | `outcome`, `score`, `level_id` | `UiRoot.show_results` | reset to `episode_map` |
| `trophies` | `trophies/trophies.tscn` | TrophiesScreen | — | title | pop |
| `settings` | `settings/settings.tscn` | SettingsScreen | — | title, pause | pop (saves) |
| `credits` | `credits/credits.tscn` | CreditsScreen | — | title | pop |

Data sources and stubs:

- **Level map:** level count per episode from `res://data/levels/e<N>/l*.json`
  (10 placeholders while none exist). Unlocks are a stub until progress
  (WP-08): only `e1-l01`, or all with `--unlock-all`.
- **Results:** Next level is `e<N>-l<M+1>` when that file exists (any
  next index while the episode has no files); hidden when lost.
- **Trophies:** ids `t01`…`t09` in the scene, all shown as not earned
  until WP-08 supplies earned ids.
- **Credits:** reads `res://CREDITS.md` (headings, bullets, `https` links,
  bold), the root `CREDITS.md` copied in by the CMake build and packed
  into every export; shows `credits.fallback` if the file is missing. The
  export also packs `res://THIRD-PARTY-NOTICES.txt`, `res://LICENSE.txt`
  and `res://SOURCE.txt` (see the build pipeline reference).
- **Intro:** pages `intro.1`…`intro.3` are placeholders for the game designer.

## Input

| Action | Default | Where |
|---|---|---|
| `ui_accept`, `ui_up/down/left/right`, `ui_focus_next/prev` | Godot built-ins (keys, D-pad, A) | focus navigation |
| `ui_cancel` | Escape, controller B | back; opens pause during play |
| `ui_pause` | P, controller Start | added at runtime by UiRoot if missing; toggles pause |

Pointer and touch press buttons directly; screens stop pointer input from
reaching the board.

## Settings

Stored in `user://settings.cfg`, section `[settings]`. Applied live; the
settings screen saves on close.

| Key | Type | Default | Applied to |
|---|---|---|---|
| `music_volume` | float 0..1 | 0.8 | audio bus `Music` (dB) |
| `sfx_volume` | float 0..1 | 0.8 | audio bus `SFX` (dB) |
| `fullscreen` | bool | false | window mode (desktop only) |
| `locale` | String | `""` (system) | `TranslationServer` |
| `reduced_motion` | bool | false | screen transitions off; board animations at `feel::kReducedMotionAnimationSpeed` (2×) through `BoardView3D.animation_speed` |
| `difficulty` | `normal` / `hard` | `normal` | passed with `play_requested` |

The settings screen has no graphics quality row: the game runs on
Godot's project settings ([About quality settings](../explanation/about-quality-settings.md)).
There is no text scale either: the UI fills the window at the tokens'
font sizes, and Godot's stretch settings (`display/window/stretch` in
`project.godot`) decide any scaling. A `text_scale` key left in an older
`settings.cfg` is kept but has no effect.

## Theme tokens

`game/ui/theme/placeholder.tokens.json`, W3C Design Tokens (DTCG) format.
Colors as hex strings or DTCG color objects; dimensions as numbers, `"16px"`
or `{value, unit}`; aliases `"{group.token}"`. A missing token falls back
to the placeholder value.

| Token | Used for |
|---|---|
| `color.surface` | screen panels |
| `color.surface-raised` | buttons, cards, slider tracks |
| `color.text`, `color.text-muted` | text, captions, disabled text |
| `color.text-on-accent` | text on accent fills |
| `color.accent`, `color.accent-pressed` | primary buttons, titles, fills, pressed |
| `color.focus` | focus ring |
| `color.danger` | `DangerLabel` |
| `color.scrim` | backdrop behind screens and modals |
| `font.size.caption/body/heading/title` | type scale |
| `font.family.body/display` | `res://` font paths (empty: Godot default) |
| `space.1`…`space.6` | gaps, padding, screen margins |
| `radius.control` | corners (panels ×2) |
| `border.focus-width` | focus ring (min 2) |
| `motion.duration.normal` | screen fade |

Theme type variations: `TitleLabel`, `HeadingLabel`, `CaptionLabel`,
`DangerLabel`, `PrimaryButton`, `CardPanel`, `ScrimPanel`, `ScreenMargin`.

## Strings

`game/ui/strings.csv` (Godot CSV translation, `keys,en`). Controls hold
the key as their text and Godot translates it. Placeholders are named
(`{n}`, `{score}`, `{page}`, `{count}`) and filled with `String.format`.

Translator context:

| Key | Context |
|---|---|
| `title.name` | Product name; usually not translated |
| `title.inspired` | Credit naming the original game and its publisher; keep both names |
| `menu.quit` | Closes the game; hidden on phones and the web |
| `intro.1`…`intro.3`, `intro.title` | Placeholder story until the game designer writes the intro |
| `intro.page` | Page counter: `{page}` current, `{count}` total |
| `map.level.open` | Level button; `{n}` level number |
| `map.level.locked` | Locked level; `{n}` level number; must say "locked" in words |
| `episode.N.name` | Placeholder episode names |
| `pause.quit_level` | Abandons the level, back to the level map |
| `results.lost` | Results title when time ran out |
| `results.score` | `{score}` is the final score, a whole number |
| `trophy.tNN.name/desc` | Placeholders until the trophies package defines them |
| `settings.reduced_motion` | Turns off transitions and other non-essential motion |
| `hud.score`, `hud.gems` | `{score}`; `{collected}` of `{goal}` gems |
| `hud.totem.uses` | `{uses}` is how many more times the totem can fire |
| `hud.frenzy.on` | Score Frenzy is active; `{seconds}` whole seconds left |
| `hud.shuffle`, `hud.hint` | Button labels; `{left}` shuffles left, `{cost}` points per hint |
| `settings.difficulty.normal/hard` | Match the level files' `normal` / `hard` keys (`DIFF-*`) |
| `credits.fallback` | Shown when the full credits file is not packaged; names stay untranslated |

Known gap (checked in Godot 4.4): Godot CSV translations have no plural forms and no locale number
grouping; no current string needs either. Plural strings will move to
gettext `.po` when one is added.

## Command-line arguments

Pass after `--`: `godot --path game -- --pseudolocalize`.

| Argument | Effect |
|---|---|
| `--pseudolocalize` | Godot pseudo-localization: accents, brackets, padding |
| `--unlock-all` | Every level open on the map (until WP-08) |
| `--start-screen=<id>` | Opens on screen `<id>` instead of the title; empty (`--start-screen=`) opens none |
| `--start-level=<id>` | Skips the menus and plays level `<id>`, as if picked on the map. CI screenshots use `e1-l01` |
| `--quit-after-frames=<n>` | Quits after `<n>` frames through `GameAudio.quit()` (see [Audio](audio.md#quitting)). Use it instead of Godot's `--quit-after` in any run that plays sound and must end without errors |
