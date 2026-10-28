/**
 * montezuma-plants: the plant library's pipeline as tools, not as typed commands.
 *
 * Every step of this loop used to be composed by hand each round - interpreter paths
 * guessed, shell loops written inline, scratch sprayed into the system temp
 * directory, the capture resolution retyped, the import step forgotten so a frame
 * showed the previous meshes. Each of those has exactly one right answer, so none of
 * them belongs in a command line. They are fixed here:
 *
 *   - argv only, through $.process.run. No shell: nothing to quote, chain or escape.
 *   - the interpreter is resolved once, by probing, and never written by a caller.
 *   - captures are 3840x2160, the project's quality floor, and not a parameter.
 *   - scratch is <workspace>/.scratch, relative, never the system temp directory.
 *   - the GPU lock is derived from git's common directory, so no path to anyone's
 *     home directory appears in this source.
 *   - `lookdev` runs the steps in the one order that works, import included.
 *
 * Everything below the hook is a pure function of data: it builds argv and formats
 * reports. `$` is only ever spelled at the call site, inside the hook.
 */

import type { Register } from 'claude-code'

const SCRATCH = '.scratch'
const SHOTS = '.scratch/lookdev'
/** The quality floor (CLAUDE.md): nothing is judged below this. Not a parameter. */
const CAPTURE = { width: 3840, height: 2160, frames: 4 }
const PARAMS = 'tools/assets/themes/plants.json'
const PLANTS = 'tools/assets/plants.py'

const MINUTE = 60_000

type Run = { exitCode: number; stdout: string; stderr: string }
type Input = { species?: string; scope?: string; page?: string; per?: number }

/** `--only a,b`, or nothing at all. Never a half-built flag. */
const onlyArgs = (species?: string): string[] =>
  species && species.trim() ? ['--only', species.trim()] : []

/** `heliconia,tree_fern` -> `HeliconiaClump`-style page prefixes. */
const pagePrefixes = (species?: string): string[] =>
  (species ?? '')
    .split(',')
    .map((k) => k.trim())
    .filter(Boolean)
    .map((k) => k.split('_').map((w) => w.charAt(0).toUpperCase() + w.slice(1)).join(''))

const tail = (text: string, lines = 40): string =>
  text.trim().split('\n').slice(-lines).join('\n')

const report = (label: string, r: Run): string =>
  `${label}: exit ${r.exitCode}\n${tail(r.stdout)}${
    r.stderr.trim() ? `\n--- stderr ---\n${tail(r.stderr, 20)}` : ''
  }`

/** The lock lives beside the directory every worktree of this checkout shares. */
const lockFrom = (gitCommonDir: string): string =>
  `${gitCommonDir.trim().replace(/\/\.git$/, '')}/.godot-gpu.lock`

const captureArgv = (lock: string, page: string, work: string): string[] => [
  'flock', lock,
  'gamescope', '--backend', 'headless',
  '-W', String(CAPTURE.width), '-H', String(CAPTURE.height), '--',
  'godot', '--path', 'game', '--rendering-driver', 'vulkan',
  `res://assets/lookdev/${page}.tscn`,
  '--write-movie', `${work}/f.png`,
  '--fixed-fps', '30', '--quit-after', String(CAPTURE.frames),
  '--', '--start-screen=',
]

const selectPages = (listing: string, scope: string, species?: string): string[] => {
  const all = listing.split('\n').map((s) => s.trim())
    .filter((n) => n.endsWith('.tscn')).map((n) => n.replace(/\.tscn$/, ''))
  const rows = all.filter((n) => n.startsWith('LD_Plants_'))
  // The _Empty companions exist for masking in measurement, not for looking at.
  const specimens = all.filter((n) => n.startsWith('LD_Plant_') && !n.endsWith('_Empty'))
  if (scope === 'rows') return rows
  const keys = pagePrefixes(species)
  const wanted = keys.length
    ? specimens.filter((n) => keys.some((k) => n.startsWith(`LD_Plant_${k}`)))
    : specimens
  return scope === 'all' ? [...rows, ...wanted] : wanted
}

