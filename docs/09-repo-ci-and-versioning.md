# 09 — Repository, CI/CD, versioning

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Branches [U]

- `master`: official releases. **A push to `master` builds and publishes the official installation package.**
- `develop`: integration branch. **A push to `develop` builds and publishes a beta package.**
- [P] Work happens on feature branches (`feature/<name>`, `fix/<name>`) merged into `develop` via PR. `develop` is merged into `master`
  via PR when a release is ready. No direct pushes to `master` (branch protection, [B] when the GitHub repo exists).

## GitHub Actions [U]

[P] Workflows:

| Workflow | Trigger | Does |
|----------|---------|------|
| `ci.yml` | PRs and pushes to any branch | Build (Windows x64), unit tests, mock-provider tests, lint data files (JSON schema check of NPC cards, scenarios, prompts) |
| `beta.yml` | push to `develop` | CI + package → **GitHub pre-release** `vX.Y.Z-beta.N` with the package attached |
| `release.yml` | push to `master` | CI + package → git tag `vX.Y.Z` + **GitHub Release** with the package and changelog |

- [P] Package contents: built OpenGothic fork (Windows x64), mod folder, sample config, README PL/EN, LICENSE files
  (ours + OpenGothic MIT + third-party). **Never** any original Gothic files or API keys.
- [P] Package format: a `.zip` (portable) plus a Windows installer `.exe` (Inno Setup) that asks for the Gothic 2 NotR folder
  and creates a shortcut launching the mod (`-game:<mod>.ini`). Both attached to the release.
- [B] Runner setup: Vulkan SDK / glslang and other OpenGothic build deps on `windows-latest` (follow upstream OpenGothic CI),
  caching of the build, submodule checkout.
- [P] Secrets: CI needs no AI keys (tests use the mock provider). Only `GITHUB_TOKEN` for releases.

## Versioning [P]

- **SemVer** for the mod: `MAJOR.MINOR.PATCH`. Before the first public release: `0.x`.
  - MINOR: new features/content; PATCH: fixes; MAJOR: breaking changes (e.g. incompatible saves after 1.0).
- **Single source of truth:** a `VERSION` file in the repo root (e.g. `0.1.0`). It is bumped in the PR that merges `develop` → `master`.
- Release (`master`): tag `v<VERSION>`. The workflow **fails** if that tag already exists (forces a bump).
- Beta (`develop`): `<VERSION>-beta.<run number>`, e.g. `0.2.0-beta.17`, where `VERSION` on `develop` is the *next* planned version.
- Version is compiled into the executable and shown in the main menu, the summary screen and every benchmark log.
- **Save data version** (AI-layer block, see [04](04-architecture-opengothic.md#save-and-load)) is a separate integer, bumped only when
  the save format changes. Saves record both the mod version and the save data version.
- **Data schema versions** (NPC cards, scenario templates) are fields inside the data files, checked at load.
- [P] Commits follow **Conventional Commits** (`feat:`, `fix:`, `docs:`, `chore:`…). The release changelog is generated from them.
- OpenGothic upstream version: the fork records which upstream commit it is based on (in `docs/` or the fork README).

## Repository hygiene files [U]

### `.gitattributes` [U: main EOL is Linux (LF)]

[P] Content:
- `* text=auto eol=lf`: LF by default for all text.
- CRLF exceptions where Windows tools require it: `*.bat`, `*.cmd`, `*.sln`, `*.vcxproj*`, `*.iss` (Inno Setup).
- Gothic Daedalus scripts `*.d` / `*.src`: text, LF, **encoding left as-is (Windows-1250)**; never re-encode to UTF-8.
- Binary: `*.png *.jpg *.tga *.tex *.fnt *.wav *.ogg *.mp3 *.zen *.dat *.vdf *.mod *.mds *.msb *.mrm *.mdl *.mdm *.mdh *.man *.exe *.dll *.zip *.ttf`.
- [B] Git LFS only if large binary assets appear (none expected in this release).

### `.editorconfig` [U]

[P] Content:
- root: `end_of_line = lf`, `insert_final_newline = true`, `trim_trailing_whitespace = true`, `charset = utf-8`.
- C/C++/CMake: 2-space indentation, matching OpenGothic's style (the fork keeps upstream style).
- JSON/YAML/Markdown: 2 spaces; Markdown keeps trailing whitespace (`trim_trailing_whitespace = false`).
- `*.d`, `*.src`: `charset` unset (Windows-1250), indentation as in the original scripts (tabs).
- `*.bat`, `*.cmd`, `*.iss`: `end_of_line = crlf`.

### `.gitignore` [U]

[P] Content:
- build outputs: `build/`, `out/`, `cmake-build-*/`, `CMakeUserPresets.json`, `*.obj`, `*.pdb`, `*.ilk`, `*.exe`, `*.dll` (outside packaging templates);
- IDE: `.vs/`, `.idea/`, `.vscode/*` (except shared `settings.json`/`extensions.json` if we add them);
- runtime: `logs/`, `saves/`, `*.sav`, crash dumps (`*.dmp`);
- **secrets: any local AI config with keys** (`GrogAI.ini`, `*.local.ini`, `.env`). Only `GrogAI.example.ini` is committed;
- **original game files**: `Data/`, `_work/`, `*.vdf`, `*.mod`, `Saves/` and any Gothic install copied for testing;
- packaging output: `dist/`, `*.zip`;
- OS junk: `Thumbs.db`, `.DS_Store`, `desktop.ini`.

## Repository [B]

- Run `git init` here; the default branch is `master`, then create `develop`.
- GitHub repo: the user creates it (or authorises the agent to). Pushing to GitHub needs the user's approval.
- OpenGothic fork: a separate GitHub fork added as a **git submodule** (e.g. `engine/`), so upstream merges stay clean.

## Branch protection (enabled 2026-11-07)

`master` and `develop`: changes only via pull request (0 approvals required, single maintainer), required status checks
`build / checks`, `build / engine`, `build / package`, no force pushes, no deletion. Admins are not forced (`enforce_admins: false`).
