# Plan 3 — Engine integration: vertical slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Start the mod from the installer, wake up in the Jarkendar pirate camp as a pirate, walk up to any pirate, type a line, and get an in-character AI answer as a subtitle; the AI's accepted commands update the quest log, move items and start fist fights.

**Architecture:** All decision logic that can be tested without the game goes into `aicore` (new `bridge` module: code-page conversion, NPC instance ↔ card id, verdicts → engine actions). The engine fork gets a thin `common/grog/` layer: `GrogHost` (owns `GameSession`, lifecycle hooks) and an AI mode in `DialogMenu`. `aicore` is linked into the engine via `add_subdirectory`.

**Tech Stack:** aicore (Plan 2), OpenGothic fork (C++20, Tempest UI, ZenKit scripts), MinGW CI build.

**Spec:** docs/02, docs/03, docs/04, docs/05; facts read from the user's NotR scripts (2026-11-07): NPC instances `PIR_1320_ADDON_GREG`, `PIR_1350_ADDON_FRANCIS`, `PIR_1351_ADDON_SAMUEL`, `PIR_1352_ADDON_ALLIGATORJACK`, `PIR_1353_ADDON_MORGAN`, `PIR_1354_ADDON_HENRY`, `PIR_1355_ADDON_SKIP`, `PIR_1356_ADDON_BILL`, `PIR_1357_ADDON_GARETT`, `PIR_1362_ADDON_BONES`, `PIR_1365_ADDON_MATT`, `PIR_1366_ADDON_BRANDON`, `PIR_1367_ADDON_OWEN`, `PIR_1368_ADDON_MALCOM`, `PIR_1370_ADDON_ANGUS`, `PIR_1371_ADDON_HANK`; items `ITMI_SILVERCUP`, `ITMI_GOLDCUP`, `ITKE_GREG_ADDON_MIS`, `ITAR_PIR_H_ADDON`, `ITAR_PIR_L_ADDON`; waypoints `ADW_PIRATECAMP_CENTER`, `ADW_PIRATECAMP_GREG`, `ADW_PIRATECAMP_BEACH_*`, `ADW_PIRATECAMP_HUT*`, …; mod manifest `[SETTINGS] WORLD=` selects the start world.

## Global Constraints

- Engine changes only in `engine/common/grog/**` plus minimal hooks (marked `// GROG:`) in `CMakeLists.txt`, `ui/dialogmenu.*`, `gothic.cpp`/`mainwindow.cpp`. Engine code style = upstream (2-space, braces as upstream).
- Engine commits go to fork branch `grog/main` via the main-repo PR (submodule pointer bump); commit messages Conventional Commits.
- Text to the engine is converted UTF-8 → cp1250 (`bridge::toGameText`), text from the engine cp1250 → UTF-8. Characters outside cp1250 are transliterated.
- Files at runtime, relative to the exe dir `bin/`: config `../GrogAI.ini`, data `../mod/data`, logs `../logs`. No file is ever written into the Gothic folder except the mod manifest.
- AI never blocks the frame: only `talkAsync`; `farewell` is local.
- Without a valid config the game still starts; talking shows the NPC's fallback line plus a one-time on-screen hint (PL/EN) about `GrogAI.ini`.

## Review Focus

1. A Polish line typed in-game (ą, ł, ż) reaches the AI as correct UTF-8 and the answer is displayed with Polish letters on the Polish install — tests in Task 2 (conversion round-trip) and user playtest.
2. Pressing "Koniec"/Esc while an answer is still pending must not crash or show the answer in a later dialog — Task 4 (pending result is dropped when the dialog closes).
3. An NPC with no card (e.g. a bandit wandering in) must keep working with its original dialogues — Task 4 (AI mode only for mapped instances).
4. Missing/broken `GrogAI.ini` or `mod/data` must not stop the game from starting — Task 3.
5. Item commands must use the real engine inventory (no duplication): giving Greg's tankard removes it from the NPC — Task 5.

---

### Task 1: Full cast data (cards for every pirate, instances, items, waypoints)

**Files:** `mod/data/cast.json` (+`instance`), `mod/data/places.json` (+`waypoint`), `mod/data/items.json` (new), `mod/data/start.json` (new), `mod/data/npc/*.json` (13 new cards), `aicore/tests/test_data.cpp`.

