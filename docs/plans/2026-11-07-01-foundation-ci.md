# Plan 1 — Foundation & CI/CD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A repo where every push builds the (still unmodified) OpenGothic fork on Windows, pushes to `develop` publish a beta package and pushes to `master` publish an official release (zip + installer).

**Architecture:** The engine is a GitHub fork of OpenGothic (`adamcharusta/OpenGothic`, branch `grog/main`) mounted as a submodule at `engine/`. Shell tools in `tools/` (version, changelog, package) are pure bash and unit-tested by bash test scripts. One reusable workflow `_build.yml` builds the engine with MSYS2/MinGW exactly like upstream, packages it, and is called by `ci.yml`, `beta.yml` and `release.yml`.

**Tech Stack:** Git submodules, GitHub Actions, MSYS2 MinGW64 + CMake + Ninja + Vulkan SDK 1.4.357.0 (copied from upstream `build.yml`), bash, Inno Setup 6, `gh` CLI.

**Spec:** [docs/09-repo-ci-and-versioning.md](../09-repo-ci-and-versioning.md), [docs/04-architecture-opengothic.md](../04-architecture-opengothic.md), [AGENTS.md](../../AGENTS.md)

## Global Constraints

- Everything (code, comments, commit messages, reports) in English. Conventional Commits.
- Text files LF; CRLF only for `*.bat *.cmd *.sln *.vcxproj* *.iss`. Daedalus `*.d`/`*.src` never re-encoded (Windows-1250).
- Packages must never contain original Gothic files (`*.vdf *.mod *.zen *.dat`, `Data/`, `_work/`) or an AI key file (`GrogAI.ini`, `*.local.ini`, `.env`).
- `VERSION` holds `MAJOR.MINOR.PATCH`, starts at `0.1.0`. Release tag `v<VERSION>`; beta `v<VERSION>-beta.<run_number>`; CI dev builds `<VERSION>-dev.<run_number>`.
- Mod name in files: `GrogIKlamstwa`. Mod manifest: `system/GrogIKlamstwa.ini`. Engine launch: `Gothic2Notr.exe -g "<gothic dir>" -game:GrogIKlamstwa.ini` (OpenGothic looks the manifest up in `<gothic>/system/`).
- All tasks of this plan are committed on branch `feature/foundation-ci` (created from `develop`) and merged into `develop` by one PR in Task 6. Never push to `master`.
- After each task: write `work/NNNN-*.md` from `work/_TEMPLATE.md` and add a row to `work/INDEX.md` (AGENTS.md rule).
- The project path contains spaces, `ł` and `–`: every script quotes every path.

## Review Focus

1. Paths with spaces and non-ASCII characters (the real project path): `package.sh` must work from such a path — test in Task 4.
2. `VERSION` saved with CRLF, trailing spaces or no final newline must still produce a clean version string — test in Task 3.
3. Re-running a release for a version whose tag already exists must fail loudly, not overwrite the release — test in Task 3.
4. A local `GrogAI.ini` (with a key) or a stray game file in the tree must never reach a package; the packager must refuse — test in Task 4.
5. A missing or uninitialised `engine/` build (no `Gothic2Notr.exe`) must fail the packager with a clear message, not produce an empty zip — test in Task 4.

---

### Task 1: Repository hygiene files and VERSION

**Files:**
- Create: `.gitattributes`, `.editorconfig`, `.gitignore`, `VERSION`, `LICENSE`, `GrogAI.example.ini`
- Create: `tools/tests/lib.sh`, `tools/tests/run.sh`, `tools/tests/test_hygiene.sh`

**Interfaces:**
- Produces: `tools/tests/lib.sh` with `assert_eq <expected> <actual> <msg>`, `assert_ok <msg> -- <cmd...>`, `assert_fail <msg> -- <cmd...>`, `assert_contains <haystack> <needle> <msg>`; failures increment a counter, `finish` exits 1 if any failed.
- Produces: `tools/tests/run.sh` runs every `tools/tests/test_*.sh` and exits non-zero if any fails. CI calls `bash tools/tests/run.sh`.

