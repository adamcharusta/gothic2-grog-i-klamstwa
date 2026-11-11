---
id: 0014
title: Playtest 2 - Malcolm not found, camera glide on new game, no "new log entry" notice, first quest
status: partial
date: 2026-11-11
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/02-characters-and-world.md, docs/07-decisions.md]
---

## Goal
Playtest 2: Malcolm was the witness, NPCs kept sending the player to him, but he could not be found.

## What was done
- Cause, from the user's log (`D:\Mod\GrogIKlamstwa\logs\session-1794411776344096000.jsonl`) and the original
  `GOTHIC.DAT` (read with a throwaway parser, not committed):
  - Malcolm's start routine is `RTN_PRESTART_1368`: alone at the lumber camp outside the palisade (`ADW_PIRATECAMP_WAY_15`
    and `LUMBER_SIT`). He only dies after Henry's vanilla dialogue, which we don't use. Owen's start routine is
    `RTN_POSTSTART_1367` (drinking on the beach), so the lumberjack pair was split.
  - Angus and Hank start in the waterhole cave (`ADW_PIRATECAMP_WATERHOLE_CAVE`), Alligator Jack at
    `ADW_ENTRANCE_2_PIRATECAMP_01` on the path to the camp.
  - Far NPCs are not simulated, so the director's `npc_approach_player` for Malcolm (accepted) did nothing.
  - The AI did not know where anyone is: Greg invented "he lurks by my hut".
- `cast.json`: every cast member has `hangout` {pl,en}; `routine` for owen/malcolm (`RTN_PRESTART_1367/1368`, the original
  pair routine: one saws while the other sits by the fire), alligator_jack (`ZS_STAND_ARMSCROSSED` at `ADW_PIRATECAMP_WAY_07`,
  his own midday spot), angus/hank (`ZS_SMALLTALK` at `ADW_PIRATECAMP_BEACH_06`, where Brandon chats by day).
- aicore: `CastMember::hangout` and `CastRoutine` (loader rejects `state` without `waypoint`); actor prompt gets
  "# Where people usually are" in the game language (without the speaker), director prompt gets the English list by id.
- Engine (`grog/ai-slice`): `GrogHost::applyRoutine` on a new game: script routine via `excRoutine`, or one state at one
  waypoint all day (`Npc::clearRoutines` + two `addRoutine`), then `resetPositionToTA` so the NPC is there at once.
  Missing function/state/waypoint or a dead NPC is logged and skipped.
- Camera glided from the original spawn to the hero on a new game: `setupWorld` teleports the hero after
  `GameSession` placed the camera. Now `camera()->reset(hero)` right after the setup.
- AI quest entries had no "new log entry" notice like the original: `GameScript::grogQuestEntry` now calls the game's own
  `B_LogEntry` (checked in `GOTHIC.DAT`: `Log_AddEntry` + `PrintScreen(PRINT_NewLogEntry, -1, YPOS_LogEntry,
  FONT_ScreenSmall, 2)` + `Snd_Play("LogEntry")`), so text and sound are the localized originals. Without that script
  function it falls back to the old plain entry + `onPrint`.
- First quest: `start.json` `first_quest` {pl,en} ("Porozmawiaj z Gregiem..."). `GameSession` puts it in `world.quests` on a
  new game (in the config language, so the director sees it too; a loaded save replaces it), and `setupWorld` writes the
  session's quest entries into the game log under "Kufel Grega" via `B_LogEntry` (notice + sound at game start).

## Files changed / created
- `mod/data/cast.json` — hangouts and routines.
- `aicore/include/grog/ai/scenario.h`, `aicore/src/scenario.cpp` — model and loader.
- `aicore/src/prompt.cpp` — hangout sections in both prompts.
- `aicore/tests/test_prompt.cpp`, `aicore/tests/test_scenario.cpp`, `test_data.cpp`, `test_playtest1.cpp` — 4 new test
  cases; `test_session.cpp` expects the first quest at index 0.
- `mod/data/start.json`, `aicore/include/grog/ai/session.h`, `aicore/src/session.cpp` — first quest.
- `engine/common/grog/groghost.{h,cpp}`, `engine/common/world/objects/npc.h` — routine override, camera reset.
- `engine/common/game/gamescript.{h,cpp}` — quest entries through `B_LogEntry`.
- `docs/02-characters-and-world.md`, `docs/07-decisions.md`.

## How it was verified
- New tests failed first (missing members), then `tools\dev\aicore.cmd`: 120/120 test cases green.
- `tools\dev\engine-syntax.cmd` (groghost, dialogmenu_ai, gamescript): passes. `bash tools/tests/run.sh`: all passed.
- NOT verified: CI build and in game (that Owen and Malcolm work at the lumber camp, Angus/Hank and Jack stand at the new
  spots, the director's approach now reaches the player, no camera glide, the log notice and sound appear, the first quest is in the log at start). Hangout texts for Francis, Garett, Skip, Jack are inferred from
  waypoint names. Only new games get the routines; old saves keep the old positions.

## What's left / notes for the next agent
- Playtest 3: find Malcolm and Owen at the lumber camp; ask any NPC "where is X"; Angus/Hank on the camp beach.
- Reported by the user, left for later: the start sabre `ITMW_ADDON_PIR1HSWORD` needs 50 strength, the hero has 10
  (pick a lighter start weapon or raise strength in `start.json`).
