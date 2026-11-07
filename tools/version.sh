#!/usr/bin/env bash
# Usage: version.sh release | beta <run_number> | dev <run_number>
# Prints the version string derived from the VERSION file in the repo root.
set -euo pipefail

die() { echo "error: $2" >&2; exit "$1"; }

mode="${1:-}"
# Resolve the repo from the script location, not the current directory.
root="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
[[ -f "$root/VERSION" ]] || die 1 "VERSION file not found in $root"
version="$(tr -d ' \t\r\n' < "$root/VERSION")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die 1 "invalid VERSION '$version' (expected MAJOR.MINOR.PATCH)"

case "$mode" in
  release)
    tag="v$version"
    if git -C "$root" rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
      die 2 "tag $tag already exists - bump VERSION"
    fi
    if git -C "$root" remote get-url origin >/dev/null 2>&1 \
       && git -C "$root" ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1; then
      die 2 "tag $tag already exists on origin - bump VERSION"
    fi
    echo "$version"
    ;;
  beta|dev)
    n="${2:-}"
    [[ "$n" =~ ^[0-9]+$ ]] || die 1 "$mode requires a numeric run number"
    echo "$version-$mode.$n"
    ;;
  *)
    die 1 "usage: version.sh release | beta <run_number> | dev <run_number>"
    ;;
esac
