---
id: 0001
title: Design and documentation (brainstorming with the user)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/00-vision.md, docs/07-decisions.md, docs/08-open-questions.md]
---

## Goal
Agree the mod's design with the user and write the design docs, AGENTS.md, CLAUDE.md, and the `work/` reporting system.

## What was done
- Design conversation with the user (in Polish); decisions recorded in `docs/07-decisions.md`.
- Reviewed OpenGothic source (cloned to a temp folder outside the project): console text field, DialogMenu,
  Daedalus externals in `gamescript.cpp`, no HTTP client, no Ikarus/LeGo, single-byte bitmap fonts.
- Wrote `docs/00`–`08`, `AGENTS.md`, `CLAUDE.md`, `work/INDEX.md`, `work/_TEMPLATE.md`.
- An independent builder-check agent reviewed the docs; its questions were either settled (fonts, build order)
  or moved to `docs/08-open-questions.md`.
- At the user's request, all docs were rewritten in English (initial version was Polish).

## Files changed / created
- `docs/*.md`, `AGENTS.md`, `CLAUDE.md`, `work/*`

## How it was verified
Docs reviewed by the builder-check agent. OpenGothic was not built or run.

## What's left / notes for the next agent
- Open questions answered (see `docs/08-open-questions.md`).
- Next step after approval: implementation plan in `docs/plans/`.
- Design approved by the user on 2026-11-07. Git repo initialised; public GitHub repo created: https://github.com/adamcharusta/gothic2-grog-i-klamstwa (branches `master`, `develop`).
