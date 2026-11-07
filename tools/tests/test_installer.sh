#!/usr/bin/env bash
# packaging/installer.iss: static checks (real compilation happens in CI with iscc).
source "$(dirname "$0")/lib.sh"
cd "$(git rev-parse --show-toplevel)" || exit 1
ISS=packaging/installer.iss

assert_ok "installer.iss exists" -- test -f "$ISS"
# Index content is LF (normalised); the working tree must be CRLF per .gitattributes.
assert_eq "crlf" "$(git check-attr eol -- "$ISS" | sed 's/.*: //')" "iss checked out as CRLF"

src="$(tr -d '\r' < "$ISS" 2>/dev/null)"
assert_contains "$src" "AppId={{" "fixed AppId GUID"
assert_contains "$src" "AppVersion={#AppVersion}" "version from /DAppVersion"
assert_contains "$src" "OutputBaseFilename=GrogIKlamstwa-{#AppVersion}-setup" "output file name"
assert_contains "$src" "CreateInputDirPage" "custom Gothic folder page"
assert_contains "$src" "System\\Gothic2.exe" "checks the Gothic install"
assert_contains "$src" "steamapps\\common\\Gothic II" "Steam default path"
assert_contains "$src" 'Name: "polish"' "Polish language"
assert_contains "$src" 'Name: "english"' "English language"
assert_contains "$src" "gothic_path.txt" "writes gothic_path.txt"
assert_contains "$src" "SaveStringsToUTF8FileWithoutBOM" "gothic_path.txt written as UTF-8 (launcher reads it with chcp 65001)"
assert_contains "$src" "-game:GrogIKlamstwa.ini" "shortcut launches the mod"
assert_not_contains "$src" ".vdf" "no game archives referenced"

finish
