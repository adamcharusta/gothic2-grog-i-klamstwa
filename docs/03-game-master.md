# 03 — The Game Master (AI layer)

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Core rule [P]

**The AI proposes, the game decides.** The model never changes game state directly. It returns a structured reply
(spoken line + list of commands), and game code **validates** every command against rules and world state.
Invalid commands are rejected and logged. The game carries on.

## Two AI roles [P]

1. **Actor (NPC)**: answers in conversation as one specific character. Receives **only what that character knows**:
   their card, their slice of the secret (their role: witness, liar…), memory of talks with the player, gossip that reached them,
   current state (time of day, recent events). It never gets the full truth, so it **cannot leak it** however hard the player pushes.
2. **Director (GM)**: knows the full truth and the clue graph. Called:
   - periodically (e.g. every few game minutes) and after significant events (fight won/lost, clue found,
     item obtained, path to the truth lost);
   - decides on events, new leads, side threads, changes to NPC knowledge ("Matt heard you're looking for him").
   - May also do nothing.

[B] Whether the roles are always separate calls, or can sometimes be merged, is decided by measurement (cost/latency).
Hard constraint: the actor's context never contains the full truth.

## Reply format [P]

Actor reply (example; the builder defines the schema [B]):

```json
{
  "speech": "A tankard? Do I look like a dishwasher? Get lost.",
  "gesture": "angry",
  "actions": [
    {"type": "attitude", "npc": "matt", "delta": -1},
    {"type": "quest_entry", "quest": "tankard", "text": "Matt is hiding something. He drinks from a tankard like Greg's."}
  ],
  "end_dialog": false
}
```

Preferred mechanism: provider **tool calling / structured output**, with a fallback "JSON in text" mode and a tolerant parser
for models that don't support it.

## Command catalogue (first version) [P]

Conversation and characters:
- `quest_entry` (add/update a log entry), `quest_new` (side quest), `quest_close` (success/failure),
- `attitude` (NPC attitude to player), `npc_knowledge` (NPC learns a fact or rumour),
- `give_item` / `take_item` / `trade` (whitelisted items only, and only if the NPC actually has them),
- `start_fight` (knockdown duel with the player), `end_dialog`.

World events (live-manipulation test, [U]):
- `npc_goto` (NPC walks to a named point), `npc_approach_player` (NPC comes over and starts talking),
- `npc_fight_npc` (two NPCs brawl), `gather` (several NPCs at a point, e.g. the campfire),
- `spawn_monster` (whitelist, only at designated points beyond the palisade),
- `place_item` / `remove_item` (named spots only), `ambient_line` (NPC says something to someone nearby).

[B] Mapping commands to existing engine/script functions (OpenGothic binds ~200 Daedalus externals).
Named points (camp waypoints/freepoints) live in mod data.

## Rules enforced by code [P]

- The scenario truth cannot change. Greg's tankard can only be handed over by whoever **actually** holds it (world state).
- Key NPCs don't die. At most one spawned monster at a time. Event rate limit (e.g. 6–8/hour, [B] tune).
- A reachable path to the truth always exists (see [01](01-story-and-scenarios.md)).
- Replies that break character (mentions of AI, models, modern world) are rejected and retried or replaced with a fallback line [B: detection method].
- AI text is trimmed to a length limit (subtitles must fit on screen).

## Memory and context [P]

- A world fact log (who did what, fight results, who told whom what); excerpts go to actors.
- Per-NPC conversation memory: recent exchanges verbatim, older ones summarised.
- Context always includes: character card, speech style, role rules ("never break character…"), reply language.
- [P] System instructions in English (models follow them better), replies in the player's chosen language (PL/EN).
  Speech-style samples in both languages.

## Waiting and failures [P]

- Requests are **asynchronous**: the game never freezes.
- While waiting, the NPC plays a "thinking"/head-scratching animation; after ~2 s it may say a short filler ("Hmm…").
- Timeout (e.g. 20 s, configurable) → the character's fallback line ("My head hurts from the grog, say that again") and a retry.
- No network / bad key / exhausted quota: a clear in-game message (PL/EN), no crash, and a hint about what to check in the config.
- Director events simply don't fire when the AI is unavailable.

## Developer view [P]

Debug mode (config switch): shows the secret, clue graph, recent requests/replies, and rejected commands.
