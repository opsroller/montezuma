<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Cut a release

How to publish version `X.Y.Z` as a GitHub release with every package
and notes made from the commit titles. Run from the repository root on an
up-to-date `main`. For what the release workflow does and what it
publishes, see [Build pipeline: Releases](../reference/build-pipeline.md#releases).

## Choose the version

Look at the notes the release would get:

```sh
python3 tools/release/changelog.py notes
```

Bump `X` for a section under **Breaking changes**, `Y` for any
**Features**, otherwise `Z`. Before 1.0.0, breaking changes bump `Y`.

## Update the changelog

1. On a branch, regenerate `CHANGELOG.md` with the new section:

   ```sh
   git switch -c release/vX.Y.Z origin/main
   python3 tools/release/changelog.py changelog --next X.Y.Z --out CHANGELOG.md
   ```

2. Commit it as `chore(release): vX.Y.Z`, open a PR and squash-merge it
   once it is green. Commits titled `chore(release): ...` are left out
   of the notes.

## Tag it

Tag the merge commit on `main` and push the tag:

```sh
git switch main && git pull --ff-only
git tag -a vX.Y.Z -m "Montezuma's X.Y.Z"
git push origin vX.Y.Z
```

The **release** workflow builds all four targets, smoke-tests them and
publishes the release. A tag that matches `v*.*.*` but is not a plain
`vX.Y.Z` (such as `v1.2.3-rc.1`), or is not on `main`, stops at its first
job and builds nothing; a tag like `v1.2` starts no run at all.

If `publish` fails on something passing, such as a network error, re-run
the failed jobs from the run's page. A draft release an interrupted run left behind is
finished, not duplicated. A release that is already published is never
changed; the job says how to delete it first.

## Publish from your own machine

Only when GitHub Actions cannot run: build the packages yourself and
publish them with the same script. Pushing the tag still queues the
**release** workflow, so cancel that run if Actions comes back
(`gh run list --workflow release.yml`, then `gh run cancel <id>`);
otherwise both try to publish and the second one fails.

1. Build the packages of the tagged commit into `dist/` as in
   [Export a desktop build](export-a-desktop-build.md) and
   [Export the web demo](export-a-web-build.md). Check out the tag first
   so each package is named `montezuma-X.Y.Z-...`. The Windows and macOS
   libraries cannot be built on Linux; without them, publish only the
   targets you have with `--only`, for example `--only linux,web`.

2. Check what would be published:

   ```sh
   python3 tools/release/publish.py --tag vX.Y.Z --dry-run dist
   ```

   It verifies every package against its `.sha256`, refuses a missing
   target or a package of another version, and writes
   `dist/release/notes.md` and `dist/release/SHA256SUMS`. Read the notes.

3. Push the tag (as above), then publish, signed in with `gh auth login`:

   ```sh
   python3 tools/release/publish.py --tag vX.Y.Z dist
   ```

## Undo a release

```sh
gh release delete vX.Y.Z --cleanup-tag --yes
```

Fix the problem on `main`, then tag the fixed commit. Prefer a new
`Z` over reusing a version someone may already have downloaded.
