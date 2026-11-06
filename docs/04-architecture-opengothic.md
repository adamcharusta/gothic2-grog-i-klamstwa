# 04 — Technical architecture (OpenGothic fork)

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Engine

- [U] **OpenGothic** is mandatory: https://github.com/Try/OpenGothic (C++, CMake, MIT; submodule deps:
  Tempest, ZenKit, bullet3, dmusic, rapidjson, TinySoundFont).
- [P] We **fork** it with minimal, well-isolated changes so upstream updates can be merged.
  New code goes in its own directory (e.g. `common/aimod/`); existing files only get small hooks.

## Findings from the code review (2026-11-07)

- `common/ui/consolewidget.*`: an existing console with a text field. Template for the dialogue input field.
- `common/ui/dialogmenu.*`: the dialogue window (choices, subtitles, dialogue camera). Gets an "AI mode":
  text field + "End" button instead of the choice list.
- `common/game/gamescript.cpp`: registers ~200 Daedalus externals (`bindExternal`), incl. `ai_output`.
  Original `AI_Output` plays **named** lines (subtitle + audio file). We need a new path:
  **an NPC speaks arbitrary text** (subtitle + gesture/animation, no audio; TTS later).
- `common/game/questlog.*`: the quest log. AI entries must go in as dynamic text.
- **No HTTP client** in the engine; we add one (HTTPS, async). [B] Library choice
  (e.g. WinHTTP on Windows behind an interface ready for libcurl elsewhere, or cpp-httplib + TLS).
- OpenGothic **does not support Ikarus/LeGo**. We don't need them, the AI logic is in C++.
- Mods launch via a manifest: `Gothic2Notr.exe -game:<mod>.ini`.
- **Fonts** (found by the builder check): text uses the original game's single-byte bitmap fonts (`utils/gthfont.cpp`), no UTF-8.
  Polish edition fonts are cp1250; English/German are cp1252 (no ą, ę, ł…). The user tests on the **Steam edition with Polish
  texts** (cp1250 fonts). Decision (see [07](07-decisions.md)):
  - AI text is converted from UTF-8 to the install's code page (detected from the game's language/fonts [B]);
  - on a Polish install both PL and EN work with the original fonts;
  - on non-Polish installs, Polish diacritics are transliterated (ą→a, ł→l…). Shipping our own fonts with Polish glyphs
    is deferred to a later release.

## Save and load

[U] Saving and loading must work at any time, including mid-investigation.
[P] All AI-layer state is serialised together with the OpenGothic save (hook into `common/game/serialize.*`,
`gamesession`, `worldstatestorage`): seed and scenario, clue graph, world fact log, per-NPC memory and summaries,
story item locations, director timers and event counters, quest entries created by the AI.
- The AI-layer data block is **versioned**, so saves from older mod versions either load or fail with a clear message.
- Loading never re-rolls the scenario.
- Requests in flight at save time are dropped; after loading, the NPC simply waits for the player's next line.
- [B] Storage format (a section inside the save vs. a sidecar file next to it).
- This is a foundation for the full-Gothic project, so build it properly rather than as a hack.

## AI layer modules [P]

```
aimod/
  config      – load configuration (provider, model, key, language, limits)
  llm/        – clients: anthropic (native API), openai_compatible (OpenAI, DeepSeek, OpenRouter, Gemini-compat, Ollama, LM Studio)
  http        – async HTTPS client, request queue, timeouts
  scenario    – seeded scenario rolling, clue graph, solvability check
  worldstate  – world facts, NPC memory, story item state
  prompt      – context assembly (card, lore excerpt, memory, rules)
  actions     – command parser + validator, execution via engine API
  director    – director loop (timer, events → GM call)
  telemetry   – benchmark logs
  ui          – AI mode in DialogMenu, "thinking" indicator, summary screen
```

The AI provider interface is shared ("send conversation + command schema, get a reply"), so adding a provider is one file.

## Mod content

- [P] Campaign data in `mod/data/` (JSON/YAML + PL/EN texts): NPC cards, lore, scenario templates, map points,
  whitelisted items and monsters, fallback lines, prompts.
- [B] **Story items** (Greg's tankard, look-alikes, key, chest) and **disabling original INFO dialogues and quests** need either
  Daedalus script changes (compiled with an open compiler) or engine-side registration/blocking. The builder chooses.
  Designer preference: as little Daedalus as possible, and never redistribute original game files.
- [B] Start directly in the Jarkendar camp, remove unneeded NPCs/monsters.
- The mod **must not contain** original game assets (licence). We ship only our code, data and any changed scripts.

## Distribution [P]

- Package: built OpenGothic fork (Windows x64) + mod folder + `README` (PL/EN) + sample config file.
- The player points it at their Gothic 2 NotR install, as with regular OpenGothic.
- [B] Repo layout: OpenGothic fork as a submodule or a separate repo. Run `git init` for this project (the folder is not a repo yet).
