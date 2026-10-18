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
- [Board themes](reference/board-themes.md): theme files, asset names, generators, budgets, validator.
- [Audio](reference/audio.md): sound formats, levels and the files behind each cue.
- [Power Totems](reference/totems.md): totem ids, charging, effects and tuning.
- [Power-ups](reference/bonuses.md): on-board power-up ids, tiers, placement, triggering and effects.
- [UI](reference/ui.md): UI classes, screens, actions, settings, theme tokens and strings.
- [MoveResult events](reference/move-result.md): cascade steps, falls, spawns and how to replay a move.
- [Match3Level](reference/match3-level.md): the level timer, score, quotas and outcome in Godot.
- [Level data](reference/level-data.md): the level file format, defaults, limits and errors.
- [Website](reference/website.md): pages, release data, platform detection, demo hand-off contract.
- [Board view](reference/board-view.md): the 3D board's classes, scenes, camera fit and feel constants.
- [CI pipeline](reference/ci-pipeline.md): the tests workflow's triggers, jobs and caches.
- [Rules spec](reference/rules-spec.md): how the game plays, with
  [Power Totems](reference/rules-totems.md), [power-ups](reference/rules-power-ups.md),
  [bonus games](reference/rules-bonus-games.md) and [sources](reference/rules-sources.md).
- [Progression](reference/progression.md): episodes, levels, unlocks, upgrades, trophies, intro level.
- [Balance targets](reference/balance.md): what the simulator checks each level against.
- [Visual target](reference/visual-target.md): the look per quality tier, frame budgets and acceptance tests.

## Explanation

- [About the rules](explanation/about-the-rules.md): which original we follow, and every choice and deviation.
- [About rule sets and themes](explanation/about-rule-sets-and-themes.md): how mini games fit the design.

## How-to guides

- [Build and test](how-to/build-and-test.md): build the core and extension, run tests, load the game, capture screenshots.
- [Export a desktop build](how-to/export-a-desktop-build.md): get or make the Windows, macOS and Linux packages.
- [Make a board theme](how-to/make-a-board-theme.md): change the temple look or add a new theme.
- [Regenerate or add sounds](how-to/regenerate-or-add-sounds.md): rebuild the generated audio, add a cue.
- [Play a sound from code](how-to/play-a-sound-from-code.md): call a cue from C++ or a scene, start and stop music.
- [Add a UI screen](how-to/add-a-ui-screen.md): a new menu screen or modal, opened from a button.
- [Check UI text and layout](how-to/check-ui-text-and-layout.md): pseudo-localization, large text, window sizes, controller only.
- [Build and preview the website](how-to/build-and-preview-the-website.md): build `site/`, preview it with or without the web demo, run its tests.

## Explanation

- [UI architecture](explanation/ui-architecture.md): why a C++ screen stack, an overlay and a tokens-built theme.
- [About the board view](explanation/about-the-board-view.md): why the view never applies rules, and how the camera frames the board.
- [Art direction](explanation/art-direction.md): why the board looks the way it does, and the rules new art follows.
- [About the visual target](explanation/about-the-visual-target.md): why a realistic high-end look, and what Godot can and cannot do with ray tracing.
- [About CI caching](explanation/about-ci-caching.md): why CI uses a compiler cache and what it leaves uncached.
- [About the build pipeline](explanation/about-the-build-pipeline.md): why builds compile natively and export once.
- [Sound direction](explanation/sound-direction.md): why the game sounds the way it does.
- [About the website](explanation/about-the-website.md): why the front page is the playable intro level and how it hands off to downloads.

Tutorials, more how-to guides and explanations are added by the work
packages that create what they describe.
