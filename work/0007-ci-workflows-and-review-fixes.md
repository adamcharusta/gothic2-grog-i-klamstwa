---
id: 0007
title: CI workflows, first beta, final review fixes (Plan 1, Task 6)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/09-repo-ci-and-versioning.md, docs/plans/2026-11-07-01-foundation-ci.md]
---

## Goal
GitHub Actions: CI on PRs, beta on `develop`, release on `master`; publish the first beta; fix final-review findings.

## What was done
- `_build.yml` (reusable: repo tests on Ubuntu, engine build on Windows/MSYS2 like upstream, package + Inno Setup installer), `ci.yml` (PRs only), `beta.yml`, `release.yml`, composite `setup-vulkan` action copied from upstream.
- CI fixes found on real runs: absolute paths for `iscc`; no duplicate push+PR builds; `strip --strip-debug` on shipped binaries (exe was 514 MB); merge commits excluded from release notes.
- PR #1 merged into `develop` → **v0.1.0-beta.1** published as a pre-release (zip 127 MB + setup 87 MB, before stripping).
- Final review (fresh reviewer) fixes: launcher reads `gothic_path.txt` as UTF-8 (`chcp 65001`), installer writes it as UTF-8; packager allowlists engine binaries and refuses game files / renamed key files / Gemini keys; tools resolve the repo from their own location; MinGW runtime licences listed.

## Files changed / created
- `.github/workflows/*`, `.github/actions/setup-vulkan/action.yml`, `tools/package.sh`, `tools/version.sh`, `tools/changelog.sh`, `packaging/*`, `tools/tests/*` (new `test_launcher.sh`, `test_workflows.sh`)

## How it was verified
- CI run 42317107195 green (engine build 13 min, package + installer produced); beta workflow green, release `v0.1.0-beta.1` has both assets.
- Local suite: 135 checks green; `test_launcher.sh` runs the real `.bat` under cmd.exe (skipped on Linux CI).
- NOT verified: installing/running on a real Gothic install (Task 7, user).

## What's left / notes for the next agent
- Deferred minors are listed in the PR #2 description / ledger; candidates for a later polish task.
- `engine/common/gamemusic.h` had an uncommitted stray `p` in the local working copy (not ours) — left untouched, user informed.
