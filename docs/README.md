# Documentation

Docs follow [Diataxis](https://diataxis.fr): each page is one kind, in its
own folder.

| Kind | Folder | Read it when |
|---|---|---|
| Tutorials | [`tutorials/`](tutorials/) | You are new and want to learn by doing |
| How-to guides | [`how-to/`](how-to/) | You have a task to get done |
| Reference | [`reference/`](reference/) | You need facts to look up |
| Explanation | [`explanation/`](explanation/) | You want to understand why |

## Reference

- [Project profile](reference/project-profile.md): stack, commands, who owns what, project rules.
- [Contracts](reference/contracts.md): interfaces between work packages.
- [Work packages](reference/work-packages.md): the plan and its status.
- [Build pipeline](reference/build-pipeline.md): desktop build jobs, artifacts, versions and pinned tools.
- [Audio](reference/audio.md): sound formats, levels and the files behind each cue.
- [Power Totems](reference/totems.md): totem ids, charging, effects and tuning.
- [Power-ups](reference/bonuses.md): on-board power-up ids, tiers, placement, triggering and effects.
- [MoveResult events](reference/move-result.md): cascade steps, falls, spawns and how to replay a move.
- [Match3Level](reference/match3-level.md): the level timer, score, quotas and outcome in Godot.
- [Rules spec](rules-spec.md): the game's rules (moving to `reference/`).
- [Board view](reference/board-view.md): the 3D board's classes, scenes, camera fit and feel constants.

## How-to guides

- [Build and test](how-to/build-and-test.md): build the core and extension, run tests, load the game, capture screenshots.
- [Export a desktop build](how-to/export-a-desktop-build.md): get or make the Windows, macOS and Linux packages.
- [Regenerate or add sounds](how-to/regenerate-or-add-sounds.md): rebuild the generated audio, add a cue.
- [Play a sound from code](how-to/play-a-sound-from-code.md): call a cue from C++ or a scene, start and stop music.

## Explanation

- [About the board view](explanation/about-the-board-view.md): why the view never applies rules, and how the camera frames the board.
- [About the build pipeline](explanation/about-the-build-pipeline.md): why builds compile natively and export once.
- [Sound direction](explanation/sound-direction.md): why the game sounds the way it does.

Tutorials, more how-to guides and explanations are added by the work
packages that create what they describe.
