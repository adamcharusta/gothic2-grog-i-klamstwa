# AGENTS.md — instructions for agents

Project: **"Grog i kłamstwa"** (Grog and Lies), a Gothic 2 Night of the Raven mod on an **OpenGothic** fork.
A mini-campaign in the Jarkendar pirate camp where an AI Game Master drives the story and the player types their own lines.

## Project language

**Everything in this project is written in English**: docs, plans, work reports, code, identifiers, comments, commit messages.
This holds even when the user talks to you in Polish. Only player-facing game text is bilingual (PL + EN).

## Before you start

1. Read this file.
2. Read [work/INDEX.md](work/INDEX.md): what's done and what's in progress. **Do not redo tasks marked `done`.**
3. Read the docs from the legend below that relate to your task. Check [docs/08-open-questions.md](docs/08-open-questions.md) for unresolved points.

## Document legend

| File | What you'll find there |
|------|------------------------|
| [docs/00-vision.md](docs/00-vision.md) | What the mod is, why (benchmark for full Gothic), goals, non-goals, anti-goals, fixed requirements |
| [docs/01-story-and-scenarios.md](docs/01-story-and-scenarios.md) | Main storyline, scenario/culprit rolling, clue graph, story items, endings |
| [docs/02-characters-and-world.md](docs/02-characters-and-world.md) | NPC cast, character cards, hero, fighting, map bounds, lore |
| [docs/03-game-master.md](docs/03-game-master.md) | AI layer: actor and director, reply format, command catalogue, rules, memory, failure handling |
| [docs/04-architecture-opengothic.md](docs/04-architecture-opengothic.md) | OpenGothic fork, relevant engine code, `aimod/` modules, mod content, distribution |
| [docs/05-ui-and-configuration.md](docs/05-ui-and-configuration.md) | Dialogue window, quest log, AI config file, providers, languages |
| [docs/06-benchmark-and-testing.md](docs/06-benchmark-and-testing.md) | Benchmark logs, summaries, test strategy, success criterion |
| [docs/07-decisions.md](docs/07-decisions.md) | Decision log: who decided what and why |
| [docs/08-open-questions.md](docs/08-open-questions.md) | Questions still waiting for the user |
| [docs/engine-upstream.md](docs/engine-upstream.md) | OpenGothic fork/submodule, upstream SHAs, how to merge upstream |
| [docs/09-repo-ci-and-versioning.md](docs/09-repo-ci-and-versioning.md) | Branches (master/develop), GitHub Actions, release & beta packages, versioning, .gitattributes/.editorconfig/.gitignore |
| [docs/plans/README.md](docs/plans/README.md) | Implementation roadmap and plans (one file per plan) |
| [work/](work/) | Completed task reports + [work/INDEX.md](work/INDEX.md) |

## Code layout

| Path | What |
|------|------|
| `aicore/` | Engine-independent AI library (C++20): config, providers, scenario, world state, prompts, commands, telemetry, session. Tests: `tools\devicore.cmd` (Windows/MSVC) or `cmake -S aicore -B build/aicore && cmake --build build/aicore && ctest --test-dir build/aicore` |
| `tools/grogai-cli/` | Terminal chat with the pirates using `aicore` |
| `mod/data/` | Campaign data: cast, places, NPC cards, lore, rules, pricing |
| `engine/` | OpenGothic fork (submodule) |
| `tools/*.sh`, `tools/tests/` | Packaging/version tools and their bash tests (`bash tools/tests/run.sh`) |
| `packaging/` | Launcher, installer, manifest, third-party notices |

Tags in docs: **[U]** user decision (don't change without asking) · **[P]** designer proposal
(may change, with a reason logged in `07-decisions.md`) · **[B]** left to the builder.

## Working rules

- Never change a **[U]** decision without the user's consent.
- New or changed decision → a row in [docs/07-decisions.md](docs/07-decisions.md) and an update to the relevant doc.
- Keep OpenGothic changes minimal and isolated (`common/aimod/`, small hooks in existing files).
- Git: work on `feature/*` or `fix/*` branches → PR to `develop`; never push directly to `master`. Conventional Commits. LF line endings.
- **Never** commit, log, or print API keys. Never add original Gothic 2 game files to the repo.
- Don't claim something works unless you ran or tested it. In your report, say what was verified and what wasn't.

## Task report (mandatory)

After every finished (or abandoned) task:

1. Create `work/NNNN-short-name.md` (next number, 4 digits) from [work/_TEMPLATE.md](work/_TEMPLATE.md).
2. Add one row to [work/INDEX.md](work/INDEX.md): `| NNNN | date | status | title | file |`.

Statuses: `done` · `partial` (describe what's left) · `blocked` (give the reason) · `abandoned`.
