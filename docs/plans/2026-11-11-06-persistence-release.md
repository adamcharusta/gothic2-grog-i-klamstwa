# Plan 5 — Failure handling, benchmark, README, release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. Steps use checkbox (`- [x]`) syntax.

**Goal:** Make the mod usable by an ordinary player with an API key (clear errors, working modern models, a README that
explains every provider) and measurable as a benchmark (summary with cost/latency, an aggregation script), then cut 0.1.0.

**Architecture:** Error-to-hint mapping, retries, the reasoning-model request format and the run summary live in `aicore`
(unit-tested); the engine only shows `TurnResult::hint`. The aggregation script is standalone Python in `tools/bench/`, tested by
a bash test on fixture logs. Save/load was already done in Plan 4.

**Tech Stack:** C++20 + doctest (aicore), Python 3 standard library only (aggregation), bash tests, Markdown.

**Spec:** docs/05 (AI configuration, errors), docs/06 (logs, summary, aggregation, success criterion), docs/09 (release flow).

## Global Constraints
- Same as Plans 2–4 (namespace `grog::ai`, `Result`, no throws across the API, UTF-8, tests first, engine hooks marked `// GROG:`).
- The API key never appears in logs, hints, summaries or the aggregation output.
- Aggregation script: Python 3.10+, standard library only, runs on the CI's `ubuntu-24.04`.
- README in both languages (README.md EN, README.pl.md PL) with the same content.
- The release (merge to `master`) happens only after the user's playtest 2 and the user's go-ahead.

## Review Focus
1. Wrong or expired API key: the player sees a clear hint naming GrogAI.ini once, not a fallback line per turn with no explanation — Task 2.
2. No internet / provider down mid-game: a short retry, then fallback line + one hint; the game never freezes — Task 2.
3. A player picks `gpt-5-mini` or `o3`: requests use `max_completion_tokens` and no `temperature`, so they don't fail with HTTP 400 — Task 1.
4. A log from an older beta (no `cost_usd`, or a truncated last line after a crash) does not break the aggregation script — Task 4.
5. A summary is written even when the player quits without an ending (outcome `playing`) — Task 3.

---

### Task 1: Reasoning models in the OpenAI-compatible client

**Files:** Modify `aicore/src/llm.cpp`, `aicore/include/grog/ai/llm.h`; Test `aicore/tests/test_llm.cpp`

**Interfaces:** Produces `bool isReasoningModel(std::string_view model)` (llm.h): true for names starting with `o1`, `o3`, `o4`,
`gpt-5` (case-insensitive), false otherwise (`gpt-4.1-mini`, `gpt-4o`, `deepseek-chat`, `claude-sonnet-5-5`).

- [x] Test `reasoning models get max_completion_tokens and no temperature`: request body captured by the fake HTTP client for model
  `gpt-5-mini` has `max_completion_tokens == req.maxTokens`, no `max_tokens`, no `temperature`; for `gpt-4.1-mini` the old body
  (`max_tokens`, `temperature`). Run `tools\dev\aicore.cmd` → FAIL.
- [x] Implement in `OpenAiCompatibleProvider::complete`; reasoning models also get `maxTokens*4` (min 2000) because hidden
  reasoning tokens count against the limit.
- [x] Run tests → PASS. Commit `feat(aicore): reasoning models in the OpenAI-compatible client`.

### Task 2: Player-facing error hints and one retry

**Files:** Modify `aicore/src/session.cpp`, `aicore/include/grog/ai/session.h`, `engine/common/grog/dialogmenu_ai.cpp`;
Create `aicore/src/hints.cpp`, `aicore/include/grog/ai/hints.h`; Test `aicore/tests/test_hints.cpp`

**Interfaces:**
- Produces `std::string playerHint(ErrorKind kind, Language lang)` (hints.h, UTF-8): non-empty for `Auth`, `RateLimit`,
  `Network`, `Timeout`, `Server`, `Config`; empty for others. PL/EN texts:
  - Auth: "Klucz API został odrzucony - sprawdź api_key w GrogAI.ini." / "The API key was rejected - check api_key in GrogAI.ini."
  - RateLimit: "Dostawca AI odrzuca zapytania (limit albo brak środków na koncie)." / "The AI provider refuses requests (rate limit or no credit on the account)."
  - Network: "Brak połączenia z dostawcą AI - sprawdź internet i base_url." / "Cannot reach the AI provider - check the internet connection and base_url."
  - Timeout: "Dostawca AI odpowiada zbyt wolno - spróbuj później albo zwiększ timeout_s." / "The AI provider is too slow - try later or raise timeout_s."
  - Server: "Dostawca AI ma awarię - spróbuj później." / "The AI provider has an outage - try later."
  - Config: "Błąd w GrogAI.ini: model lub ustawienia nie pasują do dostawcy." / "GrogAI.ini error: the model or settings don't fit the provider."
- `TurnResult::hint` (UTF-8): set by `GameSession::talk` the first time a given `ErrorKind` occurs in the session, empty otherwise.
  HTTP 400/404 from a provider map to `ErrorKind::Config` (check `statusError`).