const TOOLS = [
  {
    name: 'params',
    description:
      'Write tools/assets/themes/plants.json from the species in plants.py. Must run ' +
      'before atlases, which read it. Seconds.',
  },
  {
    name: 'atlases',
    description:
      'Draw the 36 leaf and bark atlases at 4096 from plants.json. About 3 minutes when ' +
      'the colours changed, instant when they did not. Run after params.',
  },
  {
    name: 'build',
    description:
      'Build the meshes, materials and the plant section of game/assets/library.json ' +
      "under the flake's pinned Blender, which is part of the asset: two committed " +
      'meshes were found not to reproduce byte for byte under another Blender.',
    inputSchema: {
      type: 'object',
      properties: {
        species: { type: 'string', description: 'comma-separated keys, e.g. "heliconia,ceiba"' },
      },
    },
  },
  {
    name: 'pages',
    description:
      'Write the look-dev scenes: a close-up page per specimen with its empty companion ' +
      'for masking, plus the seven band rows.',
    inputSchema: { type: 'object', properties: { species: { type: 'string' } } },
  },
  {
    name: 'import',
    description:
      "Run Godot's import pass. REQUIRED after any mesh or texture change: Godot serves " +
      'meshes from .godot/imported, so without it a capture shows the previous assets.',
  },
  {
    name: 'check',
    description:
      'Check the built meshes for degenerate faces, winding against a neighbour, normals ' +
      'facing the wrong way, UVs straddling an atlas tile, and meshes no catalogue entry ' +
      'references.',
  },
  {
    name: 'capture',
    description:
      'Render look-dev pages at 3840x2160 into .scratch/lookdev. scope "species" ' +
      '(default), "rows" or "all". About 15 s a page, nearly all of it Godot starting up.',
    inputSchema: {
      type: 'object',
      properties: {
        scope: { type: 'string', enum: ['species', 'rows', 'all'] },
        species: { type: 'string' },
      },
    },
  },
  {
    name: 'sheet',
    description:
      'Lay the captured frames out as labelled contact sheets. Nearly every fault in this ' +
      'library was found by seeing nine species at once and none by looking at one at a time.',
    inputSchema: { type: 'object', properties: { per: { type: 'number' } } },
  },
  {
    name: 'measure',
    description:
      'Read the figures of docs/reference/foliage-reference.md back off a captured frame, ' +
      'masked against its empty companion.',
    inputSchema: {
      type: 'object',
      properties: { page: { type: 'string', description: 'e.g. LD_Plant_HeliconiaClump' } },
      required: ['page'],
    },
  },
  {
    name: 'lookdev',
    description:
      'The whole loop in the order that works: params, atlases, build, pages, import, ' +
      'check, capture, sheet. Prefer this over running the steps by hand.',
    inputSchema: {
      type: 'object',
      properties: {
        species: { type: 'string' },
        scope: { type: 'string', enum: ['species', 'rows', 'all'] },
      },
    },
  },
]

