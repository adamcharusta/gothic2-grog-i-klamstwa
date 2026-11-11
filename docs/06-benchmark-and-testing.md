# 06 — Benchmark, logs, testing

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Why

[U] The mod is a benchmark ahead of a full AI-driven Gothic. We must know **whether it works and what it costs**.

## Playthrough logs [P]

Every playthrough writes a file (e.g. JSONL in `logs/`) with:
- seed, scenario type, culprit, clue graph;
- every AI request: role (actor/director), NPC, provider, model, latency, input/output tokens, estimated cost;
- the reply, command validation result (accepted / rejected and why), retries, timeouts;
- game events (fights, items, quest log entries);
- ending, play time, number of exchanges.

With voice on, every TTS call also writes a line `{"kind":"tts","npc","model","ok","chars","audio_ms","latency_ms","cost_usd","cached","error"?}`
(`price_unknown: true` when the model has no entry in `pricing.json` `tts`; cached lines cost 0; the provider's error text and the key are never logged).
When repeated failures turn the voice off for the session, one more `{"kind":"tts","npc","disabled":true,"reason":<error kind>}` line is written.

No API key, no personal data. Full conversation text behind a switch (on by default, local only).

## Summary [P]

End screen and a `summary` file per playthrough: time, cost, average/max latency, rejected-command rate, ending.
The summary also carries `tts_requests`, `tts_errors`, `tts_audio_s`, `tts_cost_usd` (from `pricing.json` `tts.<model>.usd_per_minute`, an estimate)
and `tts_latency_avg_ms` (successful requests only); all 0 with voice off.
[B] A small aggregation script comparing logs across runs or providers (e.g. Python), separate from the game.

## Cost and latency

[U] No fixed budget up front: targets are set from playtest measurements.
[P] So the builder must make the cost/latency drivers **configurable** and measured from day one:
director call frequency, context size (lore excerpt length, memory window, summary threshold), separate cheaper director model,
prompt caching where the provider supports it, streaming replies. After the first playtests, the chosen targets are recorded
in `07-decisions.md` and here.

## Test environment

[U] The user owns the **Steam edition** of G2 NotR and has API keys for testing. Agents without the game verify against the mock
provider and unit tests; in-game acceptance is done by the user (or on the user's machine) and recorded in `work/` reports.

## Testing [P]

- Unit tests for the graphics-independent parts: scenario rolling and solvability, command parser and validator,
  prompt assembly (the actor never receives the full truth), LLM clients against recorded replies (no network).
- A **mock provider** with canned replies to test the game at zero cost.
- Manual acceptance script: the sample flow Greg → Skip → Matt (persuade and beat up), all 4 scenario types,
  all 3 endings, no internet, bad key, save and load mid-investigation (state identical after loading).
- "Break the GM" tests: the player tries to extract the secret, push a character out of role, force the tankard handover.

## Release success criterion [P]

An ordinary player with an API key installs the mod following the README, plays through to one of the endings in 30–60 minutes
without crashes, and the pirates sound like Gothic pirates. Logs allow comparing at least two providers.
