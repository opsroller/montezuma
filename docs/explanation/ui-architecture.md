# UI architecture

Why the in-game UI is built the way it is. Facts are in the
[UI reference](../reference/ui.md).

## Scenes as data, behaviour in C++

The project allows no game logic in GDScript. UI behaviour (navigation,
binding, settings) is therefore C++ in `extension/src/ui/`, and each
screen's layout is a `.tscn` scene, which is data. Buttons say what they
do through properties on `NavButton` (`action`, `target`) rather than
code, so a designer can rearrange or add buttons without a build. Logic
that needs no engine (the back stack, settings ranges, clock and level-id
formatting) lives in `extension/src/ui/model/` with plain CTest tests.

## One navigation owner

Screens never open each other. They ask the `ScreenStack`, which keeps
the back stack, hides or freezes what is underneath, routes Back to the
top screen and restores focus when a screen is uncovered. This is the
usual screen-stack pattern: it keeps Back predictable and stops focus
from getting lost, which matters for controller and keyboard players.
Actions the stack cannot satisfy (play a level, quit) go up to `UiRoot`
as signals, so the UI requests and the game scene decides.

## An overlay, not the main scene

`UiRoot` is an autoload `CanvasLayer` drawn over whatever scene runs.
The board and the menus are built in parallel by different work packages;
as an overlay, the UI neither owns the main scene nor depends on it. The
title screen dims the live 3D board behind it, and the hand-off to play is
just closing every screen. The game scene listens to `play_requested`
and tells the UI about the end with `show_results`.

## Scaled to a reference size

Layouts are made for 1280×720 and scaled uniformly to the window's short
side, with the long side getting the extra room. Since 24 October 2026
that is Godot's own `canvas_items` stretch mode with the `expand` aspect
(`display/window/stretch` in `project.godot`); before, UiRoot did the same
scaling itself on the UI layer only so the 3D
view keeps rendering at the window's native resolution. On phones the
layout also sits inside the safe area.

## Theme from tokens

The graphic designer owns how the UI looks. Their tokens file (W3C Design
Tokens format) is turned into one Godot `Theme` at start-up, and scenes
refer only to theme names (`HeadingLabel`, `PrimaryButton`). Swapping the
tokens file restyles every screen without touching a scene, which is also
how a future mini game can carry its own look. Until the designer's
tokens exist the UI uses a placeholder set, checked for contrast (every
text pair at least 4.5:1).

## Accessibility defaults

Settings take effect immediately and persist (a Text size setting existed
until 23 October 2026 and is to return on Godot's scaling: work package 61), reduced
motion turns off transitions, every screen opens with focus on a sensible
control and a visible focus ring, and locked levels say "locked" in words
rather than by colour.
