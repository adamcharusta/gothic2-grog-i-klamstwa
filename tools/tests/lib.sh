#!/usr/bin/env bash
# Minimal assertion helpers for bash tests. Source this file, call assertions, end with `finish`.
TESTS_FAILED=0
TESTS_PASSED=0

_pass() { TESTS_PASSED=$((TESTS_PASSED + 1)); }
_fail() { TESTS_FAILED=$((TESTS_FAILED + 1)); echo "  FAIL: $*" >&2; }

# assert_eq <expected> <actual> <msg>
assert_eq() {
  if [[ "$1" == "$2" ]]; then _pass; else _fail "$3 (expected '$1', got '$2')"; fi
}

# assert_contains <haystack> <needle> <msg>
assert_contains() {
  if [[ "$1" == *"$2"* ]]; then _pass; else _fail "$3 (missing '$2')"; fi
}

# assert_not_contains <haystack> <needle> <msg>
assert_not_contains() {
  if [[ "$1" != *"$2"* ]]; then _pass; else _fail "$3 (unexpected '$2')"; fi
}

# assert_ok <msg> -- <cmd...>
assert_ok() {
  local msg="$1"; shift; [[ "$1" == "--" ]] && shift
  if "$@" >/dev/null 2>&1; then _pass; else _fail "$msg (command failed: $*)"; fi
}

# assert_fail <msg> -- <cmd...>
assert_fail() {
  local msg="$1"; shift; [[ "$1" == "--" ]] && shift
  if "$@" >/dev/null 2>&1; then _fail "$msg (command unexpectedly succeeded: $*)"; else _pass; fi
}

finish() {
  echo "  passed: $TESTS_PASSED, failed: $TESTS_FAILED"
  [[ "$TESTS_FAILED" -eq 0 ]]
}
