---
id: 0011
title: Campaign logic - handover, endings, fights, director clock, save/load (Plan 4)
status: partial
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/plans/2026-11-07-04-campaign-logic.md]
---

## Goal
Make a playthrough finishable and persistent.

## What was done
- aicore: `take_item` / `end_game` commands (Greg only; `returned` needs the real tankard, `fooled`/`caught` the look-alike), reward key handed over; AI sees neutral item handles (`dented_pewter_tankard` / `plain_pewter_tankard`), never internal ids; `syncPlayerItems`, `reportFight` (culprit never burned by fights), director clock (`directorDue`, `trigger`, `directAsync`; interval `director_interval_s`, min gap 30 s, no overlap, auto `unsolvable` trigger), `ending()`, `summary()`, `saveState`/`loadState` (world v2 + rng + clock + stats; answers in flight discarded via a generation counter).
- Engine (fork `grog/ai-slice`): hero inventory synced at dialog start; TakeItemFromPlayer / EndGame actions (on-screen message, `summary-<seed>.json`); `Npc::onNoHealth` hook reports player fights; director ticked every frame and its actions executed; AI state saved in the save archive as `grog/state.json` and restored after load (older saves: fresh AI + hint).

## How it was verified
- `test_campaign.cpp` (13 cases) written first; all aicore tests green on MSVC. The load-while-pending test was confirmed to fail with the generation guard disabled.
- Engine sources pass the MSVC syntax check; CI build pending.
- NOT verified in game.

## What's left
- Playtest. Gestures/animations for AI lines; summary screen UI (currently on-screen message + JSON file).
