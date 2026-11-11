# 07 — Decision log

Every significant decision goes here (date, who, decision, why). Changing a decision = a new row, never delete old ones.

| Date | Who | Decision | Why |
|------|-----|----------|-----|
| 2026-11-07 | user | Engine: OpenGothic, mandatory | Project requirement |
| 2026-11-07 | user | Scope: Jarkendar pirate camp + a few metres beyond the palisade | Mini-campaign |
| 2026-11-07 | user | Cast: camp pirates + Greg, Skip, Angus, Hank, Malcolm | — |
| 2026-11-07 | user | Story: find Greg's favourite tankard; AI as Game Master with a main storyline | Pen-and-paper RPG feel |
| 2026-11-07 | user | Random background (dice / lost / stolen / in his hut) and random culprit | Replayability |
| 2026-11-07 | user | Main ending: tankard to Greg → key → chest with captain's outfit; deception or failure possible | — |
| 2026-11-07 | user | Hero: Nameless Hero in pirate clothes, mid level | — |
| 2026-11-07 | user | Legal copy of G2 NotR required | OpenGothic uses original files |
| 2026-11-07 | user | Player supplies API key; any provider and model | Flexibility, benchmark |
| 2026-11-07 | user | Dialogue: text field only + "End" button | — |
| 2026-11-07 | user | AI-driven random events | Test of live game manipulation |
| 2026-11-07 | user | 30–60 min playthrough; mod is a benchmark for full Gothic | — |
| 2026-11-07 | user | Player-facing languages PL and EN | — |
| 2026-11-07 | user | Voice acting in the next release | — |
| 2026-11-07 | user | All project docs, code and agent work in English, even when chatting in Polish | Project-wide convention |
| 2026-11-07 | designer | Code rolls the scenario (seeded), not the AI | Consistency, reproducible benchmark |
| 2026-11-07 | designer | Culprit can be any pirate except Greg | Greg is the quest giver |
| 2026-11-07 | designer | "AI proposes, game decides": validate every command | Anti-goal: stuck story, nonsense |
| 2026-11-07 | designer | Role split: actor (character knowledge) / director (full truth) | Secret cannot leak |
| 2026-11-07 | designer | Bound the map with barriers, don't cut the ZEN | Simpler, safer |
| 2026-11-07 | designer | AI logic in the C++ fork, minimal Daedalus | OpenGothic lacks Ikarus/LeGo |
| 2026-11-07 | designer | Default provider Claude; native Anthropic API + OpenAI-compatible standard | Covers almost all providers |
| 2026-11-07 | designer | System prompts in English, replies in the player's language | Instruction-following quality |
| 2026-11-07 | designer | Windows first | User's platform |
| 2026-11-07 | designer | ~~Polish speech requires a Polish game edition; custom fonts deferred~~ (superseded below) | Engine uses original single-byte fonts |
| 2026-11-07 | user | Save/load must work, including mid-investigation | — |
| 2026-11-07 | user | Cost/latency targets set from playtest measurements, not up front | — |
| 2026-11-07 | user | Testing on the Steam edition of G2 NotR; user has API keys | — |
| 2026-11-07 | designer | ~~Mod ships its own fonts with PL+EN glyphs~~ (superseded below) | Assumed the Steam edition lacked Polish glyphs |
| 2026-11-07 | user | The user's Steam copy has Polish texts (like the original Polish release) | — |
| 2026-11-07 | designer | Use the install's original fonts; convert AI text to its code page; transliterate Polish diacritics on non-Polish installs; custom fonts deferred to a later release | Test install is Polish, so no custom fonts needed now |
| 2026-11-07 | user | GitHub Actions; branches `master` (push → official package) and `develop` (push → beta package) | Release pipeline |
| 2026-11-07 | user | Add `.editorconfig`, `.gitignore`, `.gitattributes`; main EOL is LF | Repo hygiene |
| 2026-11-07 | designer | SemVer from a `VERSION` file; betas `X.Y.Z-beta.<run>`; separate save-data version; Conventional Commits changelog | Simple, single source of truth |
| 2026-11-07 | designer | Package = zip + Inno Setup installer, published as GitHub Release / pre-release | Easy install for players |
| 2026-11-07 | designer | OpenGothic fork as a git submodule | Clean upstream merges |
| 2026-11-07 | designer | AI-layer state serialised with OpenGothic saves, versioned | Follows from save/load requirement; needed for full Gothic too |
| 2026-11-07 | designer | Build order: vertical slice first (Greg → Skip → Matt, one scenario type, one provider, no events), then the full release | De-risk engine hooks early; release scope unchanged |
| 2026-11-07 | user | Design approved; public repo `adamcharusta/gothic2-grog-i-klamstwa` created | — |
| 2026-11-07 | designer | Project licence MIT (same as OpenGothic), copyright Adam Charusta | Public repo needs a licence; user may change it |
| 2026-11-07 | designer | Implementation split into 5 plans (docs/plans/README.md); only the next plan is written in detail | Later plans depend on findings |
| 2026-11-07 | designer | AI layer lives in `aicore/` in the main repo (not `engine/common/aimod/`); the engine links it in Plan 3 | Builds/tests in seconds without the engine; keeps the fork diff tiny |
| 2026-11-07 | designer | No hard-coded model prices; `mod/data/pricing.json` is filled by the user | Avoid wrong cost figures |
| 2026-11-11 | designer | Greg's tankard / look-alike are mission items `ITMI_CORAGONSSILBER` / `ITMI_BLOODCUP_MIS` (never in Jarkendar), renamed in game via items.json `game_names`/`game_texts` | Vanilla silver/gold cups lie all over the camp and counted as the tankard (playtest 1) |
| 2026-11-11 | designer | Supersedes "no hard-coded prices": `pricing.json` ships OpenAI 4.1/4o list prices; dated snapshot names use the base name's price | Playtest showed cost 0 for every call; users rarely fill the file. Prices still need checking against the provider's list |
| 2026-11-11 | designer | AI-driven pirates have no script infos (`Npc_CheckInfo` = false); `start_fight` is a fist fight started after the dialogue closes (weapons put away, script `B_Attack` with `AR_NONE`) | Greg accosted the player forever; fights did not start or used weapons (playtest 1) |
| 2026-11-11 | designer | Hero starts with a pirate sabre, 50 gold and 2 grog (`start.json` `items`) | Starting straight in Jarkendar left an empty inventory |
| 2026-11-11 | designer | Provider errors: one retry for timeout/server/network only; one in-game hint per error kind; HTTP 400/404 = configuration error | Retrying key/quota errors only costs money; players need to know what to fix |
