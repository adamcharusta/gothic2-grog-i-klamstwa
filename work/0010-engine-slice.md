---
id: 0010
title: Engine vertical slice - AI dialogue in game (Plan 3, Tasks 1-5)
status: partial
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/plans/2026-11-07-03-engine-slice.md]
---

## Goal
Talk to the pirates in the real game with typed lines; AI answers as subtitles and drives quest log, items and fights.

## What was done
1. Data: cards for all 16 pirates (PL+EN), NotR instance names read from the user's `GOTHIC.DAT`, place waypoints, `items.json`, `start.json`; `test_data.cpp`.
2. `aicore/bridge`: UTF-8 <-> cp1250 (table generated from Python's codec), punctuation to ASCII, instance lookup, verdicts -> `EngineAction`; `test_bridge.cpp`.
3. Engine fork branch `grog/ai-slice`: CMake links `../aicore` (GROG_AI); `common/grog/groghost.*` (session lifecycle on new/load game, config in mod root or `%APPDATA%\GrogIKlamstwa`, logs fallback, world setup in ADDONWORLD: hero to `ADW_PIRATECAMP_CENTER`, light pirate armour, level, missing cast inserted, story items placed; action execution: quest log, item transfer, `ZS_ATTACK` fights, goto, approach, spawn, ambient line).
4. `common/grog/dialogmenu_ai.cpp` + hooks in `DialogMenu`: text input, async answer, subtitles without audio, "KONIEC"/Esc/empty Enter -> farewell; pending answers adopted by GrogHost when the dialog closes; NPCs without a card keep original dialogues.
5. Packaging: `mod/data` in the package, manifest `[SETTINGS] WORLD=ADDONWORLD.ZEN`; README PL/EN usage notes.
- Dev tool `tools/dev/engine-syntax.cmd` (MSVC `/Zs` over engine sources, no Vulkan SDK needed).

## How it was verified
- aicore ctest 100% (MSVC); repo bash tests green; engine sources pass the MSVC syntax check with no warnings.
- CI (MinGW full build) on PR #4: see PR.
- NOT verified: anything in the running game (needs the user's playtest, Task 6).

## What's left / notes for the next agent
- User playtest checklist (Plan 3, Task 6).
- AI state is not saved yet: loading a save starts a fresh AI session (Plan 5).
- Director loop not triggered in game yet (Plan 4). Gestures/animations for AI lines not implemented.
- Stashed user change in engine: `git -C engine stash list` (stray 'p' in gamemusic.h).
