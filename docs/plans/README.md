# Implementation roadmap

The design (docs/00–09) is split into plans that each produce working, testable software on their own.
Only the next plan is written in detail; later plans are written when the previous one is done,
because they depend on what was learned (see `work/`).

| # | Plan | Delivers | Status |
|---|------|----------|--------|
| 1 | [Foundation & CI/CD](2026-11-07-01-foundation-ci.md) | Repo hygiene files, VERSION, OpenGothic fork as submodule, CI build of the engine, beta/release packages (zip + installer) | written |
| 2 | AI core library | Engine-independent C++ library `aicore/`: config, HTTP, LLM clients (Anthropic + OpenAI-compatible), mock provider, scenario roller + clue graph, world state, prompt assembly (actor/director), command parser/validator, telemetry; unit tests in CI | not written |
| 3 | Engine integration — vertical slice | Fork hooks: AI dialogue mode (text field + "End"), NPC speaks arbitrary text, dynamic quest log, camp-only world setup, Greg → Skip → Matt playable on one scenario type with one provider | not written |
| 4 | Full campaign | All camp NPC cards PL/EN, lore base, 4 scenario types, story items, director + live events, endings and summary screen | not written |
| 5 | Persistence & release | Save/load of AI state, failure handling polish, benchmark aggregation script, README PL/EN with provider guides, first public release | not written |
