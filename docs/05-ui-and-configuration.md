# 05 — Player UI and AI configuration

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Conversation

- [U] The dialogue has **only a text input field**, no clickable suggestions.
- [U] An **"End"** button stays, as in the original. On click the character says their own goodbye in their style
  (AI-generated or from a fallback pool, [B]).
- [P] Conversations start the standard way (walk up + action key). Dialogue camera and subtitles as in the original.
- [P] The player's line also appears as a subtitle over the hero (as in Gothic), then the NPC replies.
- [P] Recent exchanges stay visible (scrollable) so the player doesn't lose the thread.
- [P] Input length limit (e.g. 300 chars). The field must accept Polish characters.
- [P] A "the NPC is thinking" indicator while waiting.

## Quest log

- [U] Leads show up in the log as quests ("Talk to Skip").
- [P] Main quest "Greg's tankard" + GM side quests. Entries are dynamic (AI text, length-validated).

## AI configuration [U]

The player picks the provider and model with their own API key. The README explains how to get a key and where to put it.

[P] A file in the mod folder (e.g. `GrogAI.ini` or `.json`, [B]):

```ini
[ai]
provider = anthropic          ; anthropic | openai_compatible
base_url =                     ; empty = provider default; e.g. https://api.deepseek.com, http://localhost:11434/v1
model = claude-sonnet-5-5
api_key =                      ; or environment variable GROGAI_API_KEY
director_model =               ; optional different (cheaper) model for the director
timeout_s = 20
temperature = 0.8

[game]
language = pl                  ; pl | en
seed =                         ; empty = random
debug = false

[telemetry]
log_dir = logs
```

- [P] Default provider: Claude (Anthropic). README covers Anthropic, OpenAI, DeepSeek, OpenRouter, Google Gemini (OpenAI-compatible endpoint),
  local Ollama and LM Studio.
- [P] README lists the models the mod was tested with, and warns that weaker models get commands wrong more often.
- The API key **never** goes into logs or crash dumps.
- [P] A simple "Test AI connection" screen in the menu, or at least a clear message at startup if the config is broken.

## NPC voice (TTS) [P]

Optional, off by default. A separate `[tts]` section (the full commented block is in `GrogAI.example.ini`):

| Key | Meaning |
|-----|---------|
| `provider` | `off` (default) or `openai_compatible` (`POST {base_url}/audio/speech`, WAV only) |
| `base_url` | Required when on, e.g. `https://api.openai.com/v1` or a local server `http://localhost:8000/v1` |
| `model` | `gpt-4o-mini-tts` (OpenAI); `tts-1-hd` for a local cloning server |
| `api_key` | Or `GROGAI_TTS_API_KEY`. Empty and the same `base_url` as `[ai]`: the `[ai]` key is reused |
| `timeout_s` | 5-60 (clamped). Use 60 for a local server on a CPU |
| `cache_dir` | Default `tts_cache`; holds only fixed lines (farewells, fallbacks) |
| `voices` | `openai` (default): OpenAI voice name + style text from the NPC card. `original`: the request carries the NPC id as the voice name, no style text; for a local cloning server fed by `tools/voices/extract_voices.py` |

- An NPC answer is shown only when text and audio are both ready; the subtitle lasts as long as the audio; Esc stops the voice.
- The hero's typed lines and director `ambient_line` prints are not voiced.
- A broken `[tts]` config or a failing request never blocks the game: subtitles only, one hint per error kind (PL/EN).
- The `[tts]` key is never logged. Original voices: clips are extracted on the player's PC from their own copy
  (`Data\Speech*.vdf`), nothing is uploaded, the mod and package ship no voice data (the README says the voices belong to the original actors and must not be shared).

## Languages [U]

PL and EN: mod UI, README, fallback lines, character style samples. NPCs speak the language set in the config.
[P] On a Polish game install (incl. the user's Steam copy) both languages display fully. On non-Polish installs Polish diacritics
are transliterated; custom fonts come later (see [04](04-architecture-opengothic.md)).
