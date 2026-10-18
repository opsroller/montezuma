# Check UI text and layout

How to check a screen for hard-coded text, clipping, large text, odd
window sizes and controller use before opening a PR. Build the extension
first ([Build and test](build-and-test.md)).

## Hard-coded or clipped text

```sh
godot --path game -- --pseudolocalize
```

Every string shows accented and in brackets. Unaccented text is
hard-coded; a missing closing bracket is clipped text.

## Largest text

Open Settings and set **Text size** to the maximum (200%). Visit every
screen: nothing may overlap or leave the window; long lists scroll.

## Window sizes and safe areas

```sh
godot --path game --resolution 1280x720
godot --path game --resolution 1170x1080
godot --path game --resolution 2400x1080
```

The UI scales from 1280×720 to fit the short side. On phones it also
stays inside the reported safe area (notches, rounded corners).

## Keyboard and controller only

Without touching the mouse, go through every screen with the arrow keys
or D-pad, Enter or A, Escape or B. Focus must always be visible, start on
the primary action, never get stuck, and come back to the opening button
after Back.

## Other locks

- `--unlock-all` opens every level on the map.
- Delete `user://settings.cfg` to start from default settings (on Linux:
  `~/.local/share/godot/app_userdata/Montezuma's/settings.cfg`).
