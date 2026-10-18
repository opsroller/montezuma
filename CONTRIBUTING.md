# Contributing

Montezuma 3D is built by many Claude agents working in parallel, each in a
role defined as a Claude plugin in `plugins/`, plus humans.

- **What to work on:** `docs/work-packages.md`.
- **How packages fit together:** `docs/contracts.md`.
- **Roles:** `plugins/README.md`.

## Rules

1. **Clean room.** Nothing from Alawar's original game: no code, art,
   audio, text or name. Record every new asset in `assets/PROVENANCE.md`.
2. **Git for everything.** Branch from `main` (`wp/NN-slug`), open a draft
   PR, merge through review. No direct pushes to `main`.
3. **Conventional Commits** for every commit and PR title, e.g.
   `feat(core): add power totem charging`. Scopes are listed in the
   `conventional-commits` skill.
4. **Stay in your paths.** Each package owns paths; ask the owner before
   changing theirs.
5. **Talk directly.** Threads message each other (and the project
   coordinator) when they depend on, block or change something for another
   package. See the `coordinate` skill.
6. **Tests pass** headless before every push.
