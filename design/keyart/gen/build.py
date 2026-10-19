# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate the key-art meshes and textures with numpy and Pillow.

    python3 design/keyart/gen/build.py [--only STAGE[,STAGE...]] [--res N]

Stages: temple, precinct, paving, props, sky, skyline, ground (default: all).
Writes design/keyart/meshes/*.obj and design/keyart/textures/*.png, and next
to each file the Godot .import file with its import settings (textures:
lossless with mipmaps, normal maps flagged; meshes: lightmap UV2 at a texel
size per mesh, no mesh compression). These are build outputs, like the
game's generated textures: git ignores them; the scenes, this code and the
renders are the sources. Same code and arguments give the same bytes.

Needs the project venv (.github/requirements/assets.txt: numpy, Pillow). The
bakes run in parallel worker processes; a full build takes about 15 minutes
on a 32-thread machine.
"""

import argparse
import sys
import time
from pathlib import Path

KEYART = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(KEYART))

from gen import temple, precinct, paving, props, sky, mountains, ground  # noqa: E402
from gen.atlas import autopack, write_obj  # noqa: E402

HDR = '# SPDX-License-Identifier: GPL-2.0-or-later\n# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors\n'

TEX_IMPORT = """[remap]

importer="texture"
type="CompressedTexture2D"

[params]

compress/mode=0
compress/normal_map={normal}
mipmaps/generate=true
detect_3d/compress_to=0
"""

OBJ_IMPORT = """[remap]

importer="wavefront_obj"
type="Mesh"

[params]

generate_tangents=true
generate_lods=false
generate_shadow_mesh=true
generate_lightmap_uv2={uv2}
generate_lightmap_uv2_texel_size={texel}
scale_mesh=Vector3(1, 1, 1)
offset_mesh=Vector3(0, 0, 0)
force_disable_mesh_compression=true
"""

# lightmap texel size (m) per mesh; None = not baked (no UV2)
LIGHTMAP_TEXEL = {
    'temple_body': 0.2, 'temple_shrines': 0.1, 'temple_small': 0.3, 'palace': 0.35, 'wall': 0.5,
    'ground_plaza': 0.5, 'ground_studio': 0.5, 'brazier': 0.05,
}


def write_imports():
    for f in sorted((KEYART / 'textures').glob('*.png')):
        (f.parent / (f.name + '.import')).write_text(TEX_IMPORT.format(normal=1 if f.stem.endswith('_normal') else 0))
    for f in sorted((KEYART / 'meshes').glob('*.obj')):
        texel = LIGHTMAP_TEXEL.get(f.stem)
        (f.parent / (f.name + '.import')).write_text(
            OBJ_IMPORT.format(uv2='true' if texel else 'false', texel=texel or 0.2))


def stage_temple(res):
    body = temple.build()
    autopack(body, res, res)
    write_obj(str(KEYART / 'meshes/temple_body.obj'), body, res, res,
              HDR + '# Templo Mayor platform, stairs and balustrades; UVs map textures/body_*.png\n')
    temple.bake(body, res, res, str(KEYART / 'textures/body'))
    sh = temple.build_shrines()
    autopack(sh, res, res)
    write_obj(str(KEYART / 'meshes/temple_shrines.obj'), sh, res, res,
              HDR + '# Shrines of Tlaloc (north, blue) and Huitzilopochtli (south, ochre); UVs map textures/shrines_*.png\n')
    temple.bake(sh, res, res, str(KEYART / 'textures/shrines'))


def stage_precinct(res):
    notes = {'temple_small': 'Invented smaller west-facing precinct temple (generic form, inferred crests)',
             'palace': 'Invented palace platform with a range of rooms (white rooms, red door frames, inferred crests)',
             'wall': 'Precinct wall segment, 40 m'}
    parts = precinct.build_parts()
    allg = [g for gs in parts.values() for g in gs]
    autopack(allg, res, res)
    for name, gs in parts.items():
        write_obj(str(KEYART / f'meshes/{name}.obj'), gs, res, res, HDR + f'# {notes[name]}; UVs map textures/precinct_*.png\n')
    temple.bake(allg, res, res, str(KEYART / 'textures/precinct'))


STAGES = {
    'temple': stage_temple,
    'precinct': stage_precinct,
    'paving': lambda res: paving.make(str(KEYART / 'textures')),
    'props': lambda res: props.make(str(KEYART)),
    'sky': lambda res: sky.make(str(KEYART)),
    'skyline': lambda res: mountains.make(str(KEYART)),
    'ground': lambda res: ground.make(str(KEYART)),
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--only', default=','.join(STAGES), help='comma-separated stages')
    ap.add_argument('--res', type=int, default=4096, help='atlas size for temple and precinct')
    a = ap.parse_args()
    (KEYART / 'meshes').mkdir(exist_ok=True)
    (KEYART / 'textures').mkdir(exist_ok=True)
    for name in a.only.split(','):
        t = time.time()
        STAGES[name](a.res)
        print(f'{name}: {time.time() - t:.0f} s', flush=True)
    write_imports()


if __name__ == '__main__':
    main()