export const register: Register = (on) => {
  /** Taken from session.start; `$` itself is never stored, passed or read. */
  let workspace = '.'
  let lock = ''
  let python = ''

  on('session.start', async ($, e, next) => {
    workspace = e.cwd ?? '.'
    for (const tool of TOOLS) await $.tool.register({ ...tool, isDeferred: false })
    return next(e)
  })

  on('tool.call', async ($, e, next) => {
    const name = String(e.tool).replace(/^mcp__montezuma-plants__/, '')
    if (name === e.tool) return next(e)
    const cwd = workspace
    const input = (e.input ?? {}) as Input

    try {
      // The shared GPU lock, derived once from the repository rather than written down.
      if (!lock) {
        const common = await $.process.run(
          ['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'],
          { cwd, timeoutMs: MINUTE },
        )
        if (!common.stdout.trim()) {
          return { text: 'not a git repository: cannot place the shared GPU lock' }
        }
        lock = lockFrom(common.stdout)
      }
      // Which interpreter has numpy and Pillow, decided by asking, once.
      if (!python && ['atlases', 'sheet', 'measure', 'lookdev'].includes(name)) {
        for (const candidate of ['.venv/bin/python', 'python3']) {
          const probe = await $.process.run([candidate, '-c', 'import numpy, PIL'],
            { cwd, timeoutMs: MINUTE }).catch(() => ({ exitCode: 1 }))
          if (probe.exitCode === 0) { python = candidate; break }
        }
        if (!python) {
          return { text:
            'no interpreter here has numpy and Pillow. Put them in the flake (then ' +
            'python3 works and nothing needs a prefix), or create the workspace venv ' +
            'from .github/requirements/assets.txt.' }
        }
      }

      const steps: Record<string, () => Promise<Run>> = {
        params: () => $.process.run(['python3', PLANTS, 'textures'], { cwd, timeoutMs: 2 * MINUTE }),
        atlases: () => $.process.run([python, 'tools/assets/gen_theme_textures.py', PARAMS],
          { cwd, timeoutMs: 60 * MINUTE }),
        build: () => $.process.run(
          ['blender', '-b', '--factory-startup', '-P', PLANTS, '--', 'build',
            ...onlyArgs(input.species)],
          { cwd, timeoutMs: 30 * MINUTE }),
        import: () => $.process.run(
          ['flock', lock, 'godot', '--path', 'game', '--import', '--headless'],
          { cwd, timeoutMs: 30 * MINUTE }),
        check: () => $.process.run(['python3', 'tools/assets/check_plant_meshes.py'],
          { cwd, timeoutMs: 10 * MINUTE }),
        sheet: () => $.process.run(
          [python, 'tools/assets/contact_sheet.py', SHOTS, `${SHOTS}/sheet.png`,
            '--per', String(input.per ?? 9)],
          { cwd, timeoutMs: 10 * MINUTE }),
      }

      if (name === 'pages' || name === 'lookdev' || steps[name]) {
        // `pages` is two runs, so it is not in the table above.
      }

      const runPages = async (): Promise<Run> => {
        const close = await $.process.run(
          ['python3', PLANTS, 'closeups', '--empty', ...onlyArgs(input.species)],
          { cwd, timeoutMs: 5 * MINUTE })
        const rows = await $.process.run(['python3', PLANTS, 'lookdev'],
          { cwd, timeoutMs: 5 * MINUTE })
        return {
          exitCode: close.exitCode || rows.exitCode,
          stdout: `${close.stdout}${rows.stdout}`,
          stderr: `${close.stderr}${rows.stderr}`,
        }
      }

      const capturePage = async (page: string): Promise<string> => {
        const work = `${SCRATCH}/cap-${page}`
        await $.process.run(['rm', '-rf', work], { cwd, timeoutMs: MINUTE })
        await $.process.run(['mkdir', '-p', work, SHOTS], { cwd, timeoutMs: MINUTE })
        const shot = await $.process.run(captureArgv(lock, page, work),
          { cwd, timeoutMs: 10 * MINUTE })
        const found = await $.process.run(['ls', '-1', work], { cwd, timeoutMs: MINUTE })
        // Movie Maker numbers its frames; the last is the settled image.
        const frames = found.stdout.split('\n').map((s) => s.trim())
          .filter((n) => n.startsWith('f') && n.endsWith('.png')).sort()
        const last = frames[frames.length - 1]
        if (!last) return `${page}: FAILED, no frame written\n${tail(shot.stderr, 8)}`
        await $.process.run(['cp', `${work}/${last}`, `${SHOTS}/${page}.png`],
          { cwd, timeoutMs: MINUTE })
        await $.process.run(['rm', '-rf', work], { cwd, timeoutMs: MINUTE })
        return `${page}: ${SHOTS}/${page}.png`
      }

      const captureAll = async (scope: string): Promise<string> => {
        const listing = await $.process.run(['ls', '-1', 'game/assets/lookdev'],
          { cwd, timeoutMs: MINUTE })
        const pages = selectPages(listing.stdout, scope, input.species)
        if (!pages.length) return 'capture: no pages matched'
        const lines: string[] = []
        for (const [i, page] of pages.entries()) {
          // Progress is pushed as it happens; nothing polls for it.
          await $.ui.status(`plants: capturing ${i + 1}/${pages.length} ${page}`)
          lines.push(await capturePage(page))
        }
        await $.ui.status(undefined)
        return `captured ${pages.length} page(s) at ${CAPTURE.width}x${CAPTURE.height}\n${
          lines.join('\n')}`
      }

      if (steps[name]) return { text: report(name, await steps[name]()) }
      if (name === 'pages') return { text: report('pages', await runPages()) }
      if (name === 'capture') return { text: await captureAll(input.scope ?? 'species') }
      if (name === 'measure') {
        const r = await $.process.run(
          [python, 'tools/assets/measure_foliage.py', `${SHOTS}/${input.page}.png`,
            '--against', `${SHOTS}/${input.page}_Empty.png`],
          { cwd, timeoutMs: 10 * MINUTE })
        return { text: report(`measure ${input.page}`, r) }
      }
      if (name === 'lookdev') {
        const out: string[] = []
        const order: Array<[string, () => Promise<Run>]> = [
          ['params', steps.params],
          ['atlases', steps.atlases],
          ['build', steps.build],
          ['pages', runPages],
          ['import', steps.import],
          ['check', steps.check],
        ]
        for (const [label, step] of order) {
          await $.ui.status(`plants: ${label}`)
          const r = await step()
          out.push(report(label, r))
          // `check` reporting faults is information, not a reason to stop the loop.
          if (r.exitCode !== 0 && label !== 'check') {
            await $.ui.status(undefined)
            return { text: `${out.join('\n\n')}\n\nstopped: ${label} failed` }
          }
        }
        out.push(await captureAll(input.scope ?? 'species'))
        out.push(report('sheet', await steps.sheet()))
        await $.ui.status(undefined)
        return { text: out.join('\n\n') }
      }
      return next(e)
    } catch (error) {
      await $.ui.status(undefined)
      return { text: `${name}: ${(error as Error).message}` }
    }
  }).catch(($, e, next) =>
    // This hook answers tool calls itself, so it is a gate: skipped on a throw, the
    // call would go on to a tool that does not exist. Where the hook had not reached
    // next, refuse with the reason; where it had, replay what that call settled to.
    next.called
      ? next(e)
      : { deny: `montezuma-plants could not run ${String(e.tool)}` },
  )
}
