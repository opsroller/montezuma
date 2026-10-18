---
name: conventional-commits
description: Use whenever you commit, name a branch or title a PR in Montezuma 3D. All history follows Conventional Commits 1.0.
---

# Source control rules

All work happens in git, on a branch, and lands through a pull request.
Nobody commits straight to `main`.

## Branches

`wp/<id>-<slug>`, for example `wp/06-artifact-models`. Work that is not a
work package uses `<type>/<slug>`, for example `fix/cascade-score`.

## Commit messages

[Conventional Commits 1.0](https://www.conventionalcommits.org/en/v1.0.0/):

```
<type>(<scope>): <summary in imperative, lower case, no period>

<optional body: why, not what>

Refs: WP-06
```

Types: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `style`, `build`,
`ci`, `chore`, `revert`. Breaking changes add `!` after the scope and a
`BREAKING CHANGE:` footer.

Scopes match the area you own:

| Scope | Area |
|---|---|
| `core` | `core/` game logic |
| `levels` | `data/levels/`, `tools/sim/` |
| `spec` | `docs/rules-spec.md` |
| `board` | `scenes/board/` |
| `assets` | `assets/models/`, `assets/materials/` |
| `audio` | `assets/audio/`, `scenes/audio/` |
| `ui` | `ui/` |
| `ci` / `build` | `.github/`, `export_presets.cfg` |
| `plugins` | `plugins/`, `.claude-plugin/` |
| `contracts` | `docs/contracts.md` |

Examples:

```
feat(core): emit cell-level events in MoveResult
fix(board): stop pieces overshooting on long falls
docs(spec): confirm 8x8 board from playthrough
ci(build): add web export job
```

## Pull requests

- Title is a Conventional Commit line; it becomes the squash commit.
- Open as a draft early, mark ready when tests pass.
- One work package per PR.
