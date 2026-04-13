# Upstream (makuka97/autoDos) — legacy context for this fork

Use this when comparing behavior, porting features, or answering “what did the original do?”

## Source

- **Repository:** https://github.com/makuka97/autoDos  
- **Purpose (author):** Automation on top of **DOSBox Staging**—drop a DOS ZIP, fingerprint filename against **`games.json`**, write a tailored **`dosbox.conf`**, spawn DOSBox. Unknown ZIPs use a heuristic **EXE scorer** and can be **autosynced** into the DB.

## Stack removed in autodos-linux

| Piece | Notes |
|--------|--------|
| **`src/main.cpp`** | Win32 GUI: drag/drop archives, listbox library, **`%APPDATA%\AutoDOS`** for `games.json` + `library.json`, **`dosbox\dosbox.exe`** next to the app. |
| **`src/launch_win32.cpp`** | **`CreateProcessA`** with a constructed command line, **`DETACHED_PROCESS`**. Replaced here by **`launch_posix.cpp`** (`fork` + `setsid` + **`execlp`**). |
| **`build.ps1`** | PowerShell build/packaging; assumed **Visual Studio** + **`dosbox\dosbox.exe`**. |
| **`BUILD.md`** | VS 2022, **CARTRIDGE**-sourced **`games.json`** and **dosbox-staging-win** tree; **miniz** downloaded into **`src/`**. |

## Original build layout (reference)

```
AutoDOS/
├── src/           main.cpp (Win32), autodos.cpp, autodos.h, miniz.*, games.json
├── dosbox/        dosbox.exe (bundled Windows DOSBox Staging from CARTRIDGE)
├── assets/        optional icon.rc
├── CMakeLists.txt
└── BUILD.md
```

**CMake** originally: `WIN32` **`AutoDOS`** target + **nlohmann/json** FetchContent. **Raylib** was mentioned in old docs but the tree used **native Win32** controls, not Raylib.

## Dependencies (upstream)

- **miniz** — single-file ZIP; still pulled in via **`#include "miniz.c"`** inside **`autodos.cpp`** in this fork (do not compile **`miniz.c`** as its own TU).  
- **games.json** — canonical path in upstream tree **`src/games.json`**; also copied to **`%APPDATA%\AutoDOS\`** on first Win32 run.  
- **DOSBox** — expected as **`.\dosbox\dosbox.exe`** relative to the built **`AutoDOS.exe`**.

## Product copy (high level)

Upstream README emphasized: no manual cycles/EMS/CD tuning for known games; **filename fingerprint** + DB; **scorer** for unknowns; analogy to dropping a ROM into a libretro-style frontend. **Caveat for implementers:** “fingerprint” is **normalized archive basename**, not a content hash—see code in **`fingerprint()`** and **`analyze()`** layers.

## Tools mentioned upstream, not in this repo

- **`autodos_scraper.py`** — referenced in old **BUILD.md** for adding DB rows (`--add --title ...`); may live only in author’s environment or another branch—verify upstream before relying on it.

## Sync without git remote

This fork uses **`scripts/sync-games-json.sh`** against the **raw** upstream **`games.json`** instead of tracking **`makuka97/autoDos`** as a **git remote** for day-to-day work. See **`docs/SYNC_AND_UPSTREAM.md`**.

## Possible future ports from upstream

- Restore **Win32 GUI** + **`launch_win32`** if dual-target builds return.  
- Optional **bundled DOSBox** for reproducible Windows builds.  
- **library.json**-style persistent game list (paths + conf paths) if parity with original UX is desired—currently covered partly by **TUI state** + shell scripts.
