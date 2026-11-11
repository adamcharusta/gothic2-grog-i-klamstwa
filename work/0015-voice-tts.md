---
id: 0015
title: NPC voice via TTS (Plan 6)
status: partial
date: 2026-11-11
agent: Claude Opus 5.5 (subagent-driven, tasks 1-8)
docs: [docs/05-ui-and-configuration.md, docs/06-benchmark-and-testing.md, docs/07-decisions.md, docs/plans/2026-11-11-07-voice-tts.md]
---

## Goal
Optional voice for every AI NPC line: OpenAI-compatible `/audio/speech` (OpenAI voices), plus an "original voices" mode that
clones the original Polish dub locally from the player's own game files.

## What was done
- Task 1: `[tts]` config section (provider, base_url, model, api_key with `GROGAI_TTS_API_KEY` and `[ai]` key reuse, timeout_s 5-60, cache_dir), redacted summary.
- Task 2: `aicore` TTS providers (`OpenAiTts`, `MockTts`, WAV-only, RIFF check, text truncation).
- Task 3: `voice` block on every NPC card (name + PL/EN style).
- Task 4: session voice path (audio before subtitle, cache for fixed lines, `tts` log line, `tts_*` summary fields, voice hints PL/EN, `pricing.json` `tts`).
- Task 5: original casting (`voice.original`) and `[tts] voices = original`.
- Task 6: `tools/voices/extract_voices.py` (VDF reader, IMA ADPCM decode, openedai-speech `voice_to_speaker.yaml`), exit codes 0/1/2/3; `.gitignore` and package checks (no `*.wav`/`*.vdf`).
- Task 7: engine hooks (WAV decode from memory, playback with subtitle, Esc stops voice, futures handed to the host before a quickload/new game tears the session down).
- Task 8: README EN + PL section "NPC voice", docs 00/04/05/06/07, plans README row 6, this report; hygiene test extended.
- Decisions 1-12 logged in `docs/07-decisions.md` (1-9 designer, 10-12 user).

## Files changed / created
- `README.md`, `README.pl.md` — "NPC voice (optional)" / "Głos NPC (opcjonalnie)": OpenAI setup, cost note, original voices tool, ini lines, GPU and `timeout_s = 60`, the notice about the original actors, openedai-speech being archived.
- `docs/00-vision.md`, `docs/04-architecture-opengothic.md`, `docs/05-ui-and-configuration.md`, `docs/06-benchmark-and-testing.md`, `docs/07-decisions.md`, `docs/plans/README.md`
- `tools/tests/test_hygiene.sh` — README and example ini `[tts]` checks
- `work/0015-voice-tts.md`, `work/INDEX.md`

## How it was verified
- `aicore` unit tests (doctest) and the bash tests (`bash tools/tests/run.sh`, incl. `test_voices.sh` with a synthetic VDF) pass.
- `extract_voices.py` was run on the user's real Gothic II install: 16/16 pirates, 21-29 s each, exit 0.
- Engine code was only syntax-checked locally (`tools\dev\engine-syntax.cmd`) and awaits CI.
- NOT verified: anything in the game. No real OpenAI TTS call, no run against a real openedai-speech/XTTS server, no playback, no timing on a real machine. The openedai-speech yaml format was checked against its README only.

## What's left / notes for the next agent
- Merge the PR, wait for the engine CI build, then playtest 3.
- Angus and Hank have no own lines in the original (dead characters), so they use the actor's generic lines and sound the same.
- openedai-speech is archived; the README says so. A maintained local server could be tested later.
- `pricing.json` TTS price is an estimate.

## Playtest 3 checklist
Open playtest-2 fixes (work/0014), voice off:
- [ ] Malcolm and Owen stand at the lumber camp; NPCs do not send the player to an empty place.
- [ ] "Where is X" answers name the right place.
- [ ] Angus and Hank are on the camp beach.
- [ ] No camera glide on a new game.
- [ ] "Nowy wpis w dzienniku" notice with its sound on a new journal entry.
- [ ] The first quest "Porozmawiaj z Gregiem" is in the log at the start.

Voice, OpenAI (`[tts]` provider openai_compatible):
- [ ] Each pirate sounds different.
- [ ] The subtitle lasts as long as the audio.
- [ ] Esc stops the voice.
- [ ] A wrong `[tts]` key shows one hint and the game goes on with subtitles.
- [ ] `summary-<seed>.json` has `tts_cost_usd` (and the other `tts_*` fields); `session-*.jsonl` has `tts` lines.
- [ ] Quickload / new game during a dialog does not crash.

Original voices (`voices = original`, local server):
- [ ] `extract_voices.py` finds clips for all 16 pirates (exit 0).
- [ ] Greg and Bones sound like original actor 1.
- [ ] Note how long one line takes on this machine (GPU or CPU) and whether `timeout_s = 60` is enough.
