# Engine upstream tracking

The engine is a fork of OpenGothic: https://github.com/adamcharusta/OpenGothic (branch `grog/main`),
mounted in this repo as the `engine/` submodule. Upstream: https://github.com/Try/OpenGothic (`master`).

Every time `grog/main` is cut from or merged with upstream, add a row.

| Date | Upstream `master` SHA | Fork `grog/main` SHA | Note |
|------|-----------------------|----------------------|------|
| 2026-11-07 | f3330c8cbf4d268a3a9f70b37c1e4c67909224f1 | f3330c8cbf4d268a3a9f70b37c1e4c67909224f1 | Fork created; `grog/main` cut from upstream `master`, no changes yet |

## Merging upstream

```bash
cd engine
git remote add upstream https://github.com/Try/OpenGothic.git   # once
git fetch upstream
git merge upstream/master        # on grog/main; resolve conflicts in our hooks only
git push origin grog/main
cd .. && git add engine && git commit -m "build: update engine to upstream <sha>"
```
