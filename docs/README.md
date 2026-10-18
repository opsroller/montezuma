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
- [Audio](reference/audio.md): sound formats, levels and the files behind each cue.
- [Power Totems](reference/totems.md): totem ids, charging, effects and tuning.
- [Power-ups](reference/bonuses.md): on-board power-up ids, tiers, placement, triggering and effects.
- [MoveResult events](reference/move-result.md): cascade steps, falls, spawns and how to replay a move.
- [Match3Level](reference/match3-level.md): the level timer, score, quotas and outcome in Godot.
- [Board view](reference/board-view.md): the 3D board's classes, scenes, camera fit and feel constants.
- [Rules spec](reference/rules-spec.md): how the game plays, with
  [Power Totems](reference/rules-totems.md), [power-ups](reference/rules-power-ups.md),
  [bonus games](reference/rules-bonus-games.md) and [sources](reference/rules-sources.md).
- [Progression](reference/progression.md): episodes, levels, unlocks, upgrades, trophies, intro level.
- [Balance targets](reference/balance.md): what the simulator checks each level against.

## Explanation

- [About the rules](explanation/about-the-rules.md): which original we follow, and every choice and deviation.
- [About rule sets and themes](explanation/about-rule-sets-and-themes.md): how mini games fit the design.

## How-to guides

- [Build and test](how-to/build-and-test.md): build the core and extension, run tests, load the game, capture screenshots.
- [Regenerate or add sounds](how-to/regenerate-or-add-sounds.md): rebuild the generated audio, add a cue.

## Explanation

- [About the board view](explanation/about-the-board-view.md): why the view never applies rules, and how the camera frames the board.
- [Sound direction](explanation/sound-direction.md): why the game sounds the way it does.

Tutorials, more how-to guides and explanations are added by the work
packages that create what they describe.
