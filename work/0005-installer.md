---
id: 0005
title: Windows installer (Plan 1, Task 5)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/09-repo-ci-and-versioning.md]
---

## Goal
Inno Setup 6 script producing `GrogIKlamstwa-<v>-setup.exe` from the staged package folder.

## What was done
- `packaging/installer.iss`: installs to `{autopf}\GrogIKlamstwa`; extra wizard page asks for the Gothic II folder (default: Steam path if present) and requires `System\Gothic2.exe`; writes `gothic_path.txt`; copies the mod manifest into `<Gothic>\System` (removed on uninstall); Start-menu and optional desktop shortcuts run `bin\Gothic2Notr.exe -g "<gothic>" -game:GrogIKlamstwa.ini`; Polish and English UI.
- Script kept ASCII-only (Inno reads BOM-less files as ANSI).

## Files changed / created
- `packaging/installer.iss`, `tools/tests/test_installer.sh`

## How it was verified
Static test failed first (11 failures), passes after (13/13). The script was NOT compiled locally (no Inno Setup here); CI compiles it in Task 6.

## What's left / notes for the next agent
- Real install is checked by the user smoke test (Task 7).
