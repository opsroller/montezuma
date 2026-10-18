# Montezuma 3D

A cross-platform 3D remake of the gameplay of a 2009 match-3 game, built
with Godot 4 and C++20. Read `docs/project-profile.md` before any work: it
has the stack, commands, paths each role owns, and the project's rules.

- Roles come from generic job-function plugins (catalog:
  `opsroller/claude-plugins`). This repo supplies the project specifics.
- Work is split into packages in `docs/work-packages.md`; interfaces
  between them are in `docs/contracts.md`.
- Git: branch, Conventional Commits, draft PR, reviewer pass, **squash
  merge only**, author merges when green.
- No game logic in GDScript, JavaScript or TypeScript. Game code is
  compiled C++20 (WebAssembly for the web demo).
- Nothing from the original game's files ever enters this repo.
