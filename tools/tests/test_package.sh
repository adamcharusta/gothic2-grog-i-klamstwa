#!/usr/bin/env bash
# tools/package.sh: package layout, launcher CRLF, manifest encoding, refusal on missing exe / forbidden files.
source "$(dirname "$0")/lib.sh"
ROOT="$(git rev-parse --show-toplevel)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
REPO="$TMP/repo z ł – test"
mkdir -p "$REPO"
for p in tools packaging README.md README.pl.md GrogAI.example.ini LICENSE VERSION; do
  [[ -e "$ROOT/$p" ]] && cp -r "$ROOT/$p" "$REPO/"
done
mkdir -p "$REPO/engine" && echo "MIT License (fake engine licence)" > "$REPO/engine/LICENSE"
cd "$REPO" || exit 1
git init -q

fake_engine() {
  rm -rf "$TMP/eng ł"; mkdir -p "$TMP/eng ł"
  : > "$TMP/eng ł/Gothic2Notr.exe"; : > "$TMP/eng ł/libstdc++-6.dll"
}
pack() { bash tools/package.sh --version 0.1.0-beta.3 --engine-dir "$TMP/eng ł" --out "$TMP/out ł" ; }

# success
fake_engine
last="$(pack 2>"$TMP/err" | tail -1)"; code=$?
assert_eq "0" "$code" "package succeeds"
ZIP="$TMP/out ł/GrogIKlamstwa-0.1.0-beta.3-win64.zip"
assert_eq "$ZIP" "$last" "last stdout line is the zip path"
list="$(unzip -l "$ZIP" 2>/dev/null)"
D="GrogIKlamstwa-0.1.0-beta.3-win64"
for p in bin/Gothic2Notr.exe bin/libstdc++-6.dll system/GrogIKlamstwa.ini Start-GrogIKlamstwa.bat \
         GrogAI.example.ini VERSION README.md README.pl.md \
         LICENSES/LICENSE LICENSES/OpenGothic-LICENSE LICENSES/THIRD_PARTY.md; do
  assert_contains "$list" "$D/$p" "zip contains $p"
done
STAGE="$TMP/out ł/$D"
assert_eq "0.1.0-beta.3" "$(tr -d '\r\n' < "$STAGE/VERSION")" "VERSION inside package"
# -U: MSYS grep strips CR in text mode
assert_ok "launcher has CRLF" -- grep -qU $'\r$' "$STAGE/Start-GrogIKlamstwa.bat"
assert_fail "launcher has no bare LF" -- grep -qvU $'\r$' "$STAGE/Start-GrogIKlamstwa.bat"
assert_ok "manifest version filled" -- grep -q "^Version=0.1.0-beta.3" "$STAGE/system/GrogIKlamstwa.ini"
assert_ok "manifest is Windows-1250 (ł = 0xB3)" -- grep -q $'\xb3' "$STAGE/system/GrogIKlamstwa.ini"

# run from inside another git repo (cwd must not matter)
OTHER="$TMP/other repo"; mkdir -p "$OTHER" && git -C "$OTHER" init -q
fake_engine
(cd "$OTHER" && bash "$REPO/tools/package.sh" --version 0.1.0-beta.3 --engine-dir "$TMP/eng ł" --out "$TMP/out2 ł" >/dev/null 2>&1)
assert_eq "0" "$?" "package works when cwd is another repo"

# unexpected files in the engine dir (original game binaries, build junk) are refused
for junk in vdfs32g.dll Gothic2.exe a.sav "GrogAI (1).ini" GrogAI.ini.bak; do
  fake_engine; : > "$TMP/eng ł/$junk"
  pack >/dev/null 2>"$TMP/err"; assert_eq "4" "$?" "engine dir with '$junk' refused"
done
fake_engine; mkdir -p "$TMP/eng ł/data"; : > "$TMP/eng ł/data/x"
pack >/dev/null 2>&1; assert_eq "4" "$?" "lower-case data/ dir refused"

# missing exe
fake_engine; rm "$TMP/eng ł/Gothic2Notr.exe"
pack >/dev/null 2>"$TMP/err"; code=$?
assert_eq "3" "$code" "missing exe exits 3"
assert_contains "$(cat "$TMP/err")" "Gothic2Notr.exe not found in" "missing exe message"

# forbidden files
fake_engine; echo "api_key = x" > "$TMP/eng ł/GrogAI.ini"
pack >/dev/null 2>"$TMP/err"; code=$?
assert_eq "4" "$code" "GrogAI.ini refused"
assert_contains "$(cat "$TMP/err")" "forbidden file in package:" "forbidden message"
assert_contains "$(cat "$TMP/err")" "GrogAI.ini" "forbidden file named"

fake_engine; : > "$TMP/eng ł/Anims.vdf"
pack >/dev/null 2>&1; assert_eq "4" "$?" "game archive refused"

fake_engine; cp README.md README.md.bak; echo "key sk-ant-aaaaaaaaaaaaaaaaaaaaaaaa" >> README.md
pack >/dev/null 2>&1; assert_eq "4" "$?" "API key in text refused"
mv README.md.bak README.md
fake_engine; cp README.md README.md.bak; echo "key AIzaSyA1234567890abcdefghijklmnopqrstu" >> README.md
pack >/dev/null 2>&1; assert_eq "4" "$?" "Gemini key in text refused"
mv README.md.bak README.md

finish
