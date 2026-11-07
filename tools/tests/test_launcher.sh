#!/usr/bin/env bash
# Start-GrogIKlamstwa.bat executed for real under cmd.exe (Windows only; skipped elsewhere).
source "$(dirname "$0")/lib.sh"
ROOT="$(git rev-parse --show-toplevel)"
if ! command -v cmd.exe >/dev/null 2>&1 && [[ ! -x /c/Windows/System32/cmd.exe ]]; then
  echo "  skipped: needs Windows cmd.exe"; finish; exit $?
fi
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
PKG="$TMP/pkg"; GOTHIC="$TMP/Gothic II Złota Edycja"
mkdir -p "$PKG/bin" "$PKG/system" "$GOTHIC/System"
: > "$GOTHIC/System/Gothic2.exe"
sed 's/\r*$/\r/' "$ROOT/packaging/Start-GrogIKlamstwa.bat" > "$PKG/Start-GrogIKlamstwa.bat"
cp "$ROOT/packaging/system/GrogIKlamstwa.ini" "$PKG/system/"
# Fake engine: an exe that accepts any arguments and exits 0.
cp /c/Windows/System32/whoami.exe "$PKG/bin/Gothic2Notr.exe"
# gothic_path.txt as UTF-8 without BOM (what the installer and Notepad write).
printf '%s' "$(cygpath -w "$GOTHIC")" > "$PKG/gothic_path.txt"

out="$(cd "$PKG" && cmd.exe //c "$(cygpath -w "$PKG/Start-GrogIKlamstwa.bat")" < /dev/null 2>&1)"
[[ -n "${DEBUG_LAUNCHER:-}" ]] && echo "$out"
assert_not_contains "$out" "Mod manifest not found" "launcher accepts a Gothic path with Polish characters"
assert_ok "manifest copied into <Gothic>\System" -- test -f "$GOTHIC/System/GrogIKlamstwa.ini"

finish
