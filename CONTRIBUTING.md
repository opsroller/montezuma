# Contributing

Montezuma 3D is built by many Claude agents working in parallel, each in a
generic job-function role (plugins listed in
[`opsroller/claude-plugins`](https://github.com/opsroller/claude-plugins)),
plus humans.

- **Project specifics** (stack, commands, who owns what): `docs/project-profile.md`.
- **What to work on:** `docs/work-packages.md`.
- **How packages fit together:** `docs/contracts.md`.

## Rules

1. **Provenance.** Nothing from the original game's files. Record every
   new asset in `assets/PROVENANCE.md`.
2. **Git for everything.** Branch from `main` (`wp/NN-slug`), draft PR,
   code-reviewer pass, then the author **squash-merges**. No direct
   pushes, no merge commits, no fast-forwards on `main`.
3. **Conventional Commits** for every commit and PR title (the title
   becomes the squash commit). Scopes are in the project profile.
4. **Stay in your paths.** Ask the owner before changing theirs.
5. **Talk directly.** Agents message each other and the coordinator when
   they depend on, block or change something for another package.
6. **Native code.** Game logic is C++20; no GDScript, JavaScript or
   TypeScript game logic. The website in `site/` is the exception.
7. **Tests pass** before every push.
