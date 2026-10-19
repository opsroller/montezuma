<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About CI on every push

Every push to any branch of this repository runs the same checks a pull
request gets. Whoever pushes, a person or an agent, reads the result for
that exact commit from CI. Nobody has to reproduce the checks by hand to
learn whether a commit is good, and two people asking about the same
commit get the same answer.

## Why pushes, not only pull requests

Until this change, CI ran on pushes to `main` and on pull requests. A
branch with no PR yet got no runs at all, so work in progress was checked
only by whatever its author remembered to run locally, on whatever
machine and toolchain they had. Running on every push closes that gap:
the first push of a branch is checked on the same pinned runners, tools
and options as `main`.

## Why branch pushes have no path filter

GitHub matches a `push` path filter against the files changed by that one
push, not against everything the branch changes. A branch that touches a
level and then pushes a docs fix would get no `levels` or `tests` run on
its head, the commit that is reviewed and merged. So pushes run every
check. The exception is the `windows` workflow, whose 30-minute MSVC
build only matters when the build system changes: it keeps its filter on
`push` and also runs on `pull_request` for every PR, where the filter
covers the whole PR.

## Why a branch PR skips its own pull_request run

With both triggers, a PR from a branch of this repository would run every
check twice per push: once for the push and once for the PR. Every job
therefore skips on a `pull_request` event unless the PR comes from a fork
(`windows` aside, above).
The push run reports on the same head commit, and GitHub shows those
results on the PR, so each commit is checked once and runner time is not
spent twice. Forks still run on `pull_request`, because their pushes run
in the fork, not here.

The trade-off: a `pull_request` run tests the PR merged into `main`,
while a push run tests the branch as pushed. A branch that is behind
`main` can pass here and still conflict in behaviour with newer `main`.
Merging `main` into the branch before landing, and the run on `main` after
each squash merge, cover that case.

## Why Windows stays off feature branches

Feature-branch pushes use the same reduced matrix as PRs (Linux and macOS,
plus the `windows` workflow when the build system changes). `main` and
manual runs keep the full matrix, so a Windows-only break shows up on
`main` at the latest. See [CI pipeline](../reference/ci-pipeline.md) for
the triggers of each workflow.
