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
- [Build pipeline](reference/build-pipeline.md): desktop and web build jobs, artifacts, versions and pinned tools.
- [IntroLevel](reference/intro-level.md): the web demo's scene root, its win signal and page events.
- [Board themes](reference/board-themes.md): theme files, asset names, generators, budgets, validator.
- [Audio](reference/audio.md): sound formats, levels and the files behind each cue.
- [Power Totems](reference/totems.md): totem ids, charging, effects and tuning.
- [Power-ups](reference/bonuses.md): on-board power-up ids, tiers, placement, triggering and effects.
- [UI](reference/ui.md): UI classes, screens, actions, settings, theme tokens and strings.
- [MoveResult events](reference/move-result.md): cascade steps, falls, spawns and how to replay a move.
- [Match3Level](reference/match3-level.md): the level timer, score, quotas and outcome in Godot.
- [Level data](reference/level-data.md): the level file format, defaults, limits and errors.
- [Website](reference/website.md): pages, release data, platform detection, demo hand-off contract.
- [Website audit](reference/website-audit.md): what the website passes and what is open, page by page.
- [Board view](reference/board-view.md): the 3D board's classes, scenes, camera fit and feel constants.
- [CI pipeline](reference/ci-pipeline.md): the tests, assets and levels workflows' triggers, jobs and caches.
- [Rules spec](reference/rules-spec.md): how the game plays, with
  [Power Totems](reference/rules-totems.md), [power-ups](reference/rules-power-ups.md),
  [bonus games](reference/rules-bonus-games.md) and [sources](reference/rules-sources.md).
- [Progression](reference/progression.md): episodes, levels, unlocks, upgrades, trophies, intro level.
- [Balance targets](reference/balance.md): what the simulator checks each level against.
- [Level session](reference/level-session.md): one level by the full rules, its commands, reports and defaults.
- [Match3Game](reference/match3-game.md): the game scene's controller, its signals, cues and self-test.
- [Visual target](reference/visual-target.md): the look per quality tier, frame budgets and acceptance tests.
- [Fire](reference/fire.md): the brazier fire's simulation, flipbooks, scene, shaders, flickering light and per-tier behaviour.
- [Quality tiers](reference/quality-tiers.md): the `QualityTiers` autoload, settings key, feature ids, node groups and render settings per tier.
- [Benchmark and readability capture](reference/benchmark.md): the benchmark replay, its JSON report, feature keys and readability checks.
- [Benchmark results](reference/benchmark-results.md): measured frame times, feature rows and readability per machine and tier.

## Explanation

- [About the rules](explanation/about-the-rules.md): which original we follow, and every choice and deviation.
- [About rule sets and themes](explanation/about-rule-sets-and-themes.md): how mini games fit the design.

## How-to guides

- [Build and test](how-to/build-and-test.md): build the core and extension, run tests, load the game, capture screenshots.
- [Export a desktop build](how-to/export-a-desktop-build.md): get or make the Windows, macOS and Linux packages.
- [Export the web demo](how-to/export-a-web-build.md): get or make the web package and run it in a browser.
- [Make a board theme](how-to/make-a-board-theme.md): change the temple look or add a new theme.
- [Regenerate or add sounds](how-to/regenerate-or-add-sounds.md): rebuild the generated audio, add a cue.
- [Play a sound from code](how-to/play-a-sound-from-code.md): call a cue from C++ or a scene, start and stop music.
- [Run the balance simulator](how-to/run-the-balance-simulator.md): play levels with bots and check them against the targets.
- [Add a UI screen](how-to/add-a-ui-screen.md): a new menu screen or modal, opened from a button.
- [Check UI text and layout](how-to/check-ui-text-and-layout.md): pseudo-localization, large text, window sizes, controller only.
- [Build and preview the website](how-to/build-and-preview-the-website.md): build `site/`, preview it with or without the web demo, update the embedded web build, check a deployed demo, run its tests.
- [Add a level](how-to/add-a-level.md): write a campaign level file from the progression tables, test it and play it.
- [Make content depend on the quality tier](how-to/make-content-depend-on-the-quality-tier.md): desktop-only nodes, fallbacks, light shadows, texture variants.
- [Capture the effects showcase](how-to/capture-the-effects-showcase.md): record every totem and power-up effect to frames.
- [Run the benchmark](how-to/run-the-benchmark.md): measure frame time and check readability on a desktop or a phone.
- [Tune the fire](how-to/tune-the-fire.md): change the brazier fire's simulation, colour or particles and check it.

## Explanation

- [UI architecture](explanation/ui-architecture.md): why a C++ screen stack, an overlay and a tokens-built theme.
- [About the board view](explanation/about-the-board-view.md): why the view never applies rules, and how the camera frames the board.
- [About the game scene](explanation/about-the-game-scene.md): why the scene is a thin layer over the level session, and why a move is one animation.
- [Art direction](explanation/art-direction.md): why the board looks the way it does, and the rules new art follows.
- [About the visual target](explanation/about-the-visual-target.md): why a realistic high-end look, and what Godot can and cannot do with ray tracing.
- [About the fire](explanation/about-the-fire.md): why a simulated flipbook with motion vectors, why our own simulation, how it stays off the board.
- [About quality tiers](explanation/about-quality-tiers.md): how a tier is picked, why the renderer is fixed per platform, why tiers only switch effects off.
- [About lighting](explanation/about-lighting.md): baked and real-time GI per tier, why VoxelGI over SDFGI, physical units, height fog and the lighting costs.
- [About post-processing and the colour grade](explanation/about-post-processing.md): glow before tonemapping, the CDL grade as a LUT and its palette check, backdrop depth of field that follows the camera, and their costs.
- [About the atmosphere](explanation/about-the-atmosphere.md): why one fog volume that is empty over the board, why shafts come from stretched noise, why cards on mobile and web.
- [About CI caching](explanation/about-ci-caching.md): why CI uses a compiler cache and what it leaves uncached.
- [About CI on every push](explanation/about-ci-on-every-push.md): why every branch push runs the checks, and why branch PRs skip their own run.
- [About level clocks](explanation/about-level-clocks.md): why each level has its own base clock, why timer upgrades add to it, and why the clock rule counts only wins.
- [About the episode 3 levels](explanation/about-the-episode-3-levels.md): why chains then steel, what the simulator measures for chains, and human time against the clock.
- [About the episode 4 levels](explanation/about-the-episode-4-levels.md): how the one ward is taught, the chain shapes, and why `random` has no target from episode 4.
- [About the episode 5 levels](explanation/about-the-episode-5-levels.md): how the final episode gets harder through goals and obstacles alone, its new chain shapes, and what the simulator cannot see there.
- [About the build pipeline](explanation/about-the-build-pipeline.md): why builds compile natively and export once, and how the web demo is built.
- [Sound direction](explanation/sound-direction.md): why the game sounds the way it does.
- [About the website](explanation/about-the-website.md): why the front page is the playable intro level and how it hands off to downloads.

Tutorials, more how-to guides and explanations are added by the work
packages that create what they describe.
