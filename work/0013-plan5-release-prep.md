---
id: 0013
title: Failure handling, benchmark, README (Plan 5, Tasks 1-5)
status: partial
date: 2026-11-11
agent: Claude Opus 5.5 (Claude Code)
docs: [docs/plans/2026-11-11-06-persistence-release.md, docs/05-ui-and-configuration.md, docs/06-benchmark-and-testing.md]
---

## Goal
Make the mod usable by an ordinary player with an API key and measurable as a benchmark, up to the release.

## What was done
- Reasoning models (`o1`/`o3`/`o4`/`gpt-5*`): `max_completion_tokens` (≥ 2000), no `temperature`.
- Provider errors: HTTP 400/404 → `Config`; one retry for timeout/server/network; `TurnResult::hint` (PL/EN, once per
  error kind) shown in game after the fallback line.
- Summary: cost, tokens, latency avg/max, requests, rejected rate, provider/models; stats saved in saves (old saves load);
  the engine writes `summary-<seed>.json` also when the player quits without an ending (`summary` log event).
- `tools/bench/aggregate.py` (stdlib only): rows per provider/model with runs, turns, requests, errors, latency avg/p95/max,
  tokens, cost, rejected %, fallback %, endings; `--json`; skips broken lines; exit 2 without logs. Shipped in the package.
- README EN/PL: key pages and ini lines for Anthropic, OpenAI, DeepSeek, OpenRouter, Gemini, Ollama, LM Studio; tested
  models; troubleshooting; benchmark logs. Example ini: base_url required for openai_compatible.

## How it was verified
- `tools\dev\aicore.cmd`: 116 test cases green. The new session tests were confirmed to fail with the old session.cpp
  (4 failures), the rest of the new tests compiled only against the new API.
- `bash tools/tests/run.sh`: all passed (new `test_bench.sh`, README checks in `test_hygiene.sh`, package contains aggregate.py
  — the package check failed before the package.sh change).
- `aggregate.py` on the user's real playtest-1 log: 21 requests, 26 794 input tokens (≈ $0.013 at the listed price; the user
  saw ~$0.01).
- Engine: MSVC syntax check of groghost.cpp / dialogmenu_ai.cpp. CI build and in-game behaviour NOT verified.
- Provider URLs and example model names (Gemini, DeepSeek, OpenRouter, Ollama, LM Studio) are from the agent's knowledge, not
  tested with real keys.

## What's left / notes for the next agent
- Task 6: beta from develop, user playtest 2 (checklist in work/0012 + wrong key shows a hint, summary has cost > 0,
  `aggregate.py logs` prints a row), then version 0.1.0 and PR develop → master after the user's go-ahead.
- Branch `feature/plan5-release` is stacked on `fix/playtest-1` (PR #6): merge #6 first.
