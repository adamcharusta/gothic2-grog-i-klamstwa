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

Coming soon: step-by-step guides for Anthropic (Claude), OpenAI, DeepSeek, OpenRouter, Google Gemini and local models (Ollama, LM Studio).

## Licence

MIT, see [LICENSE](LICENSE). Third-party components: [packaging/THIRD_PARTY.md](packaging/THIRD_PARTY.md).
