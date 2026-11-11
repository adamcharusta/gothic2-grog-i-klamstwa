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
