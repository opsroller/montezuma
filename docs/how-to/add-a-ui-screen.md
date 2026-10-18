# Add a UI screen

How to add a menu screen or modal and open it from a button. Names and
APIs are in the [UI reference](../reference/ui.md).

1. **Add the strings.** Append keys to `game/ui/strings.csv`, one per
   visible text (`shop.title,Shop`). Add translator context to the
   strings table in `docs/reference/ui.md`.
2. **Make the scene.** Create `game/ui/screens/<id>/<id>.tscn` whose root
   is a `Screen` (or a `Screen` subclass when it binds data). Copy an
   existing screen for the backdrop, centred panel and column. Set
   `screen_id`, `default_focus`, and `modal` / `pauses_game` /
   `back_behavior` as needed. Give text controls the key as `text`, and
   styles through theme type variations (`HeadingLabel`, `PrimaryButton`),
   never colours or sizes in the scene.
3. **Add buttons.** Use `NavButton` with `action` and `target` (for
   example `push` + `shop`). Mark nodes your code reads as unique names
   (`%Name`).
4. **Bind data, if any.** Add a `Screen` subclass in
   `extension/src/ui/screens/<id>_screen.{hpp,cpp}`, override `on_open`
   (call `Screen::on_open` first if you need `get_params()`), and register
   it in `extension/src/ui/register_ui.cpp`. Read game state through
   signals or queries from the contracts; never decide rules in the screen.
5. **Register the id.** Add `"<id>": "res://ui/screens/<id>/<id>.tscn"` to
   `screens` on `ScreenStack` in `game/ui/ui_root.tscn`.
6. **Open it.** Point a NavButton at it, or call
   `UiRoot.get_stack().push("<id>", params)`.
7. **Check it** with [Check UI text and layout](check-ui-text-and-layout.md)
   and update the screens table in the reference.
