#!/usr/bin/env bash
# tools/version.sh: release / beta / dev version strings and tag guard.
source "$(dirname "$0")/lib.sh"
ROOT="$(git rev-parse --show-toplevel)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
REPO="$TMP/repo z ł – test"
mkdir -p "$REPO" && cp -r "$ROOT/tools" "$REPO/"
cd "$REPO" || exit 1
git init -q && git config user.email t@t && git config user.name t
printf '0.2.0\n' > VERSION && git add -A && git commit -qm "feat: init"

V="$REPO/tools/version.sh"
assert_eq "0.2.0"         "$(bash "$V" release)"   "release version"
assert_eq "0.2.0-beta.17" "$(bash "$V" beta 17)"   "beta version"
assert_eq "0.2.0-dev.5"   "$(bash "$V" dev 5)"     "dev version"

printf '0.2.0 \r\n' > VERSION
assert_eq "0.2.0" "$(bash "$V" release)" "CRLF and trailing space are stripped"

printf '0.2' > VERSION
assert_fail "invalid VERSION rejected" -- bash "$V" release
printf '0.2.0\n' > VERSION
assert_fail "beta without run number rejected" -- bash "$V" beta
assert_fail "unknown mode rejected" -- bash "$V" nightly 3

git tag v0.2.0
bash "$V" release >/dev/null 2>"$TMP/err"; code=$?
assert_eq "2" "$code" "existing tag exits 2"
assert_contains "$(cat "$TMP/err")" "tag v0.2.0 already exists" "existing tag message"

finish