- [x] Tests: `playerHint` for each kind in both languages contains no "api_key=" value and is non-empty where listed;
  `talk` with `scriptedErrors = {Auth, Auth}` over two turns → first result has hint, second has none, both use a fallback line;
  `scriptedErrors = {Timeout}` then a good reply → the turn succeeds (one retry for `Timeout`/`Server`/`Network`, none for
  `Auth`/`RateLimit`/`Config`), telemetry shows two requests. Run → FAIL.
- [x] Implement; the engine prints `toGameText(r.hint)` with `Gothic::inst().onPrint` in `grogTick` when non-empty
  (replaces the current `takeHint` call there).
- [x] Run tests + `tools\dev\engine-syntax.cmd` (via `subst`, the path has an en dash) → PASS. Commit `feat: player-facing AI error hints`.

### Task 3: Run summary with cost and latency

**Files:** Modify `aicore/src/session.cpp`, `aicore/include/grog/ai/session.h`, `aicore/src/telemetry.cpp`,
`aicore/include/grog/ai/telemetry.h`, `engine/common/grog/groghost.cpp`; Test `aicore/tests/test_campaign.cpp`

**Interfaces:**
- `Telemetry::request(...)` returns `double` cost (0 when unknown) so the session can sum it.
- `GameSession::summary()` gains: `cost_usd` (double), `input_tokens`, `output_tokens`, `latency_ms_avg`, `latency_ms_max`
  (successful requests, actor + director), `requests`, `rejected_rate` (rejected / all verdicts, 0 when none), `provider`, `model`,
  `director_model`. Existing keys stay.
- Engine: `GrogHost::onSessionExit` writes `summary-<seed>.json` if no ending wrote one (outcome `playing`).

- [x] Tests: scripted replies with known token counts and a Pricing with `{"mock": {1, 2}}` → `cost_usd` matches; latency avg/max
  from fake latencies; `rejected_rate` = 0.5 for one accepted + one rejected verdict; summary keys present with no ending.
  Run → FAIL.
- [x] Implement (stats accumulated under `mu_`, saved/loaded with `stats` in `saveState`; missing fields default to 0).
- [x] Run tests + engine syntax check → PASS. Commit `feat: run summary with cost and latency`.

### Task 4: Benchmark aggregation script

**Files:** Create `tools/bench/aggregate.py`, `tools/tests/test_bench.sh`, `tools/tests/data/bench/` (two small fixture
session logs + one summary, made from the playtest-1 log shape with content removed).

**Interfaces:** `python3 tools/bench/aggregate.py [--json] <log dir or files...>` reads `session-*.jsonl` and `summary-*.json`.
Groups by (provider, actor model). Text output: one row per group with `runs`, `turns`, `requests`, `errors`, `latency avg/p95/max
ms`, `tokens in/out`, `cost USD` (sum, and per run), `rejected %`, `fallback %`, `endings` (e.g. `returned:1 playing:1`).
`--json` prints the same as a list of objects. Exit 0 on success, 2 when no logs were found.

- [x] Test (`test_bench.sh`, uses `tools/tests/lib.sh`): fixture dir → exit 0, row for `openai_compatible gpt-4.1-mini` with
  `runs 2`, the expected request count and cost; a log with a truncated last line and no `cost_usd` fields is still counted;
  empty dir → exit 2; output never contains `api_key`. Run `bash tools/tests/test_bench.sh` → FAIL.
- [x] Implement (standard library only; unknown lines skipped and counted in a `skipped lines` footer).
- [x] Run `bash tools/tests/run.sh` → ALL TESTS PASSED. Commit `feat(tools): benchmark aggregation script`.

### Task 5: README with provider guides (PL + EN)

**Files:** Modify `README.md`, `README.pl.md`, `GrogAI.example.ini`; Test `tools/tests/test_hygiene.sh` (extend)

- [x] Test: both READMEs contain sections for Anthropic, OpenAI, DeepSeek, OpenRouter, Google Gemini, Ollama, LM Studio, and
  "tested models"; GrogAI.example.ini says base_url is required for `openai_compatible`. Run → FAIL.
- [x] Write: per provider the key page, `provider`/`base_url`/`model` lines (Anthropic default; OpenAI `https://api.openai.com/v1`;
  DeepSeek `https://api.deepseek.com`; OpenRouter `https://openrouter.ai/api/v1`; Gemini
  `https://generativelanguage.googleapis.com/v1beta/openai`; Ollama `http://localhost:11434/v1`; LM Studio
  `http://localhost:1234/v1`), billing note; "Tested models" table (gpt-4.1-mini: playtest 1, works; others: not tested yet);
  warning that weaker/local models get commands wrong more often; troubleshooting (the in-game hints from Task 2, where logs are,
  `aggregate.py` usage). Fix the example ini's base_url comment.
- [x] Run `bash tools/tests/run.sh` → PASS. Commit `docs: provider guides in README (PL/EN)`.

### Task 6: Playtest 2 and release 0.1.0 (user-gated)

- [ ] PR → `develop` → beta. User playtest 2 with the checklist from `work/0012` plus: wrong key shows the hint; summary file has
  cost > 0; `aggregate.py logs` prints a row. Results in `work/`.
- [ ] After the user's go-ahead: version 0.1.0 (`tools/version.sh`), changelog, PR `develop` → `master` (release workflow).
