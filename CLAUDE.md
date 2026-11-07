# CLAUDE.md

The main project instructions are in **[AGENTS.md](AGENTS.md)**. Read it in full before starting:
it has the document legend, working rules, and the mandatory reporting in `work/`.

Legend summary:

| File | Topic |
|------|-------|
| [docs/00-vision.md](docs/00-vision.md) | Vision, goals, non-goals, anti-goals |
| [docs/01-story-and-scenarios.md](docs/01-story-and-scenarios.md) | Story, scenario rolling, endings |
| [docs/02-characters-and-world.md](docs/02-characters-and-world.md) | NPCs, hero, map, lore |
| [docs/03-game-master.md](docs/03-game-master.md) | AI layer (actor/director, commands, rules) |
| [docs/04-architecture-opengothic.md](docs/04-architecture-opengothic.md) | OpenGothic fork, modules, distribution |
| [docs/05-ui-and-configuration.md](docs/05-ui-and-configuration.md) | Dialogue UI, AI provider config |
| [docs/06-benchmark-and-testing.md](docs/06-benchmark-and-testing.md) | Logs, metrics, tests |
| [docs/07-decisions.md](docs/07-decisions.md) | Decision log |
| [docs/08-open-questions.md](docs/08-open-questions.md) | Open questions for the user |
| [docs/engine-upstream.md](docs/engine-upstream.md) | Engine fork and upstream tracking |
| [docs/09-repo-ci-and-versioning.md](docs/09-repo-ci-and-versioning.md) | Branches, GitHub Actions, packages, versioning, repo hygiene files |
| [work/INDEX.md](work/INDEX.md) | What's already done |

Notes for Claude:
- **Write everything in English** (docs, reports, code, commits), even when the user chats in Polish. Reply to the user in their language.
- At session start, read `work/INDEX.md` and the latest reports so you don't repeat work.
- After each task, write a report in `work/` (template: `work/_TEMPLATE.md`) and update the index.
- You're on Windows. The project path contains Polish characters and an en dash ("–"), so always quote paths in shell commands.
