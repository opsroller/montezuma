# Montezuma's

A cross-platform 3D remake of the gameplay of *The Treasures of Montezuma*
(Visual Shape and Alawar, PC 2006), built with Godot 4 and C++20, licensed
GPL-2.0-or-later.
Read `docs/reference/project-profile.md` before any work: it has the
stack, commands, paths each role owns and the project's rules.

- Roles come from generic job-function plugins (catalog:
  `opsroller/claude-plugins`). This repo supplies the project specifics.
  A discipline starts only once its role plugin is merged.
- Work is split into packages in `docs/reference/work-packages.md`;
  interfaces between them are in `docs/reference/contracts.md`.
- Git: branch, Conventional Commits, draft PR, code-reviewer pass,
  **squash merge only**, author merges when green. The owner does not
  review PRs.
- Game code is compiled C++20 (WebAssembly for the web demo). No game
  logic in GDScript, JavaScript or TypeScript.
- Every source file carries the SPDX header from the project profile.
- No individual's name in project files other than the owner's;
  third-party license notices that must be kept verbatim are exempt.
- Docs follow Diataxis (`docs/README.md`) and change in the same PR as
  the code.
- All code and assets are our own; nothing from the original game's files.
