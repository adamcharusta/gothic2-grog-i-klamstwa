#!/usr/bin/env bash
# Repository hygiene: line endings, binary files, ignores, VERSION, editorconfig.
source "$(dirname "$0")/lib.sh"
cd "$(git rev-parse --show-toplevel)" || exit 1

attr() { git check-attr "$1" -- "$2" | sed 's/.*: //'; }

assert_eq "lf"   "$(attr eol foo.cpp)" "cpp uses LF"
assert_eq "lf"   "$(attr eol docs/x.md)" "md uses LF"
assert_eq "crlf" "$(attr eol foo.bat)" "bat uses CRLF"
assert_eq "crlf" "$(attr eol foo.cmd)" "cmd uses CRLF"
assert_eq "crlf" "$(attr eol foo.iss)" "iss uses CRLF"
for f in foo.png foo.zen foo.vdf foo.tga foo.dat; do
  assert_eq "set" "$(attr binary "$f")" "$f is binary"
done
assert_eq "unspecified" "$(attr working-tree-encoding foo.d)" "Daedalus scripts are not re-encoded"

for f in GrogAI.ini x.local.ini .env build/x dist/x.zip logs/a.jsonl Data/Anims.vdf _work/x a.sav Thumbs.db; do
  assert_ok "$f is ignored" -- git check-ignore -q --no-index "$f"
done
for f in GrogAI.example.ini VERSION docs/00-vision.md mod/data/cast.json aicore/tests/data/x.json; do
  assert_fail "$f is not ignored" -- git check-ignore -q --no-index "$f"
done

version="$(cat VERSION 2>/dev/null)"
assert_ok "VERSION is semver" -- grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' VERSION
assert_eq "0.1.0" "$version" "VERSION starts at 0.1.0"

ec="$(cat .editorconfig 2>/dev/null)"
assert_contains "$ec" "end_of_line = lf" "editorconfig default LF"
assert_contains "$ec" "[*.{bat,cmd,iss}]" "editorconfig CRLF section"
assert_contains "$(sed -n '/\[\*\.{bat,cmd,iss}\]/,/^\[/p' .editorconfig 2>/dev/null)" "end_of_line = crlf" "editorconfig CRLF for Windows scripts"

# Engine submodule (OpenGothic fork)
if [[ -f .gitmodules ]]; then
  assert_eq "https://github.com/adamcharusta/OpenGothic.git" "$(git config -f .gitmodules submodule.engine.url)" "engine submodule url"
  assert_eq "grog/main" "$(git config -f .gitmodules submodule.engine.branch)" "engine submodule branch"
else
  _fail ".gitmodules missing"
fi
assert_ok "engine-upstream.md records an upstream SHA" -- grep -Eq '[0-9a-f]{40}' docs/engine-upstream.md

# README provider guides (both languages)
for readme in README.md README.pl.md; do
  r="$(cat "$readme" 2>/dev/null)"
  for p in "Anthropic" "OpenAI" "DeepSeek" "OpenRouter" "Gemini" "Ollama" "LM Studio" \
           "https://api.openai.com/v1" "https://api.deepseek.com" "https://openrouter.ai/api/v1" \
           "https://generativelanguage.googleapis.com/v1beta/openai" "http://localhost:11434/v1" "http://localhost:1234/v1" \
           "gpt-4.1-mini" "aggregate.py"; do
    assert_contains "$r" "$p" "$readme mentions $p"
  done
done
assert_contains "$(cat README.md)" "## Tested models" "README.md tested models section"
assert_contains "$(cat README.pl.md)" "## Przetestowane modele" "README.pl.md tested models section"
assert_contains "$(cat GrogAI.example.ini)" "required for openai_compatible" "example ini: base_url required for openai_compatible"

finish
