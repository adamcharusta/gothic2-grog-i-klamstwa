# Plan 4 — Campaign logic (engine-independent part) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Everything the campaign needs to be finishable — handing the tankard back, the three endings, fights feeding the story, the Game Master running on a clock, and saving/loading the AI state — implemented and tested in `aicore`, with thin engine hooks.

**Architecture:** New `GameSession` methods are the only API the engine calls: `syncPlayerItems`, `reportFight`, `directorDue`/`directAsync`, `ending()`, `summary()`, `saveState`/`loadState`. New commands `take_item` and `end_game` go through the same validator. Engine hooks (inventory sync on dialog start, fight result, director tick, save/load) are small and land in the fork.

**Spec:** docs/01 (endings), docs/03 (director, rules), docs/04 (save and load), docs/06 (summary).

## Global Constraints
- Same as Plan 2 (namespace, `Result`, no throws across the API, UTF-8, tests first).
- Story items: `greg_tankard`, `tankard_lookalike`, `key_reward_chest`, `captain_outfit` (items.json).
- Director interval default 240 s game time (`[ai] director_interval_s`), min 30 s between calls; event triggers: `fight`, `item_moved`, `unsolvable`.
- `kWorldSaveVersion` bumps to 2 (ending, director clock, fight log); version 1 saves are rejected with a clear message.

## Review Focus
1. The player hands Greg the look-alike: the game must not grant the real ending — Task 2.
2. A save made mid-conversation loads without the pending answer and without duplicating memory — Task 5.
3. The Game Master is never called twice at once and never more often than the minimum gap — Task 4.
4. Beating up the culprit must not make the truth unreachable (the culprit holds the tankard) — Task 3.
5. `end_game` from anyone but Greg, or before he holds a tankard, is rejected — Task 2.

---

### Task 1: Player items in world state and prompts
- `void GameSession::syncPlayerItems(const std::set<std::string>& storyItemsOnPlayer)`: items listed → holder `player`; items whose holder was `player` but are no longer listed → holder `lost`.
- Actor prompt line "The player carries: …" (story items only, display names from items.json `name.pl/en`).
- `take_item` command (actor): accepted only if the player holds the item; apply → holder = speaker; bridge → `EngineAction::TakeItemFromPlayer`.
- Tests: sync moves items; take_item accepted/rejected; prompt mentions carried tankard for Greg.

### Task 2: Endings
- `end_game {outcome}` (only speaker `greg`): `returned` requires Greg holds `greg_tankard`; `fooled`/`caught` require Greg holds `tankard_lookalike`; `gave_up` always. Accepted `returned`/`fooled` → `key_reward_chest` moves to player (+ engine GiveItemToPlayer).
- `std::optional<Ending> GameSession::ending()`; `nlohmann::json summary()` (seed, type, culprit, location, outcome, play time, turns, director calls, rejected commands, tokens).
- Greg's card/rules: he inspects any tankard he is given; the real one is pewter with a dent and a carved anchor (lore `grog`).
- Tests: Review Focus 1 and 5; key moves on returned; summary fields.

### Task 3: Fights feed the story
- `void reportFight(npc, bool playerWon, gameTimeS)`: fact known by camp, `knockedOutByPlayer`, attitude −1 for the loser NPC (but culprits never become "burned" by a fight alone), director trigger `fight`.
- Tests: fact added; Review Focus 4 (isSolvable still true after beating the culprit).

### Task 4: Director clock
- `bool directorDue(int64_t gameTimeS)`, `std::future<DirectorResult> directAsync(trigger, t)`, `void trigger(std::string)`; internal: last call time, in-flight flag, queued triggers (deduplicated), interval and min gap from config; `unsolvable` trigger raised automatically when `isSolvable(world, burned)` turns false.
- Tests: Review Focus 3 with a fake clock; triggers fire before the interval; in-flight blocks a second call.

### Task 5: Save / load
- `nlohmann::json saveState()` (world + rng state + director clock + counters), `Result<bool> loadState(const nlohmann::json&)`; `kWorldSaveVersion = 2`.
- Rng gains `state()`/`setState()`.
- Tests: round-trip equality; Review Focus 2 (save while a talkAsync is pending → saved memory excludes the in-flight turn; after load the late result is discarded via a session generation counter).

### Task 6: Engine hooks (fork)
- Dialog start: collect story item instances in hero inventory → `syncPlayerItems`.
- TakeItemFromPlayer action; ending → on-screen message + summary written to logs; key handed over.
- Fight result: hook where an NPC is knocked out by the player / knocks the player out → `reportFight`.
- `GrogHost::tick`: `directorDue` → `directAsync`; execute accepted director actions when ready.
- Save/load: write `saveState()` next to the save slot as `<slot>.grog.json`; read it on load (fallback: fresh session + hint).
- Verified by CI build + user playtest.