- [ ] Test: every cast NPC has a card and an `instance` matching `^PIR_\d+_ADDON_[A-Z_]+$`; every place has a `waypoint` starting with `ADW_`; `items.json` maps `greg_tankard`, `tankard_lookalike`, `key_reward_chest`, `captain_outfit` to instances; `start.json` has `world`, `waypoint`, `armor`, `level`. RED → write data (cards in the style of the three existing ones, PL+EN, based on each pirate's role in NotR) → GREEN.
- [ ] `loadGameData` loads `items.json`/`start.json` into `GameData::items` (`id → instance`) and `GameData::start`. Commit `feat(data): cards and engine ids for the whole pirate cast`.

### Task 2: aicore `bridge`

**Files:** `aicore/include/grog/ai/bridge.h`, `aicore/src/bridge.cpp`, `aicore/tests/test_bridge.cpp`.

**Produces:**
- `std::string toGameText(std::string_view utf8)` (cp1250; unknown chars transliterated, e.g. „ ” → ", – → -), `std::string fromGameText(std::string_view cp1250)`.
- `std::optional<std::string> npcIdForInstance(const GameData&, std::string_view instance)` (case-insensitive) and `instanceForNpc`.
- `struct EngineAction { enum Kind { QuestEntry, GiveItemToPlayer, StartFight, NpcGoto, NpcApproachPlayer, NpcFightNpc, SpawnMonster, AmbientLine } kind; std::string npcInstance, npc2Instance, itemInstance, waypoint, monsterInstance, text /* cp1250 */; }`
- `std::vector<EngineAction> toEngineActions(const std::vector<Verdict>&, std::string_view speaker, const GameData&)` — only accepted verdicts; unknown mapping → skipped.

- [ ] Tests: "Żółć ąęłńóśźż ĄĘŁŃÓŚŹŻ" round-trips; "„cytat” – x…" → `"cytat" - x...`; instance lookup case-insensitive; give_item `greg_tankard` → `GiveItemToPlayer{npcInstance=PIR_..., itemInstance=ITMI_SILVERCUP}`; rejected verdicts produce nothing; quest text converted to cp1250. RED → implement → GREEN. Commit `feat(aicore): engine bridge`.

### Task 3: Link aicore into the engine + `GrogHost`

**Files:** `engine/CMakeLists.txt` (hook), `engine/common/grog/groghost.h/.cpp`, `tools/package.sh` (+`mod/data`), `packaging/system/GrogIKlamstwa.ini` (+`[SETTINGS] WORLD=ADDONWORLD.ZEN`), `.github/workflows/_build.yml` (engine job checks out main repo so `../aicore` exists — already true), tests in `tools/tests/test_package.sh`.

- `GrogHost::inst()`: `bool enabled()`, `void onStartGame()` (load config/data, create providers + session; on failure stays enabled with `lastError` for the hint), `void onWorldLoaded(World&)` (Task 5), `GameSession* session()`, `std::string hint()`.
- CMake: `if(EXISTS ${CMAKE_SOURCE_DIR}/../aicore/CMakeLists.txt) add_subdirectory(../aicore aicore) target_link_libraries(Gothic2Notr PRIVATE aicore) target_compile_definitions(Gothic2Notr PRIVATE GROG_AI=1)`.
- [ ] Package test: zip contains `mod/data/cast.json` and manifest has `WORLD=ADDONWORLD.ZEN`. RED → GREEN.
- [ ] CI engine build green with aicore linked. Commit (fork + main repo) `feat(engine): link aicore and add GrogHost`.

### Task 4: AI dialogue mode

**Files:** `engine/common/ui/dialogmenu.h/.cpp` (hooks), `engine/common/grog/aidialog.h/.cpp`.

- When a dialogue starts with an NPC whose instance maps to a card: no script choices; an input line ("> " + text + caret) is drawn where choices are; one extra choice "Koniec"/"End" (Esc also ends).
- Enter: show the player's line as the player's subtitle, start `talkAsync`, show "..." and play a think gesture; poll in `tick`; on result show the NPC subtitle (duration 1.5 s + 60 ms/char, no sound), apply `toEngineActions`, print quest entries with the usual "new log entry" notice.
- End: NPC says `farewell()`; dialog closes after the line. A pending future is kept by `AiDialog` until it finishes and its result is discarded if the dialog was closed (Review Focus 2).
- Unmapped NPCs: unchanged original behaviour (Review Focus 3).
- [ ] Verified by CI build + user playtest (scripted checklist in the work report).

### Task 5: World setup and engine actions

**Files:** `engine/common/grog/worldsetup.cpp`, `engine/common/grog/actions.cpp`.

- On new game in ADDONWORLD: teleport hero to `start.waypoint`, equip `start.armor`, set level `start.level` (via script functions/`Npc` API), insert missing cast NPCs at their routine start, give the tankard item to its holder NPC (or insert it at the place's waypoint), give `key_reward_chest` to Greg, lookalike to Matt.
- Actions: QuestEntry → `QuestLog` topic "Kufel Grega"/"Greg's tankard" + entry; GiveItemToPlayer → move one item from NPC to hero inventory (only if the NPC has it); StartFight → NPC attacks the player (script `B_Attack(npc, hero, AR_NONE, 1)`); director actions are logged only (Plan 4).
- [ ] Verified by CI build + user playtest. Commit `feat(engine): world setup and AI actions`.

### Task 6: Playtest checklist and release

- [ ] Merge to `develop` → beta. User runs the checklist: start in camp; talk to Greg in Polish; get "Porozmawiaj ze Skipem" in the log; talk to Skip and Matt; win a fist fight; receive an item; End with farewell; Esc during a pending answer; bandit/other NPC still uses original dialogues; broken key → fallback line + hint. Record results in `work/`.
