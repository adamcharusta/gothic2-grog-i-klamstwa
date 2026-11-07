#!/usr/bin/env bash
# tools/changelog.sh: Conventional Commits grouped into Markdown sections.
source "$(dirname "$0")/lib.sh"
ROOT="$(git rev-parse --show-toplevel)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
REPO="$TMP/repo z ł – test"
mkdir -p "$REPO" && cp -r "$ROOT/tools" "$REPO/"
cd "$REPO" || exit 1
git init -q && git config user.email t@t && git config user.name t
c() { git commit -q --allow-empty -m "$1"; }
c "chore: base"; BASE="$(git rev-parse HEAD)"
c "feat: a"; FEAT="$(git rev-parse HEAD)"
c "fix: b"; c "docs: c"; c "refactor: d"; c "feat(ui): e"
git checkout -q -b side && c "fix: side" && git checkout -q - && git merge -q --no-ff -m "Merge pull request #9 from x/side" side

out="$(bash tools/changelog.sh)"
assert_contains "$out" "## Features" "features section"
assert_contains "$out" "- a (" "feat listed"
assert_contains "$out" "- e (" "scoped feat listed"
assert_contains "$out" "## Fixes" "fixes section"
assert_contains "$out" "- b (" "fix listed"
assert_contains "$out" "## Maintenance" "maintenance section"
assert_contains "$out" "- c (" "docs listed"
assert_contains "$out" "## Other" "other section"
assert_contains "$out" "- d (" "refactor listed"

assert_not_contains "$out" "Merge pull request" "merge commits excluded"

out2="$(bash tools/changelog.sh "$FEAT")"
assert_not_contains "$out2" "- a (" "from-ref excludes older commits"
assert_contains "$out2" "- b (" "from-ref keeps newer commits"

out3="$(bash tools/changelog.sh "$BASE")"
assert_not_contains "$out3" "- base (" "base commit excluded"

finish
