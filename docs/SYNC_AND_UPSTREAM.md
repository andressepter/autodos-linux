# games.json sync, local overlay, and upstream merge requests

## Two files

1. **Primary** (`src/games.json` or `AUTODOS_DB`) — track the upstream database. Refresh with `scripts/sync-games-json.sh` (replace) or `--merge` (upstream wins on key clashes; keys only in your old file are kept).

2. **Local overlay** (`games.local.json` or `AUTODOS_DB_LOCAL`) — your entries and overrides. Same JSON shape (`_meta` + `games`). **Lookup merges primary then overlay; on duplicate keys the overlay wins.** Autosync from scored imports writes here when `AUTODOS_DB_LOCAL` / `--local-db` is set; otherwise it writes the primary file (legacy).

## Merge request workflow

1. Add or tune games in your **local** file (or let autosync fill it).

2. Export a PR fragment (new keys vs upstream main):

   ```bash
   ./build/bin/autodos-cli export-pr --local-db games.local.json
   ```

   Or:

   ```bash
   python3 scripts/export_pr_games_fragment.py \
     --local games.local.json \
     --out games.pr-fragment.json
   ```

   Add `--include-overrides` if you also want keys that exist upstream but differ in your overlay.

3. Fork [makuka97/autoDos](https://github.com/makuka97/autoDos), branch from `main`, merge the `games` object from `games.pr-fragment.json` into `src/games.json` (by hand or JSON merge), bump `_meta` if needed, open a pull request.

4. Optional: install [GitHub CLI](https://cli.github.com/) and use `gh pr create` after pushing your branch.

## autodos-tui

The ncurses UI can set primary/local paths, pick a DOSBox base profile, run **prepare**, **sync** (merge upstream into primary), and **export** the PR fragment (requires a real local overlay file on disk).
