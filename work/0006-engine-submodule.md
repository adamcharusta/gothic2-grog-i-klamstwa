---
id: 0006
title: OpenGothic fork as engine submodule (Plan 1, Task 2)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/engine-upstream.md, docs/04-architecture-opengothic.md]
---

## Goal
Fork OpenGothic to the user's account and mount it as the `engine/` submodule.

## What was done
- Fork `adamcharusta/OpenGothic` created (user approved), branch `grog/main` cut from upstream `master` f3330c8 and set as default.
- Submodule `engine` → `https://github.com/adamcharusta/OpenGothic.git`, `branch = grog/main`.
- `docs/engine-upstream.md`: upstream SHA table and how to merge upstream; added to legends in AGENTS.md/CLAUDE.md.

## Files changed / created
- `.gitmodules`, `engine` (submodule), `docs/engine-upstream.md`, `AGENTS.md`, `CLAUDE.md`, `tools/tests/test_hygiene.sh`

## How it was verified
Submodule assertions failed before (2 failures), pass after; full suite green.
Locally the submodule is initialised non-recursively (engine's own submodules not fetched); CI uses `submodules: recursive`.

## What's left / notes for the next agent
- Engine is unmodified. AI hooks come in Plan 3.
