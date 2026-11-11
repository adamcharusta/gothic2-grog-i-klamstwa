#!/usr/bin/env bash
# tools/bench/aggregate.py: compares benchmark logs across runs and providers.
source "$(dirname "$0")/lib.sh"
ROOT="$(git rev-parse --show-toplevel)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
PY=python3
"$PY" -c "import sys" >/dev/null 2>&1 || PY=python
A="$ROOT/tools/bench/aggregate.py"
DATA="$ROOT/tools/tests/data/bench"

out="$("$PY" "$A" "$DATA")"; code=$?
assert_eq 0 "$code" "aggregates the fixture logs"
assert_contains "$out" "openai_compatible" "provider column"
assert_contains "$out" "gpt-4.1-mini" "model column"
assert_contains "$out" "claude-sonnet-5-5" "second provider row"
assert_contains "$out" "skipped lines: 1" "truncated last line is skipped, not fatal"
assert_not_contains "$out" "api_key" "never prints keys"

json="$("$PY" "$A" --json "$DATA")"
row="$(printf '%s' "$json" | "$PY" -c "import json,sys; r=[x for x in json.load(sys.stdin) if x['model']=='gpt-4.1-mini'][0]; print(r['runs'], r['requests'], r['errors'], round(r['cost_usd'],5), r['rejected_pct'], r['fallback_pct'], r['latency_ms_max'], r['endings'].get('returned',0), r['endings'].get('playing',0))")"
assert_eq "2 4 1 0.00168 50.0 50.0 3000.0 1 1" "$row" "openai row: runs, requests, errors, cost, rejected %, fallback %, max latency, endings"

mkdir -p "$TMP/empty"
"$PY" "$A" "$TMP/empty" >/dev/null 2>&1; assert_eq 2 "$?" "no logs -> exit 2"
finish
