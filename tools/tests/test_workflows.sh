#!/usr/bin/env bash
# GitHub Actions workflows: triggers, release/beta differences, build settings, no secrets besides GITHUB_TOKEN.
source "$(dirname "$0")/lib.sh"
cd "$(git rev-parse --show-toplevel)" || exit 1
WF=.github/workflows

for f in _build.yml ci.yml beta.yml release.yml; do
  assert_ok "$f exists" -- test -f "$WF/$f"
done
assert_ok "setup-vulkan action exists" -- test -f .github/actions/setup-vulkan/action.yml

build="$(cat "$WF/_build.yml" 2>/dev/null)"
ci="$(cat "$WF/ci.yml" 2>/dev/null)"
beta="$(cat "$WF/beta.yml" 2>/dev/null)"
rel="$(cat "$WF/release.yml" 2>/dev/null)"

assert_contains "$build" "workflow_call" "_build is reusable"
assert_contains "$build" "submodules: recursive" "_build checks out submodules"
assert_contains "$build" "1.4.357.0" "_build pins Vulkan SDK"
assert_contains "$build" "--target Gothic2Notr" "_build builds the game target"
assert_contains "$build" "iscc" "_build compiles the installer"
assert_contains "$build" "tools/tests/run.sh" "_build runs repo tests"
assert_contains "$build" "tools/package.sh" "_build packages"
assert_contains "$build" 'cygpath -w "$PWD/dist' "iscc gets absolute paths (iscc resolves relative ones against packaging/)"

assert_contains "$ci" "pull_request" "ci runs on PRs"
assert_contains "$ci" "version.sh dev" "ci uses dev version"
assert_not_contains "$ci" "push:" "ci does not double-build on push"

assert_contains "$beta" "- develop" "beta triggers on develop"
assert_not_contains "$beta" "- master" "beta does not trigger on master"
assert_contains "$beta" "--prerelease" "beta is a pre-release"
assert_contains "$beta" "version.sh beta" "beta uses beta version"

assert_contains "$rel" "- master" "release triggers on master"
assert_not_contains "$rel" "- develop" "release does not trigger on develop"
assert_not_contains "$rel" "--prerelease" "release is not a pre-release"
assert_contains "$rel" "concurrency" "release runs are serialised"
assert_contains "$rel" "version.sh release" "release uses release version"

all="$(cat "$WF"/*.yml 2>/dev/null)"
assert_not_contains "$all" "GROGAI_API_KEY" "no AI key in CI"
others="$(grep -o 'secrets\.[A-Za-z_]*' "$WF"/*.yml 2>/dev/null | sed 's/.*secrets\.//' | sort -u | grep -v '^GITHUB_TOKEN$' || true)"
assert_eq "" "$others" "only GITHUB_TOKEN secret used"

finish
