---
id: 0012
title: Fixes from the first playtest (Plan 5a)
status: partial
date: 2026-11-11
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/plans/2026-11-11-05-playtest-fixes.md, docs/07-decisions.md]
---

## Goal
Fix what the user's first in-game playtest of beta.5 showed (OpenAI `gpt-4.1-mini`, ~$0.01 spent).

## Playtest result (user, 2026-11-11)
Works: typing, NPC answers in Polish, provider config. Problems reported: menu music plays on too long; NPCs don't take up brawls
(fights refused, delayed, or with weapons); Greg keeps accosting the player; NPCs lack character; no weapon at start; gold and
silver cups everywhere. Root causes (from `logs/session-*.jsonl`, `log.txt` and code) are in the plan's table.

## What was done
- aicore: tolerant command arguments (`fact`→`text`, nested `items`, string/float `delta`); `npc_approach_player` needs an
  opening `text`, 10-game-minute cooldown per NPC, the line goes into the NPC's memory; `ambient_line` needs an `npc`; director
  told the game language; events-per-hour counter now resets every game hour (it never did); `lastApproachS` /
  `eventsHourStartS` saved (optional, old v2 saves load); pricing falls back from dated snapshot names; `start.json` `items`;
  items.json `game_names` / `game_texts`.
- mod data: tankards are mission items not present in Jarkendar; start kit (pirate sabre, 50 gold, 2 grog); OpenAI prices;
  `actor.md` / `director.md` list each command's exact fields, push character, brawls and "never invent who has the tankard".
- Engine (fork `grog/ai-slice`): `Npc_CheckInfo` false for AI pirates; `start_fight` / `npc_fight_npc` deferred until the
  dialogue closes, weapons put away (back after the knockout or 180 s), script `B_Attack(.., AR_NONE, 1)` with the old
  `ZS_ATTACK` push as fallback; director's approach line spoken first when the NPC opens the dialogue; story items renamed on
  creation (`initializeInstanceItem` hook); start kit given and sabre equipped; music theme cleared on world load.

## Files changed / created
- `aicore/src/{commands,prompt,session,bridge,telemetry,world}.cpp`, headers — see above.
- `aicore/tests/test_playtest1.cpp` — 13 cases reproducing the log; old tests updated for the intended changes.
- `mod/data/{items,start,pricing}.json`, `mod/data/rules/{actor,director}.md`.
- `engine/common/grog/{groghost.cpp,groghost.h,dialogmenu_ai.cpp}`, `engine/common/game/gamescript.{cpp,h}`.
- `docs/plans/2026-11-11-05-playtest-fixes.md`, `docs/plans/README.md`, `docs/07-decisions.md`.

## How it was verified
- `tools\dev\aicore.cmd`: 107 test cases green (new tests failed first: compile errors on missing API, then the old
  behaviour). `bash tools/tests/run.sh`: all passed.
- Engine sources: MSVC syntax check (`tools\dev\engine-syntax.cmd`) of groghost.cpp, dialogmenu_ai.cpp, gamescript.cpp passes.
- NOT verified: CI engine build (MinGW), anything in game. In particular: whether `B_Attack` makes it a real knock-out brawl,
  whether clearing the theme shortens the menu music (hypothesis), the renamed items in the inventory, and whether the
  instance names `ITMI_CORAGONSSILBER`, `ITMI_BLOODCUP_MIS`, `ITMW_ADDON_PIR1HSWORD`, `ITFO_BOOZE` behave (they exist in the
  user's GOTHIC.DAT).
- Prices in pricing.json are list prices known to the agent, not checked against the provider today.

## What's left / notes for the next agent
- CI build, beta, second playtest (checklist: no cups on the ground count; Greg doesn't run after you; a challenged pirate
  brawls with fists right after the line; journal entries appear; director lines in Polish; sabre at start; cost > 0 in log).
- If NPCs still feel flat on `gpt-4.1-mini`, compare with `gpt-4.1` before rewriting cards.
- Reasoning models (gpt-5*, o3) need `max_completion_tokens` and no temperature in `llm.cpp` (not done).
