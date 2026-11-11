# Plan 5a — Fixes from the first playtest

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Fix what the user's first playtest of beta.5 (2026-11-11, `gpt-4.1-mini`) showed, so the next playtest can go through the whole story.

**Evidence:** `logs/session-1794363331982669900.jsonl` and `log.txt` from the user's install; user answers (music switches after a while; fights do not start, NPCs draw weapons, NPCs often refuse).

## Root causes found

| # | Symptom | Root cause |
|---|---------|------------|
| 1 | Gold and silver cups everywhere | Story items mapped to vanilla loot `ITMI_SILVERCUP` / `ITMI_GOLDCUP`; any cup the player picks up counts as Greg's tankard (`item_moved` triggers in the log). |
| 2 | Greg keeps accosting the player | AI dialogue bypasses script dialogues, so Greg's `important` infos are never marked as told; the perception script keeps sending him. The director also sent Matt three times with the same line. |
| 3 | NPCs have no character | Schema does not say which fields each command takes: all 3 `quest_entry` commands came with `fact` instead of `text` and were rejected; `attitude` came nested. Director gets no language (lines in English) and its `npc_approach_player` text is dropped by the engine; `ambient_line` without `npc` is rejected. Weak rules for brawls and for not inventing culprits. |
| 4 | No weapon at start | `start.json` only sets armour and level; starting straight in ADDONWORLD gives an empty inventory. |
| 5 | Fights: refused, delayed, with weapons | `start_fight` pushes `ZS_ATTACK` while the dialogue is still open and without the script's attack setup (`B_Attack`); NPCs draw their weapons. Rules do not tell pirates they like brawls. |
| 6 | Menu music plays on in game | Menu theme keeps playing until the first sound-zone update and a measure-timed transition. |
| 7 | (found) Director events stop after 8 | `eventsThisHour` is never reset. |
| 8 | (found) Cost logged as 0 | `pricing.json` has no models. |

## Global constraints
- Same as Plans 2–4 (aicore tests first, engine changes small and marked `// GROG:`).
- Engine changes are verified by CI build + user playtest only (no local engine build).

## Tasks

### Task 1: aicore — command arguments the models actually send
- [x] Tests: `quest_entry` with `fact` only → accepted, text = fact; `attitude` with `{"items":{"delta":1}}` or `"delta":"1"` → accepted; garbage still rejected.
- [x] `normalizeArgs` in `readActions`.
- [x] Prompts: per-command field list in `actor.md` / `director.md`; schema descriptions per field.

### Task 2: aicore — director language, opening lines, limits
- [x] Tests: director prompt names the language; `npc_approach_player` needs a `text` (opening line) and the same NPC is rejected again within 10 game minutes; accepted approach line is stored in that NPC's memory as its own words; `ambient_line` without `npc` rejected with a clear reason (prompt says it needs one); events counter resets after a game hour; bridge passes the approach text.
- [x] Implementation (`validate(..., gameTimeS)`, `WorldState::lastApproachS`, `eventsHourStartS`, JSON fields optional for v2 saves).

### Task 3: aicore/mod — characters and fights in prompts
- [x] `actor.md`: stay in character, vary reactions, change `attitude` rarely, pirates accept fist fights eagerly (`start_fight` + `end_dialog`), never invent who has the tankard.
- [x] Prompt test: rules text contains the brawl rule.

### Task 4: mod data — story items, start kit, pricing
- [x] `items.json`: real tankard / look-alike mapped to mission items that do not exist in Jarkendar, plus in-game names PL/EN; `start.json`: `items` (pirate sabre, gold); `pricing.json`: gpt-4.1(-mini), gpt-4o-mini, Claude.
- [x] Data loader tests.

### Task 5: engine (fork `grog/ai-slice`)
- [x] (implemented, MSVC syntax check; CI build + playtest pending) `npc_checkinfo` returns false for NPCs with an AI card (no accosting).
- [x] (implemented, MSVC syntax check; CI build + playtest pending) `start_fight`: close the dialogue, put the NPC's weapons away (fist fight), call script `B_Attack(npc, hero, AR_NONE, 1)`; weapons given back after the fight.
- [x] (implemented, MSVC syntax check; CI build + playtest pending) `npc_approach_player`: the NPC opens with the director's line.
- [x] (implemented, MSVC syntax check; CI build + playtest pending) Story items get their in-game names; start kit added in `setupWorld`.
- [x] (implemented, MSVC syntax check; CI build + playtest pending) World load: stop the menu theme and force an immediate music-zone update.

### Task 6: Release and playtest
- [ ] PR → develop → beta; user playtest; report in `work/`.
