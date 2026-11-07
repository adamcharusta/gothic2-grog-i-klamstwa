#!/usr/bin/env bash
# Usage: package.sh --version <v> --engine-dir <dir> --out <dir>
# Stages the mod package into <out>/GrogIKlamstwa-<v>-win64/, zips it, prints the zip path last.
# Exit codes: 1 usage, 3 engine exe/licence missing, 4 forbidden content, 5 no zip tool.
set -euo pipefail

die() { echo "error: $2" >&2; exit "$1"; }

version="" engine="" out=""
while (( $# )); do
  case "$1" in
    --version) version="${2:-}"; shift 2 ;;
    --engine-dir) engine="${2:-}"; shift 2 ;;
    --out) out="${2:-}"; shift 2 ;;
    *) die 1 "unknown argument: $1" ;;
  esac
done
[[ -n "$version" && -n "$engine" && -n "$out" ]] || die 1 "usage: package.sh --version <v> --engine-dir <dir> --out <dir>"
[[ -f "$engine/Gothic2Notr.exe" ]] || die 3 "Gothic2Notr.exe not found in $engine"

root="$(git rev-parse --show-toplevel)"
[[ -f "$root/engine/LICENSE" ]] || die 3 "engine/LICENSE not found (is the engine submodule initialised?)"

name="GrogIKlamstwa-$version-win64"
stage="$out/$name"
zipfile="$out/$name.zip"
rm -rf "$stage" "$zipfile"
mkdir -p "$stage/bin" "$stage/system" "$stage/LICENSES"

# Engine binaries: everything the engine build produced (exe, dlls, ...).
cp -r "$engine"/. "$stage/bin/"

# Mod files
iconv -f UTF-8 -t WINDOWS-1250 "$root/packaging/system/GrogIKlamstwa.ini" \
  | sed "s/^Version=.*/Version=$version/" > "$stage/system/GrogIKlamstwa.ini"
sed 's/\r*$/\r/' "$root/packaging/Start-GrogIKlamstwa.bat" > "$stage/Start-GrogIKlamstwa.bat"
cp "$root/GrogAI.example.ini" "$root/README.md" "$root/README.pl.md" "$stage/"
printf '%s\n' "$version" > "$stage/VERSION"
cp "$root/LICENSE" "$stage/LICENSES/LICENSE"
cp "$root/engine/LICENSE" "$stage/LICENSES/OpenGothic-LICENSE"
cp "$root/packaging/THIRD_PARTY.md" "$stage/LICENSES/THIRD_PARTY.md"

# Refuse forbidden content: AI key files, original game files, API keys in text files.
forbidden="$(cd "$stage" && {
  find . -type f \( -iname 'GrogAI.ini' -o -iname '*.local.ini' -o -name '.env' \
    -o -iname '*.vdf' -o -iname '*.mod' -o -iname '*.zen' -o -iname '*.dat' \) -print
  find . -type d \( -name 'Data' -o -name '_work' \) -print
  grep -rlE --exclude='*.exe' --exclude='*.dll' 'sk-[A-Za-z0-9_-]{20,}' . || true
})"
if [[ -n "$forbidden" ]]; then
  rm -rf "$stage"
  die 4 "forbidden file in package: $(printf '%s\n' "$forbidden" | head -1 | sed 's|^\./||')"
fi

# Zip with whatever is available.
if command -v zip >/dev/null 2>&1; then
  (cd "$out" && zip -qr "$name.zip" "$name")
elif command -v 7z >/dev/null 2>&1; then
  (cd "$out" && 7z a -tzip -bso0 -bsp0 "$name.zip" "$name")
elif py="$(command -v python3 || command -v python)"; then
  "$py" -c 'import shutil, sys; shutil.make_archive(sys.argv[1], "zip", sys.argv[2], sys.argv[3])' \
    "$out/$name" "$out" "$name"
else
  die 5 "no zip tool found (zip, 7z or python)"
fi
echo "$zipfile"
