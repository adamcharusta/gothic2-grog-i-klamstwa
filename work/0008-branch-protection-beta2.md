---
id: 0008
title: Branch protection and beta 2 (Plan 1, Task 7)
status: partial
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/09-repo-ci-and-versioning.md]
---

## Goal
Protect `master`/`develop`; user smoke-tests the installer on a real Gothic install.

## What was done
- PR #2 merged; `v0.1.0-beta.2` published (setup 7.0 MB, zip 10.3 MB after stripping debug info).
- Branch protection on `master` and `develop`: PR required, checks `build / checks`, `build / engine`, `build / package`, no force push/deletion. Verified via `gh api .../branches/master/protection`.

## How it was verified
`gh api` returned the settings above.

## What's left / notes for the next agent
- **User smoke test pending**: install `GrogIKlamstwa-0.1.0-beta.2-setup.exe`, point at the Steam Gothic II folder, start from the shortcut; expected: unmodified OpenGothic main menu. Status becomes `done` when the user confirms.
