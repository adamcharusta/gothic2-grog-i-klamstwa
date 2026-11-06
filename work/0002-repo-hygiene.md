---
id: 0002
title: Repo hygiene files, VERSION and bash test harness (Plan 1, Task 1)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/09-repo-ci-and-versioning.md, docs/plans/2026-11-07-01-foundation-ci.md]
---

## Goal
Add `.gitattributes`, `.editorconfig`, `.gitignore`, `VERSION`, `LICENSE`, `GrogAI.example.ini` and a bash test harness.

## What was done
- LF by default, CRLF for `*.bat *.cmd *.sln *.vcxproj* *.iss`, Daedalus scripts not re-encoded, Gothic/binary formats marked binary.
- `.gitignore` covers build output, secrets (`GrogAI.ini`, `*.local.ini`, `.env`), original game files, runtime logs/saves, `.superpowers/` agent scratch.
- `VERSION` = `0.1.0`; MIT `LICENSE`; sample AI config.
- `tools/tests/lib.sh` (assert helpers), `tools/tests/run.sh` (runs all `test_*.sh`), `tools/tests/test_hygiene.sh`.

## Files changed / created
- `.gitattributes`, `.editorconfig`, `.gitignore`, `VERSION`, `LICENSE`, `GrogAI.example.ini`, `tools/tests/*`

## How it was verified
`bash tools/tests/run.sh` failed before the files existed (25 failures) and passes after (29/29).

## What's left / notes for the next agent
- Run all repo tests with `bash tools/tests/run.sh`.
