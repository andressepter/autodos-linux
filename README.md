# autodos-linux

Linux-focused fork of [makuka97/autoDos](https://github.com/makuka97/autoDos): fingerprint ZIPs against a **`games.json`** database, generate **DOSBox** configs, and launch **`dosbox`** / **dosbox-staging** from your distro. No bundled emulator and no Windows GUI—use **`autodos-cli`** or **`autodos-tui`**.

## Dependencies

| Purpose | Packages (Arch examples) |
|--------|---------------------------|
| Build | `cmake`, `gcc`, `make` |
| JSON (CMake FetchContent) | network on first configure |
| Optional TUI | `ncurses` (e.g. `pacman -S ncurses`) |
| Runtime | `dosbox` or `dosbox-staging` on `PATH` |
| Scripts | `python3`, `curl` |

## Build

```bash
git clone https://github.com/andressepter/autodos-linux.git
cd autodos-linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Artifacts:

- `build/bin/autodos-cli` — always built  
- `build/bin/autodos-tui` — built only if CMake finds Curses

## Quick use

```bash
# Optional: copy local DB overlay template
cp games.local.json.example games.local.json

# Analyze a ZIP (uses src/games.json + optional games.local.json)
./build/bin/autodos-cli analyze ./game.zip --db src/games.json --local-db games.local.json

# Full prepare: extract, write .conf next to zip, optional autosync to local DB
./build/bin/autodos-cli prepare ./game.zip --profile window-opengl-hq3x --local-db games.local.json

# Launch (set binary if not `dosbox`)
AUTODOS_DOSBOX=dosbox ./build/bin/autodos-cli launch ./game.conf
```

Interactive menu:

```bash
./build/bin/autodos-tui
```

## Configuration

| Topic | Location / env |
|--------|----------------|
| Game database (primary) | `src/games.json`, or `AUTODOS_DB` / `--db` |
| Overlay (your keys win) | `games.local.json`, or `AUTODOS_DB_LOCAL` / `--local-db` |
| DOSBox base profile | `config/bases/<name>.conf`, or `AUTODOS_BASE_PROFILE`, `AUTODOS_BASE_CONF`, `--profile` / `--base` |
| DOSBox binary | `AUTODOS_DOSBOX` (default `dosbox`) |
| TUI state | `~/.config/autodos/tui-state.json` |
| Repo root (TUI sync) | `AUTODOS_REPO_ROOT` |

See **`config/bases/`** for example global DOSBox snippets (video, MIDI notes). Per-game **`cycles`**, **EMS/XMS**, **`autoexec`** are still appended by the engine.

## Scripts

- **`scripts/sync-games-json.sh`** — refresh primary `games.json` from upstream raw URL (`--merge` keeps keys only you had).  
- **`scripts/export_pr_games_fragment.py`** / **`autodos-cli export-pr`** — build a JSON fragment for a PR back to upstream.

Details: **[docs/SYNC_AND_UPSTREAM.md](docs/SYNC_AND_UPSTREAM.md)**.

## Upstream / history

Original Windows-centric build notes and removed pieces are summarized in **[docs/UPSTREAM_LEGACY.md](docs/UPSTREAM_LEGACY.md)** for future porting or comparison.

## License

Upstream did not ship a `LICENSE` file in-tree at fork time; confirm with [makuka97/autoDos](https://github.com/makuka97/autoDos) before redistributing.