- [ ] **Step 1: Write `tools/tests/lib.sh`, `tools/tests/run.sh` and the failing test `tools/tests/test_hygiene.sh`** asserting, from the repo root:
  - `git check-attr eol -- foo.cpp` → `eol: lf`; `foo.bat`, `foo.iss` → `eol: crlf`; `foo.png`, `foo.zen`, `foo.vdf` → `binary: set` (check `git check-attr binary`).
  - `git check-attr working-tree-encoding -- foo.d` → `unspecified` (no re-encoding).
  - `git check-ignore -q` succeeds for `GrogAI.ini`, `x.local.ini`, `.env`, `build/x`, `dist/x.zip`, `logs/a.jsonl`, `Data/Anims.vdf`, `_work/x`, `a.sav`, `Thumbs.db`; fails for `GrogAI.example.ini`, `VERSION`, `docs/00-vision.md`.
  - `VERSION` content matches `^[0-9]+\.[0-9]+\.[0-9]+$` and equals `0.1.0`.
  - `.editorconfig` contains `end_of_line = lf` under `[*]` and `end_of_line = crlf` for `[*.{bat,cmd,iss}]`.
- [ ] **Step 2: Run** `bash tools/tests/run.sh` — expected FAIL (files missing).
- [ ] **Step 3: Create the hygiene files** with the exact content listed in docs/09 §"Repository hygiene files". C/C++/CMake settings copy upstream OpenGothic's `.editorconfig` (2 spaces, `max_line_length = 120`). `[*.{d,src}]`: `charset` unset, `indent_style = tab`. `[*.md]`: `trim_trailing_whitespace = false`. `VERSION` = `0.1.0`. `LICENSE` = MIT, copyright "2026 Adam Charusta" (designer's call — see decision log). `GrogAI.example.ini` = the sample from docs/05 with `api_key =` empty.
- [ ] **Step 4: Run** `git add --renormalize . && bash tools/tests/run.sh` — expected PASS, and `git status` shows no unexpected EOL churn in docs.
- [ ] **Step 5: Commit** `chore: add repo hygiene files, VERSION and test harness`. Write work report + INDEX row.

### Task 2: OpenGothic fork as submodule

**Files:**
- Create: `.gitmodules` (via `git submodule add`), `engine/` (submodule), `docs/engine-upstream.md`
- Modify: `tools/tests/test_hygiene.sh` (add submodule assertions)

**Interfaces:**
- Produces: submodule `engine` → `https://github.com/adamcharusta/OpenGothic.git`, `branch = grog/main`. Upstream base commit SHA recorded in `docs/engine-upstream.md` (table: date, upstream SHA, fork SHA, note).

- [ ] **Step 1: Add failing assertions** to `test_hygiene.sh`: `git config -f .gitmodules submodule.engine.url` equals `https://github.com/adamcharusta/OpenGothic.git`; `submodule.engine.branch` equals `grog/main`; `docs/engine-upstream.md` contains a 40-hex SHA.
- [ ] **Step 2: Run** `bash tools/tests/run.sh` — expected FAIL.
- [ ] **Step 3: Create the fork** — **ask the user to confirm first** (outward action on their account): `gh repo fork Try/OpenGothic --clone=false`. Then in a temp clone of the fork: `git checkout -b grog/main origin/master && git push -u origin grog/main` and `gh repo edit adamcharusta/OpenGothic --default-branch grog/main`.
- [ ] **Step 4: Add the submodule** `git submodule add -b grog/main https://github.com/adamcharusta/OpenGothic.git engine` then `git submodule update --init --recursive`. Write `docs/engine-upstream.md` with the upstream `master` SHA the branch was cut from, and add it to the legend in `AGENTS.md` and `CLAUDE.md`.
- [ ] **Step 5: Run** `bash tools/tests/run.sh` — expected PASS.
- [ ] **Step 6: Commit** `build: add OpenGothic fork as engine submodule`. Work report + INDEX row.

### Task 3: Version and changelog tools

**Files:**
- Create: `tools/version.sh`, `tools/changelog.sh`
- Test: `tools/tests/test_version.sh`, `tools/tests/test_changelog.sh`

