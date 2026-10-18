# Role plugins

Each plugin is one job on the port: an agent definition with the role and
job function, the skills it needs, and the paths it owns. Every role also
loads `montezuma-common`.

| Plugin | Role | Skills |
|---|---|---|
| `montezuma-common` | Shared by all roles | clean-room, conventional-commits, work-package, coordinate; `montezuma-orienter` agent |
| `gameplay-engineer` | Game logic in `core/` | gdscript-core, headless-tests |
| `level-designer` | Rules spec, levels, balance | rules-research, level-data, balance-sim |
| `board-presentation` | 3D board, camera, input, animation, feel | board-scene, game-feel |
| `asset-artist` | Original models, materials, shaders | procedural-models, materials-shaders |
| `audio-designer` | Original sound, music, cue player | sound-synthesis, audio-cues |
| `ui-engineer` | HUD, menus, screens | godot-ui |
| `release-engineer` | CI, exports, releases | godot-export |

## Installing

From a checkout of this repo, in Claude Code:

```
/plugin marketplace add ./
/plugin install montezuma-common@montezuma-3d
/plugin install gameplay-engineer@montezuma-3d
```

or from anywhere: `/plugin marketplace add opsroller/montezuma`.

An agent that can't install plugins can still take a role: read
`plugins/<role>/agents/<role>.md` as its brief and the `SKILL.md` files
under `plugins/<role>/skills/` and `plugins/montezuma-common/skills/`.

## Adding or changing a role

Add a role only when a work package needs skills no existing role has.
Validate with `claude plugin validate plugins/<name>` and
`claude plugin validate .`, and commit as `feat(plugins): ...`.
