# 00 — Vision: "Grog i kłamstwa" (Grog and Lies)

Status: approved by the user · Date: 2026-11-07

Tags used in all docs: **[U]** user decision (do not change without asking) · **[P]** designer proposal (may change, log it in `07-decisions.md`) · **[B]** left to the builder.

## What the mod is

A mini-campaign for **Gothic 2: Night of the Raven (NotR)**, running on **OpenGothic** (mandatory, our fork).
It is confined to the **pirate camp in Jarkendar** plus a few metres beyond the palisade.
The player (the Nameless Hero in pirate clothes) searches for **Greg's lost favourite tankard**.

**The hook:** the plot, side quests and events are driven by an **AI Game Master**, like a pen-and-paper RPG session:
the GM has a main storyline but adapts to the player's decisions. The player does not pick prepared lines,
they **type their own**. Characters answer in line with Gothic lore and their personality.

## Why (most important)

The mod is a **benchmark / proof of concept** for the real goal: the whole of Gothic driven by AI the same way. Therefore:

- Everything campaign-specific (characters, lore, scenario, the tankard) lives in **data**, not engine code.
  Scaling to full Gothic should mean adding data, not rewriting the AI layer.
- The mod must **measure** how well it works: logs, latency, cost, rejected AI commands, provider comparison.

## Goals

1. A **30–60 minute** playthrough that is worth replaying: each run rolls a different background and culprit.
2. Free-text conversation with every pirate, in Polish or English.
3. The AI changes the game live: quest log, items, attitude, fights, NPC movement, random events.
4. The player chooses the AI provider and model (Claude, OpenAI, DeepSeek, OpenRouter, local Ollama/LM Studio…) with their own API key.
5. The story is always completable, and the game never crashes on AI or network errors.

## Non-goals (this release)

- Voice acting / TTS (next release; the architecture should leave room for it).
- Clickable suggested lines (only a text field and an "End" button).
- The rest of Gothic's world, other camps, multiplayer.
- Ikarus, LeGo, Union support.
- Our own AI hosting or proxy server: the player connects directly to their provider.

## Anti-goals (failure even if it technically works)

- Pirates talk like a polite chatbot instead of drunk Gothic cutthroats.
- The AI breaks character, mentions the outside world, or reveals the secret when the player asks.
- A reply takes more than a few seconds with nothing happening (the NPC should at least visibly "think").
- The story gets stuck because the AI lost a clue and the tankard can no longer be found.
- The game crashes on no internet, API error, exhausted quota, or a nonsense model reply.

## Fixed requirements

- A legal copy of **Gothic 2 Night of the Raven** [U] (OpenGothic uses the original game files).
- Target platform: **Windows** first [P]. Do not break OpenGothic's portability.
- Languages: **PL and EN** [U] (UI, README, character speech). Project docs and code are in English [U].
