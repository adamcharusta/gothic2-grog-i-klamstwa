# Grog i kłamstwa (Grog and Lies)

*[Polska wersja](README.pl.md)*

A mini-campaign for **Gothic II: Night of the Raven**, running on a fork of [OpenGothic](https://github.com/Try/OpenGothic).
It takes place in the pirate camp in Jarkendar. Greg's favourite tankard has gone missing, and you have to find it.

What makes it different: an **AI Game Master** runs the story. You don't pick dialogue lines, you **type your own**,
and the pirates answer in character. Every playthrough rolls a different background and a different culprit.

> Status: early development. Packages are built automatically: betas from `develop`, releases from `master`.

## Requirements

- A legal copy of **Gothic II: Night of the Raven** (e.g. Steam or GOG). The mod contains no original game files.
- Windows 10/11 x64 and a GPU with Vulkan or DirectX 12 support (OpenGothic requirement).
- An API key for an AI provider (see below).

## Installation

**Installer:** run `GrogIKlamstwa-<version>-setup.exe`, point it at your Gothic II folder
(for Steam usually `C:\Program Files (x86)\Steam\steamapps\common\Gothic II`), and start the game from the created shortcut.

**Zip:** unpack anywhere, put your Gothic II folder path into `gothic_path.txt` next to `Start-GrogIKlamstwa.bat`,
then run `Start-GrogIKlamstwa.bat`.

## AI configuration

Copy `GrogAI.example.ini` to `GrogAI.ini` and fill in the provider, model and API key. Never share `GrogAI.ini`.
Put it next to `Start-GrogIKlamstwa.bat` (zip install) or in `%APPDATA%\GrogIKlamstwa\` (recommended for the installer, since Program Files is not writable). Logs go to `logs\` next to the mod or to `%APPDATA%\GrogIKlamstwa\logs`.

In game: walk up to a pirate and talk as usual. Type your line and press Enter; **Enter on an empty line**, Esc or **END** finishes the conversation.

### Getting an API key

You pay the AI provider directly. A whole session with a small model costs cents: the first playtest cost about $0.01 with `gpt-4.1-mini`.
Most providers need a payment method or prepaid credit, even for a small amount.

Paste the key into `api_key =` without quotes. You can also set the environment variable `GROGAI_API_KEY` and leave `api_key` empty.
`base_url` is required for every provider except Anthropic.

**Anthropic (Claude):** get a key at https://console.anthropic.com/settings/keys (Settings → API keys).
```ini
provider = anthropic
model = claude-sonnet-5-5
```

**OpenAI:** get a key at https://platform.openai.com/api-keys (it starts with `sk-proj-`). To see which models your project can use, open Settings → Project → Limits.
```ini
provider = openai_compatible
base_url = https://api.openai.com/v1
model = gpt-4.1-mini
```
Reasoning models (`gpt-5*`, `o3`, `o4-mini`) also work, but they are slower and cost more per line.

**DeepSeek:** get a key at https://platform.deepseek.com/api_keys.
```ini
provider = openai_compatible
base_url = https://api.deepseek.com
model = deepseek-chat
```

**OpenRouter** (many models with one key): get a key at https://openrouter.ai/keys. Model names look like `vendor/model`.
```ini
provider = openai_compatible
base_url = https://openrouter.ai/api/v1
model = openai/gpt-4.1-mini
```

**Google Gemini:** get a key at https://aistudio.google.com/apikey.
```ini
provider = openai_compatible
base_url = https://generativelanguage.googleapis.com/v1beta/openai
model = gemini-2.5-flash
```

**Ollama** (runs on your computer, free, no key): install Ollama, run `ollama pull qwen2.5:14b`, and keep Ollama running while you play.
```ini
provider = openai_compatible
base_url = http://localhost:11434/v1
model = qwen2.5:14b
api_key =
```

**LM Studio** (runs on your computer, free, no key): load a model and start the local server in the Developer tab.
```ini
provider = openai_compatible
base_url = http://localhost:1234/v1
model = <the model name LM Studio shows>
api_key =
```

Two more settings:
- `director_model`: a cheaper model from the same provider for the Game Master.
- `director_interval_s`: how often the Game Master steps in (default: every 240 game seconds).

## NPC voice (optional)

The pirates can speak their answers aloud. Voice is **off by default**; without it you get the subtitles only, exactly as before.
It is configured in the `[tts]` section of `GrogAI.ini` (see `GrogAI.example.ini`). The hero's own lines are not voiced.

With voice on, an NPC answer appears only when the audio is ready, so it takes longer: about a second more with OpenAI,
several seconds with a local server on a CPU. The subtitle lasts as long as the audio, and Esc stops the voice. A quickload or
a new game during a conversation waits for the running voice requests before it loads.

### OpenAI voices

If you already use an OpenAI key in `[ai]`, add this section (the `[ai]` key is reused when the `base_url` is the same):

```ini
[tts]
provider = openai_compatible
base_url = https://api.openai.com/v1
model = gpt-4o-mini-tts
```

Every pirate gets his own OpenAI voice and a style text (a gruff drunk old pirate, a nervous young one, and so on).
Pirates who share an original voice actor share an OpenAI voice too. Cost: `mod/data/pricing.json` assumes about
$0.015 per minute of audio (`tts.gpt-4o-mini-tts.usd_per_minute`). That is an estimate: check OpenAI's current price list.
A local OpenAI-compatible server (for example Kokoro-FastAPI) can be used too: put its address in `base_url`, and set `model`
to a model that server offers. The pirates ask for OpenAI voice names (`onyx`, `ash`, ...), so the server must accept them
(or map them to its own voices). Kokoro has other voice names and no Polish voice, so check its docs first.

### Original voices (local cloning, advanced)

With `voices = original` the pirates talk in voices cloned from the original Polish dub of Gothic II. These voices speak
**Polish only** (the generated configuration uses `language: pl`), so use this mode with `[game] language = pl`.
Cloning happens **only on your PC, from your own legal copy of the game**. The mod ships no voice data. No cloud cloning service is used or supported.
The voices belong to the original voice actors: **never share or publish the extracted clips or the cloned lines.**

You need a local OpenAI-compatible text-to-speech server that maps a voice name to a reference WAV and clones it
(XTTS-v2 does that). The example below uses [openedai-speech](https://github.com/matatonic/openedai-speech). Note that this project
is archived (since 2026-01-04) and its author calls it mostly obsolete. Any server with a `/v1/audio/speech` endpoint that maps a voice
name to a reference WAV works the same way; openedai-speech is only the one whose configuration the tool below writes.
Its reference audio is capped at 30 seconds.

1. Extract the reference clips from your game (needs Python 3). `--out` is the `voices` folder of the server:

   ```
   python tools/voices/extract_voices.py --gothic "C:\Program Files (x86)\Steam\steamapps\common\Gothic II" --out "<openedai-speech folder>\voices"
   ```

   It reads `Data\Speech*.vdf`, writes `<pirate>.wav` for each of the 16 pirates (about 20 s each, change with `--seconds`) and a
   `voice_to_speaker.yaml`. Exit code 0 = all 16 pirates, 1 = bad arguments, 2 = no `Data\Speech*.vdf` found,
   3 = some pirates got no clip (they are listed on stderr). With exit code 3 the other pirates' clips and the yaml are still
   written, but the pirates without a clip are missing from the yaml, so the server cannot voice them: the game shows the
   voice hint and turns the voice off for the rest of the session. Fix the extraction before you play with this mode.
   Angus and Hank have no lines of their own in the original game, so they use the generic voice of their actor and sound
   the same.
2. Copy `voice_to_speaker.yaml` to the server's `config\voice_to_speaker.yaml` (or merge it into yours) and start the server.
3. Set in `GrogAI.ini`:

   ```ini
   [tts]
   provider = openai_compatible
   base_url = http://localhost:8000/v1
   model = tts-1-hd
   voices = original
   timeout_s = 60
   ```

   Use the port your server listens on. A GPU is recommended: XTTS on a CPU takes seconds per line, and each
   answer waits for it. Keep `timeout_s = 60` for slow machines.

After re-extracting the clips or switching to another server, delete the `tts_cache` folder (next to the mod, or
`%APPDATA%\GrogIKlamstwa\tts_cache`): it keeps old farewells and fallback lines in the old voices.

### Voice problems

If the voice does not work, the game shows one hint (wrong `[tts]` key, no credit, server unreachable, request rejected,
server not returning a 16-bit WAV) and goes on with subtitles only. After a rejected key or request, or after two failed requests
in a row, the voice stays off until you load a save or start a new game, so a dead server does not slow down every answer.
Each request is logged as a `tts` line in `logs\session-<seed>.jsonl` (latency, characters, audio length, cost).
`summary-<seed>.json` has `tts_requests`, `tts_errors`, `tts_audio_s`, `tts_cost_usd` and `tts_latency_avg_ms`.
Fixed lines such as farewells and fallback lines are cached in `tts_cache\`.

## Tested models

| Provider | Model | Result |
|----------|-------|--------|
| OpenAI | `gpt-4.1-mini` | Playtest 1 (beta.5): dialogue works, about $0.01 per session. Commands were sometimes malformed (fixed in beta.6). |
| Others | — | Not tested yet |

Weaker models and small local models get game commands wrong more often: journal entries go missing and fights don't start.
They also slip out of character more easily. If the pirates feel flat, try a larger model.

## Troubleshooting

When something is wrong with the AI, the game shows a one-line hint:
- *The API key was rejected*: `api_key` is wrong or revoked.
- *The AI provider refuses requests*: you hit the rate limit, or the account has no credit (add billing or credit).
- *Cannot reach the AI provider*: there is no internet, or `base_url` is wrong. For Ollama or LM Studio, check that the program is running.
- *GrogAI.ini error*: the provider doesn't have that model name, or the `base_url` path is wrong.
- *GrogAI.ini not found*: put the file next to `Start-GrogIKlamstwa.bat` or in `%APPDATA%\GrogIKlamstwa\`.

If an AI reply fails, the pirate says a ready-made line instead, so the game keeps going. Details are in `logs\session-<seed>.jsonl`.
The API key is never written there.

## Benchmark logs

Every session writes two files:
- `logs\session-<seed>.jsonl`: every AI request, with its latency, tokens and cost.
- `logs\summary-<seed>.json`: ending, play time, cost, latency and the share of rejected commands.

To compare sessions or providers, run (needs Python 3):

```
python tools/bench/aggregate.py logs
```

## Licence

MIT, see [LICENSE](LICENSE). Third-party components: [packaging/THIRD_PARTY.md](packaging/THIRD_PARTY.md).
