<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Operations

*Reference. How the team's work measures on cost, flow and quality, the
waste found in each operations review and what each review cost. The CI
workflows these measures read are in [CI pipeline](ci-pipeline.md); why
CI runs on every push is in
[About CI on every push](../explanation/about-ci-on-every-push.md). The
plan the flow measures cover is in [Work packages](work-packages.md).*

## Measures

Values are for the window of the latest review in the
[review log](#review-log). Baseline is the value from the previous review;
the first review has none, so its trends are unknown. Target is the level
the team aims for, where one is set. Lead time runs from PR opened to PR
merged. A review round is one review that gives a Changes needed or
Passed verdict.

| Measure | Value | Baseline | Target | Trend | Paired with | Source |
|---|---|---|---|---|---|---|
| PRs merged | 78 | none | none | unknown | spend | `gh pr list --state merged` |
| PR lead time, merged | median 30 min over 78 PRs; longest #11 11h52m, #16 11h33m, #7 11h30m, #18 11h13m | none | none | unknown | review rounds | `gh pr list --state merged` |
| Open PR age at 15:56Z | #61 18h28m, #72 14h56m, #78 8h21m, #86 6h20m, #88 5h47m | none | median lead time | unknown | bench wait | `gh pr list --state open` |
| Review rounds per PR | #78, #81, #86, #88 4 each; #80, #83, #91 2 each | none | 2 or fewer | unknown | PR lead time | review verdicts on each PR |
| CI runs with real jobs per docs-only push | 5 (tests, assets, levels, build, web) | none | none | unknown | CI queue depth | `gh run list` |
| CI queue depth at 15:52Z | 62 queued, 9 in progress of the last 200 runs | none | none | unknown | runs per docs-only push | `gh run list` |
| CI run outcomes, last 200 runs | 43 success, 51 skipped, 35 cancelled, 71 not finished | none | none | unknown | pushes per PR | `gh run list` |
| Spend | not measured | none | none | unknown | throughput | no usage records reachable |

Data gaps: no usage or spend records, no gap log in the repository and no
Actions billable minutes per run.

## Findings

| Id | Finding | Size over the window | Owner | Verdict |
|---|---|---|---|---|
| OR1-F2 | Bench-result and docs-only pushes start the full CI on push. The missing push path filter is deliberate, so a docs-only head is still checked. | About 40 working runs for 8 docs-only pushes, 15:34Z to 15:44Z on 2026-10-19 | devops-engineer | weakened: count corrected from 5 to 8 pushes |
| OR1-F3 | Reviewed PRs waited for the matched Mac mini M4 benchmark before merging. Cause (bench blocked on machine permission prompts) is inferred from thread timing. | #78, #86, #88: about 4h each, 11:05Z to 15:40Z on 2026-10-19 | project-manager (bench queueing); it-support (standing machine permissions) | weakened: 3 PRs, not 5; cause inferred |
| OR1-F4 | Re-review loops: docs findings arrived round by round, and fix commits arrived without their docs. | #78, #81, #86 and #88 4 rounds each, against a target of 2 or fewer | code-reviewer (all docs findings in round 1); authoring roles (docs with each fix commit) | weakened: cause shared with authors |
| OR1-F1 | Push and pull_request both run on each PR commit. | Same-repo pull_request jobs skip by design, so near-zero runner time | devops-engineer | dropped |
| OR1-F5 | Author threads idle after green gates. | Threads were active 15:34Z to 15:44Z and no gates were green | project-manager | dropped |

## Review log

| Review | Window | Investigators | Independent check | Cost |
|---|---|---|---|---|
| OR1 | 2026-10-18T04:00Z to 2026-10-19T15:56Z | 0 | engineering-operations reviewer, 16 tool calls | about 88k reviewer tokens; reviewer's own session cost not measured |
