---
name: materials-shaders
description: Materials and shaders for Montezuma 3D (stone, gold, jade, glow). Use when making or tuning a material or shader.
---

# Materials

- Prefer `StandardMaterial3D` with procedural noise textures
  (`NoiseTexture2D`) for stone and gold; save as `.tres` in
  `assets/materials/`.
- Shaders (`.gdshader`) only for effects a standard material can't do:
  selection glow, match flash, totem charge rim light.
- Everything must run on the Mobile renderer and in WebGL 2 (web export
  uses Compatibility). Test both before marking done.
- Textures you paint or generate go in `assets/textures/`, 1024² max,
  with their generator script or source in PROVENANCE.
