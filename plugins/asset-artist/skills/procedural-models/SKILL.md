---
name: procedural-models
description: How to make Montezuma 3D models from code, with or without Blender. Use when creating or changing a mesh in assets/models/.
---

# Making models

## Option A: Blender, headless (preferred when available)

```sh
blender --background --python tools/assets/make_artifacts.py -- --out assets/models/artifacts
```

The script builds geometry with `bpy`/`bmesh`, applies modifiers, and
exports one `.glb` per asset. Install Blender from blender.org's Linux
tarball if `blender` is missing.

## Option B: Godot, in code

A `@tool` or headless GDScript in `tools/assets/` builds `ArrayMesh` with
`SurfaceTool` (or CSG for blocky temple parts), then saves with
`ResourceSaver.save(mesh, "res://assets/models/....tres")`.

## Shapes

Seven artifact kinds, each a carved object with its own outline: for
example a sun disc, a skull, a feathered serpent coil, a jade mask, a
pyramid stud, a jaguar head, a moon crescent. Bevel edges so they catch
light; keep the top view distinct.

## Manifest

`assets/manifest.json` maps logical names to files:

```json
{ "artifact_0": "res://assets/models/artifacts/sun_disc.glb",
  "temple_board": "res://assets/models/temple/board.glb" }
```
