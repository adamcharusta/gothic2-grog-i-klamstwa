---
id: 0003
title: Version and changelog tools (Plan 1, Task 3)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/09-repo-ci-and-versioning.md]
---

## Goal
`tools/version.sh` (release / beta / dev strings, tag guard) and `tools/changelog.sh` (Conventional Commits → Markdown).

## What was done
- `version.sh release` prints `VERSION`, exits 2 if tag `v<VERSION>` exists locally or on `origin`; `beta <n>` / `dev <n>` add suffixes; VERSION is trimmed of spaces/CR.
- `changelog.sh [<from-ref>]` groups commits into Features / Fixes / Other / Maintenance. `build`, `test`, `style` also go to Maintenance (in addition to `docs`, `chore`, `ci`).

## Files changed / created
- `tools/version.sh`, `tools/changelog.sh`, `tools/tests/test_version.sh`, `tools/tests/test_changelog.sh`

## How it was verified
Tests failed before implementation (16 failures), pass after (`bash tools/tests/run.sh` → all passed). Tests run in a temp repo whose path contains spaces, `ł` and `–`.

## What's left / notes for the next agent
- Used by the CI workflows (Task 6).
