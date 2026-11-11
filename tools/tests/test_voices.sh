#!/usr/bin/env bash
# tools/voices/extract_voices.py: cuts each pirate's reference clip out of synthetic Speech*.vdf archives.
source "$(dirname "$0")/lib.sh"
ROOT="$(git rev-parse --show-toplevel)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
PY=python3
"$PY" -c "import sys" >/dev/null 2>&1 || PY=python
X="$ROOT/tools/voices/extract_voices.py"
FIX="$ROOT/tools/tests/fixtures/make_speech_vdf.py"
G="$TMP/Gothic ł – II"; REF="$TMP/ref"; OUT="$TMP/out ł/voices"; CWD="$TMP/cwd"
mkdir -p "$CWD" "$TMP/logs"

counts="$("$PY" "$FIX" "$G" "$REF")"
assert_eq 0 "$?" "fixture builds"
before="$(cd "$TMP" && find . \( -path './out*' -o -path ./logs \) -prune -o -print | sort; cksum "$G/Data/Speech_Addon.vdf")"

(cd "$CWD" && "$PY" "$X" --gothic "$G" --out "$OUT" >"$TMP/logs/stdout" 2>"$TMP/logs/err"); code=$?
assert_eq 3 "$code" "pirates without clips -> exit 3"
err="$(cat "$TMP/logs/err")"
assert_contains "$err" "francis" "pirate without clips named on stderr"
assert_not_contains "$err" "greg" "greg has clips"
assert_not_contains "$err" "angus" "angus gets the SVM_6 fallback"
assert_ok "greg.wav written" -- test -f "$OUT/greg.wav"
assert_ok "angus.wav written (SVM fallback)" -- test -f "$OUT/angus.wav"
assert_ok "skip.wav written" -- test -f "$OUT/skip.wav"
assert_fail "no clip file for francis" -- test -e "$OUT/francis.wav"

# greg.wav = the decoded _01_ lines, longest first, with 0.3 s (13230 samples) of silence between them
check="$("$PY" - "$OUT/greg.wav" "$REF" "$counts" <<'EOF'
import json, struct, sys, wave
from pathlib import Path
path, ref, counts = sys.argv[1], Path(sys.argv[2]), json.loads(sys.argv[3])
raw = open(path, "rb").read(44)
w = wave.open(path, "rb")
fmt_tag = struct.unpack("<H", raw[20:22])[0]
got = w.readframes(w.getnframes())
long_ = (ref / "DIA_ADDON_GREG_BYE_01_02.raw").read_bytes()
short = (ref / "DIA_ADDON_GREG_HELLO_01_00.raw").read_bytes()
want = long_ + b"\0\0" * 13230 + short
n = counts["dia_addon_greg_bye_01_02.wav"] + counts["DIA_ADDON_GREG_HELLO_01_00.WAV"] + 13230
print(raw[:4].decode(), raw[8:12].decode(), fmt_tag, w.getnchannels(), w.getsampwidth() * 8, w.getframerate(),
      w.getnframes() == n, got == want)
EOF
)"
assert_eq "RIFF WAVE 1 1 16 44100 True True" "$check" "greg.wav: PCM 16-bit mono 44.1 kHz, exact decoded samples + gaps, no hero line"

yaml="$(cat "$OUT/voice_to_speaker.yaml" 2>/dev/null)"
assert_contains "$yaml" "tts-1-hd:" "yaml has the tts-1-hd model"
assert_contains "$yaml" $'  greg:\n    model: xtts\n    speaker: voices/greg.wav\n    language: pl' "yaml greg entry"
assert_contains "$yaml" "openedai-speech" "yaml pins its format source"
assert_not_contains "$yaml" "francis" "yaml lists only pirates with clips"
assert_contains "$(cat "$TMP/logs/stdout")" "greg" "stdout summary per pirate"

after="$(cd "$TMP" && find . \( -path './out*' -o -path ./logs \) -prune -o -print | sort; cksum "$G/Data/Speech_Addon.vdf")"
assert_eq "$before" "$after" "nothing written outside --out"
assert_eq "" "$(ls -A "$CWD")" "nothing written to the working directory"
assert_eq $'angus.wav\ngreg.wav\nhank.wav\nskip.wav\nvoice_to_speaker.yaml' "$(ls "$OUT")" "out holds only the clips and the yaml"

# --seconds: stop once the total is reached (here: the longest clip alone)
"$PY" "$X" --gothic "$G" --out "$TMP/out ł/short" --seconds 0.01 >/dev/null 2>&1
n="$("$PY" -c "import wave,sys; print(wave.open(sys.argv[1]).getnframes())" "$TMP/out ł/short/greg.wav" 2>/dev/null)"
assert_eq "$("$PY" -c "import json,sys; print(json.loads(sys.argv[1])['dia_addon_greg_bye_01_02.wav'])" "$counts")" "$n" "--seconds 0.01 keeps only the longest clip"

# every pirate has a clip -> exit 0
G2="$TMP/g2"; "$PY" "$FIX" --all "$G2" "$TMP/ref2" >/dev/null
"$PY" "$X" --gothic "$G2" --out "$TMP/out ł/all" >/dev/null 2>"$TMP/logs/err2"
assert_eq 0 "$?" "all 16 pirates got a clip -> exit 0"
assert_eq 16 "$(grep -c 'model: xtts' "$TMP/out ł/all/voice_to_speaker.yaml" 2>/dev/null)" "yaml has 16 entries"

# corrupt archives (huge entry count, entry past EOF, root offset past EOF) are skipped, not fatal
G3="$TMP/g3"; "$PY" "$FIX" --corrupt "$G3" "$TMP/ref3" >/dev/null
"$PY" "$X" --gothic "$G3" --out "$TMP/out ł/corrupt" >/dev/null 2>"$TMP/logs/err3"
assert_eq 3 "$?" "corrupt archives -> still runs (exit 3: other pirates have no clip)"
err3="$(cat "$TMP/logs/err3")"
assert_not_contains "$err3" "Traceback" "no traceback on corrupt archives"
assert_contains "$err3" "Speech1.vdf" "huge entry count reported"
assert_contains "$err3" "Speech3.vdf" "root offset past EOF reported"
assert_ok "greg.wav identical despite a broken Greg entry" -- cmp -s "$OUT/greg.wav" "$TMP/out ł/corrupt/greg.wav"
assert_ok "the valid entry of a partly broken archive is used" -- test -f "$TMP/out ł/corrupt/francis.wav"

# usage error -> exit 1 (exit 2 means "no speech archives")
"$PY" "$X" --out "$TMP/out ł/usage" >/dev/null 2>&1
assert_eq 1 "$?" "missing --gothic -> exit 1"

# no Data/Speech*.vdf -> exit 2
mkdir -p "$TMP/empty/Data"
"$PY" "$X" --gothic "$TMP/empty" --out "$TMP/out ł/none" >/dev/null 2>&1
assert_eq 2 "$?" "no speech archives -> exit 2"
assert_fail "nothing written without archives" -- test -e "$TMP/out ł/none"

finish