**Interfaces:**
- Produces: `tools/version.sh <mode> [run_number]` prints one line to stdout.
  - `release` → `<VERSION>`; exit 2 with `error: tag v<VERSION> already exists` on stderr if that tag exists locally or on `origin` (`git ls-remote --tags origin` when `origin` exists).
  - `beta <n>` → `<VERSION>-beta.<n>`; `dev <n>` → `<VERSION>-dev.<n>`.
  - Invalid `VERSION` content or missing `n` → exit 1 with a message. Reads `VERSION` from the repo root (`git rev-parse --show-toplevel`), strips `\r`, spaces and newlines.
- Produces: `tools/changelog.sh [<from-ref>]` prints Markdown to stdout: sections `## Features` (`feat:`/`feat(...)`), `## Fixes` (`fix:`), `## Other` (everything else except `docs:`/`chore:`/`ci:` which go to `## Maintenance`); one `- <subject> (<short sha>)` per commit from `<from-ref>..HEAD`, or full history when omitted. Empty sections omitted.

- [ ] **Step 1: Write failing tests.** Each test builds a throwaway repo under `mktemp -d` (with a space and `ł` in its name), copies `tools/` into it, and asserts:
  - `VERSION`=`0.2.0` → `release` prints `0.2.0`; `beta 17` prints `0.2.0-beta.17`; `dev 5` prints `0.2.0-dev.5`.
  - `VERSION` written as `printf '0.2.0 \r\n'` → `release` prints `0.2.0`.
  - `VERSION`=`0.2` → `release` exits 1; `beta` without a number exits 1.
  - after `git tag v0.2.0`, `release` exits 2 and stderr contains `tag v0.2.0 already exists`.
  - changelog: commits `feat: a`, `fix: b`, `docs: c`, `refactor: d` → output contains `## Features`, `- a (`, `## Fixes`, `- b (`, `## Maintenance`, `- c (`, `## Other`, `- d (`; with `<from-ref>` = the `feat: a` commit, `- a (` is absent.
- [ ] **Step 2: Run** `bash tools/tests/run.sh` — expected FAIL.
- [ ] **Step 3: Implement** `tools/version.sh` and `tools/changelog.sh` (`set -euo pipefail`, `git log --format='%h%x09%s'`).
- [ ] **Step 4: Run** `bash tools/tests/run.sh` — expected PASS.
- [ ] **Step 5: Commit** `build: add version and changelog tools`. Work report + INDEX row.

### Task 4: Package assembler

**Files:**
- Create: `tools/package.sh`, `packaging/system/GrogIKlamstwa.ini`, `packaging/Start-GrogIKlamstwa.bat`, `packaging/THIRD_PARTY.md`, `README.md`, `README.pl.md`
- Test: `tools/tests/test_package.sh`

**Interfaces:**
- Consumes: `tools/version.sh` (Task 3) — callers pass the result as `--version`.
- Produces: `tools/package.sh --version <v> --engine-dir <dir> --out <dir>` creates `<out>/GrogIKlamstwa-<v>-win64/` and `<out>/GrogIKlamstwa-<v>-win64.zip`, prints the zip path as the last stdout line. Layout:
  ```
  GrogIKlamstwa-<v>-win64/
    bin/Gothic2Notr.exe, bin/*.dll          (from --engine-dir)
    system/GrogIKlamstwa.ini                 (mod manifest)
    Start-GrogIKlamstwa.bat
    GrogAI.example.ini, VERSION (contains <v>), README.md, README.pl.md
    LICENSES/LICENSE, LICENSES/OpenGothic-LICENSE, LICENSES/THIRD_PARTY.md
  ```
- Produces: exit 3 with `error: Gothic2Notr.exe not found in <dir>` if the exe is missing; exit 4 with `error: forbidden file in package: <path>` if the staged tree contains any forbidden pattern from Global Constraints or any file whose content matches `sk-[A-Za-z0-9_-]{20,}`.

- [ ] **Step 1: Write failing tests** (temp dirs with a space and `ł` in the name; fake engine dir with an empty `Gothic2Notr.exe` and `libstdc++-6.dll`):
  - success: zip exists, `unzip -l` lists every path in the layout above; `VERSION` inside equals the passed version; `Start-GrogIKlamstwa.bat` inside has CRLF line endings.
  - missing exe → exit 3, message as specified.
  - a `GrogAI.ini` placed in the fake engine dir → exit 4 naming it; a `Anims.vdf` in the engine dir → exit 4; a `README` copy containing `sk-ant-aaaaaaaaaaaaaaaaaaaaaaaa` → exit 4.
