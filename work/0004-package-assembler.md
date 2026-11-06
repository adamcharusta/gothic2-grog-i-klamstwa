---
id: 0004
title: Package assembler, launcher and READMEs (Plan 1, Task 4)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/09-repo-ci-and-versioning.md]
---

## Goal
`tools/package.sh` builds `GrogIKlamstwa-<v>-win64/` + `.zip` from an engine build, refusing unsafe content.

## What was done
- Layout: `bin/` (engine build), `system/GrogIKlamstwa.ini` (converted to Windows-1250, `Version=` filled), `Start-GrogIKlamstwa.bat` (forced CRLF), `GrogAI.example.ini`, `VERSION`, READMEs, `LICENSES/`.
- Exit 3: missing `Gothic2Notr.exe` or `engine/LICENSE`. Exit 4: key files, original game files (`*.vdf *.mod *.zen *.dat`, `Data/`, `_work/`) or `sk-...` keys in text files.
- Zip via `zip`, `7z`, or Python `zipfile` (whichever exists).
- `README.md` (EN) and `README.pl.md` (PL), `packaging/THIRD_PARTY.md`, `packaging/Start-GrogIKlamstwa.bat`.

## Files changed / created
- `tools/package.sh`, `tools/tests/test_package.sh`, `packaging/*`, `README.md`, `README.pl.md`

## How it was verified
`test_package.sh` failed first (23 failures), passes after (25/25); whole suite green. Paths with spaces and `ł` used throughout.
The launcher `.bat` was not executed (needs the game) — covered by the user smoke test in Task 7.

## What's left / notes for the next agent
- `[FILES]` in the manifest is empty until Plan 3 adds world/scripts.
