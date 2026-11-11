# Implementation roadmap

The design (docs/00–09) is split into plans that each produce working, testable software on their own.
Only the next plan is written in detail; later plans are written when the previous one is done,
because they depend on what was learned (see `work/`).

| # | Plan | Delivers | Status |
|---|------|----------|--------|
| 1 | [Foundation & CI/CD](2026-11-07-01-foundation-ci.md) | Repo hygiene files, VERSION, OpenGothic fork as submodule, CI build of the engine, beta/release packages (zip + installer) | done (beta.2 published; user smoke test pending) |
| 2 | [AI core library](2026-11-07-02-ai-core.md) | Engine-independent C++ library `aicore/`: config, HTTP, LLM clients (Anthropic + OpenAI-compatible), mock provider, scenario roller + clue graph, world state, prompt assembly (actor/director), command parser/validator, telemetry; unit tests in CI | done |
| 3 | [Engine integration — vertical slice](2026-11-07-03-engine-slice.md) | Fork hooks: AI dialogue mode (text field + "End"), NPC speaks arbitrary text, dynamic quest log, camp-only world setup, Greg → Skip → Matt playable on one scenario type with one provider | implemented, playtest pending |
| 4 | [Campaign logic](2026-11-07-04-campaign-logic.md) | All camp NPC cards PL/EN, lore base, 4 scenario types, story items, director + live events, endings and summary screen | implemented (logic tested; engine hooks need playtest) |
| 5a | [Playtest 1 fixes](2026-11-11-05-playtest-fixes.md) | Story items not vanilla loot, no accosting, fist fights, director limits and language, command field tolerance, start kit, pricing | implemented (aicore tested; engine needs CI + playtest) |
| 5 | [Failure handling, benchmark, README, release](2026-11-11-06-persistence-release.md) | Reasoning models, player-facing error hints + retry, summary with cost/latency, aggregation script, provider guides PL/EN, release 0.1.0 | implemented up to Task 5; playtest 2 and release pending (user) |
| 6 | [NPC voice (TTS)](2026-11-11-07-voice-tts.md) | Optional TTS voice for every AI NPC line (OpenAI-compatible `/audio/speech`), voice per pirate, cached fixed lines, cost in the benchmark | approved 2026-11-11, in progress |
