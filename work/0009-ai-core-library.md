---
id: 0009
title: AI core library aicore + grogai-cli (Plan 2, Tasks 1-9)
status: done
date: 2026-11-07
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/plans/2026-11-07-02-ai-core.md, docs/03-game-master.md, docs/04-architecture-opengothic.md]
---

## Goal
Engine-independent C++20 library for the AI Game Master, fully tested without the game or paid API calls.

## What was done (one commit per task)
1. Scaffold: `aicore/` CMake (nlohmann/json 3.11.3, doctest 2.4.11 via pinned FetchContent), `Result<T>`/`Error`, `version()`, UTF-8 safe file io (`pathFromUtf8`), CI job in `_build.yml` (`checks`: gcc build + ctest), local `tools/dev/aicore.cmd` (MSVC).
2. `parseConfig` for `GrogAI.ini` (anthropic / openai_compatible / mock, env `GROGAI_API_KEY`, director_model, validation, key redaction).
3. `IHttpClient` (WinHTTP on Windows, stub elsewhere), providers: Anthropic Messages API, OpenAI-compatible chat/completions (no auth header when no key, e.g. Ollama), `MockProvider`; HTTP errors → Auth / RateLimit / Server / BadResponse, never echoing the body.
4. `Rng` (xoshiro256** + splitmix64, golden values), `rollScenario` (4 types, culprit, motive, two node-disjoint witness chains, liars, gossip), `isSolvable`; 500-seed property test incl. burning any single witness.
5. `WorldState` (items, facts, NPC memory/attitude/knowledge, quests, events, monster) with versioned JSON (`kWorldSaveVersion = 1`).
6. NPC cards (`mod/data/npc/greg|skip|matt.json`, PL+EN), lore, rules; `buildActorRequest` (only the NPC's own knowledge) and `buildDirectorRequest` (full truth); property test: unaware NPCs never see the truth.
7. `parseActorReply`/`parseDirectorReply` (tolerant of prose and code fences, strict JSON inside), `validate`/`apply` for the full command catalogue, event and monster limits, `breaksCharacter`.
8. `Telemetry` JSONL (session, request, verdicts, events; no keys; content logging switch; cost from `mod/data/pricing.json`).
9. `GameSession` (talk / talkAsync / direct / farewell, fallback lines, one retry for bad or out-of-character replies) and `tools/grogai-cli`.

## Files changed / created
- `aicore/**`, `tools/grogai-cli/**`, `tools/dev/aicore.cmd`, `mod/data/**`, `.github/workflows/_build.yml`, `tools/tests/test_workflows.sh`

## How it was verified
- Each task: test written first, watched fail (link errors / failed assertions), then pass. Final: `tools\dev\aicore.cmd` → 100% (MSVC 14.51, Debug); `bash tools/tests/run.sh` → all passed.
- `grogai-cli` run with `provider = mock`: chat with Greg and Matt, /director, /bye, JSONL log written.
- NOT verified locally: gcc/Linux build (CI will), WinHTTP against a real provider (needs the user's key).

## What's left / notes for the next agent
- User: run grogai-cli with a real key (instructions in the PR / chat).
- `mod/data/pricing.json` has no prices on purpose; fill from provider price lists.
- Only greg/skip/matt have cards; scenarios may involve others → Plan 4 adds all cards. Plan 3 can pin a slice seed.
- One combined report for Plan 2 instead of one per task (all tasks landed in one session/PR).
