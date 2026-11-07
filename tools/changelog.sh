#!/usr/bin/env bash
# Usage: changelog.sh [<from-ref>]
# Prints a Markdown changelog of commits in <from-ref>..HEAD (whole history if omitted),
# grouped by Conventional Commit type.
set -euo pipefail

range="HEAD"
[[ -n "${1:-}" ]] && range="$1..HEAD"

features=(); fixes=(); maintenance=(); other=()
while IFS=$'\t' read -r sha subject; do
  [[ -z "$sha" ]] && continue
  type="${subject%%:*}"; type="${type%%(*}"; type="${type%!}"
  text="${subject#*: }"
  [[ "$subject" == *": "* ]] || text="$subject"
  line="- $text ($sha)"
  case "$type" in
    feat) features+=("$line") ;;
    fix) fixes+=("$line") ;;
    docs|chore|ci|build|test|style) maintenance+=("$line") ;;
    *) other+=("$line") ;;
  esac
done < <(git log --format='%h%x09%s' "$range")

section() {
  local title="$1"; shift
  (( $# == 0 )) && return 0
  printf '## %s\n\n' "$title"
  printf '%s\n' "$@"
  printf '\n'
}
section "Features" "${features[@]+"${features[@]}"}"
section "Fixes" "${fixes[@]+"${fixes[@]}"}"
section "Other" "${other[@]+"${other[@]}"}"
section "Maintenance" "${maintenance[@]+"${maintenance[@]}"}"