- [ ] **Step 2: Run** `bash tools/tests/run.sh` — expected FAIL.
- [ ] **Step 3: Create the packaging inputs.**
  - `packaging/system/GrogIKlamstwa.ini`: Gothic mod manifest with `[INFO] Title=Grog i kłamstwa`, `Version=` (filled by `package.sh` via `sed`), `Authors=Adam Charusta`, `Webpage=https://github.com/adamcharusta/gothic2-grog-i-klamstwa`, `Description=` one line; `[FILES]` left empty (world/scripts come in Plan 3). Saved in Windows-1250 (Gothic reads manifests in the ANSI code page).
  - `packaging/Start-GrogIKlamstwa.bat` (CRLF): reads the Gothic path from `gothic_path.txt` next to it if present; copies `system\GrogIKlamstwa.ini` to `<gothic>\system\` if missing; runs `bin\Gothic2Notr.exe -g "<gothic>" -game:GrogIKlamstwa.ini`; without `gothic_path.txt` runs without `-g` (OpenGothic autodetects the install) and prints a hint in PL and EN when the manifest cannot be copied.
  - `README.md` (EN) / `README.pl.md` (PL): what the mod is, requirement of a legal G2 NotR, install from zip or installer, where the AI config goes (provider guides come in Plan 5 — leave a "Coming soon" heading), link to the other language.
  - `packaging/THIRD_PARTY.md`: OpenGothic (MIT) and its submodules with their licences (list from `engine/.gitmodules`).
- [ ] **Step 4: Implement** `tools/package.sh` (stage into `<out>/GrogIKlamstwa-<v>-win64/`, copy `engine/LICENSE` as `OpenGothic-LICENSE`, scan, then `zip -r`; on Windows runners use `7z a` when `zip` is absent).
- [ ] **Step 5: Run** `bash tools/tests/run.sh` — expected PASS.
- [ ] **Step 6: Commit** `build: add package assembler, launcher and READMEs`. Work report + INDEX row.

### Task 5: Windows installer

**Files:**
- Create: `packaging/installer.iss` (CRLF), `tools/tests/test_installer.sh`

**Interfaces:**
- Consumes: the staged folder from Task 4.
- Produces: `iscc packaging/installer.iss /DAppVersion=<v> /DSourceDir=<staged folder> /DOutputDir=<out>` → `<out>/GrogIKlamstwa-<v>-setup.exe`.

- [ ] **Step 1: Write a failing static test** `test_installer.sh` asserting `installer.iss` has CRLF endings and contains: `AppId={{` (fixed GUID), `AppVersion={#AppVersion}`, `OutputBaseFilename=GrogIKlamstwa-{#AppVersion}-setup`, a custom wizard page asking for the Gothic 2 NotR folder, a check that `<chosen>\System\Gothic2.exe` exists, `[Languages]` entries for `Polish` and `English`, and no reference to `.vdf`.
- [ ] **Step 2: Run** `bash tools/tests/run.sh` — expected FAIL.
- [ ] **Step 3: Write `installer.iss`.** Installs the staged folder to `{autopf}\GrogIKlamstwa`; the Gothic folder page defaults to the Steam path `{commonpf32}\Steam\steamapps\common\Gothic II` when it exists; writes that path to `{app}\gothic_path.txt`; copies `system\GrogIKlamstwa.ini` into `<gothic>\System\`; creates Start-menu and optional desktop shortcuts running `{app}\bin\Gothic2Notr.exe -g "<gothic>" -game:GrogIKlamstwa.ini`; the uninstaller removes the copied manifest.
- [ ] **Step 4: Run** `bash tools/tests/run.sh` — expected PASS. (Real compilation is verified in CI, Task 6.)
- [ ] **Step 5: Commit** `build: add Inno Setup installer`. Work report + INDEX row.

### Task 6: GitHub Actions — build, CI, beta, release

**Files:**
- Create: `.github/workflows/_build.yml`, `.github/workflows/ci.yml`, `.github/workflows/beta.yml`, `.github/workflows/release.yml`
- Create: `.github/actions/setup-vulkan/action.yml` (copy of upstream's, same inputs)

**Interfaces:**
- Consumes: `tools/version.sh`, `tools/changelog.sh`, `tools/package.sh`, `packaging/installer.iss`.
- Produces: reusable workflow `_build.yml` (`on: workflow_call`, input `version: string`) with jobs:
  1. `checks` (ubuntu-24.04): `bash tools/tests/run.sh`.
  2. `engine` (windows-latest, MSYS2 MINGW64, steps mirrored from upstream `build.yml` job `windows`, run inside `engine/`, `submodules: recursive`, Vulkan SDK `1.4.357.0`): writes `engine/game/build.h` with `appBuild = "Grog i klamstwa <version>"`, builds target `Gothic2Notr` only, copies the three MinGW runtime DLLs.
  3. `package` (windows-latest, needs both): runs `tools/package.sh` in Git Bash, installs Inno Setup (`choco install innosetup -y --no-progress`), runs `iscc`, uploads artifact `GrogIKlamstwa-<version>` containing the zip and the setup exe.
- `ci.yml`: `on: pull_request` and `push` to branches other than `master`/`develop`; computes `version.sh dev ${{ github.run_number }}`; calls `_build.yml`.
- `beta.yml`: `on: push: branches: [develop]`; version `beta <run_number>`; calls `_build.yml`; job `publish` (`permissions: contents: write`) downloads the artifact and runs `gh release create v<v> --prerelease --target $GITHUB_SHA --title "Grog i kłamstwa <v> (beta)" --notes "$(tools/changelog.sh <latest v* tag or nothing>)" <zip> <setup.exe>`.
- `release.yml`: `on: push: branches: [master]`; version `release` (fails on an existing tag, by design); same as beta but without `--prerelease`, title without "(beta)", and `concurrency: release` so two merges can't race.

- [ ] **Step 1: Add a failing test** `tools/tests/test_workflows.sh` asserting: each workflow file exists; `beta.yml` triggers on `develop` only and contains `--prerelease`; `release.yml` triggers on `master` only, has no `--prerelease`, and has `concurrency`; `_build.yml` contains `workflow_call`, `submodules: recursive`, `1.4.357.0`, `--target Gothic2Notr`, `iscc`; no workflow file contains `GROGAI_API_KEY` or `secrets.` other than `secrets.GITHUB_TOKEN`.
- [ ] **Step 2: Run** `bash tools/tests/run.sh` — expected FAIL.
- [ ] **Step 3: Write the workflows and the composite action.**
- [ ] **Step 4: Run** `bash tools/tests/run.sh` — expected PASS. Lint with `actionlint` if available (`go run github.com/rhysd/actionlint/cmd/actionlint@latest` or skip and note it in the report).
- [ ] **Step 5: Commit** `ci: add build, CI, beta and release workflows`, push `feature/foundation-ci`, open a PR to `develop` (`gh pr create --base develop`). **Verify:** the `ci.yml` run on the PR is green and its artifact contains `GrogIKlamstwa-<v>-win64.zip` and `GrogIKlamstwa-<v>-setup.exe` (`gh run view --log`, `gh run download`).
- [ ] **Step 6: Merge the PR into `develop`** (after the user has looked at it). **Verify:** `beta.yml` succeeds and `gh release list` shows `v0.1.0-beta.<n>` marked Pre-release with both files attached. Work report + INDEX row.

### Task 7: Branch protection and first smoke test by the user

**Files:**
- Modify: `docs/09-repo-ci-and-versioning.md` (record what was enabled), `work/INDEX.md`

- [ ] **Step 1: Ask the user to confirm**, then enable protection on `master` and `develop` with `gh api -X PUT repos/adamcharusta/gothic2-grog-i-klamstwa/branches/<b>/protection`: require a PR, require the `checks` and `engine` status checks from `_build.yml`, disallow force pushes. **Verify:** `gh api repos/adamcharusta/gothic2-grog-i-klamstwa/branches/master/protection` returns those settings.
- [ ] **Step 2: User smoke test** (the agent cannot do this without the game): the user installs the beta setup, points it at the Steam Gothic II folder, starts from the shortcut. Expected: unmodified OpenGothic starts Gothic 2 NotR (main menu visible). Record the result in the work report; if it fails, status `blocked` with the user's description.
- [ ] **Step 3: Commit** `docs: record branch protection and smoke test`. Work report + INDEX row.
