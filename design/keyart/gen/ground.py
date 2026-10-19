# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Ground meshes for the lightmap bakes: the paved plaza inside the precinct wall
and a smaller studio floor for the hero and icon stills. Flat grids with UVs in
paving tiles (6.4 m), so the material maps textures/paving_*.png directly and
the importer can unwrap a lightmap UV2."""

import numpy as np

HDR = '# SPDX-License-Identifier: GPL-2.0-or-later\n# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors\n'
TILE = 6.4


def grid(path, note, x0, x1, z0, z1, step):
    xs = np.arange(x0, x1 + 1e-6, step); zs = np.arange(z0, z1 + 1e-6, step)
    nx, nz = len(xs), len(zs)
    with open(path, 'w') as f:
        f.write(HDR + note)
        for z in zs:
            for x in xs:
                f.write(f'v {x:.2f} 0 {z:.2f}\n')
        for z in zs:
            for x in xs:
                f.write(f'vt {x / TILE:.5f} {-z / TILE:.5f}\n')
        f.write('vn 0 1 0\n')
        for j in range(nz - 1):
            for i in range(nx - 1):
                a = j * nx + i + 1; b = a + 1; c = a + nx + 1; d = a + nx
                f.write(f'f {a}/{a}/1 {d}/{d}/1 {c}/{c}/1 {b}/{b}/1\n')


def make(out):
    grid(out + '/meshes/ground_plaza.obj', '# Paved plaza inside the precinct wall, 1 unit = 1 m\n', -136, 136, -56, 264, 8)
    grid(out + '/meshes/ground_studio.obj', '# Studio floor for the hero and icon stills, 1 unit = 1 m\n', -120, 120, -120, 120, 8)
