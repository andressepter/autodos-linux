#pragma once
// autodos.h — AutoDOS core library public interface
// Drop any DOS game zip. AutoDOS figures out the rest.

#include <string>
#include <vector>

namespace AutoDOS {

// ── Result from analyze() ─────────────────────────────────────────────────────

struct AnalyzeResult {
    bool        success     = false;
    std::string exe;          // relative path inside extracted zip e.g. "DOOM.EXE"
    std::string workDir;      // subdirectory to cd into e.g. "DAGGER"
    std::string gameType;     // SIMPLE | INSTALLED | CD_BASED | BATCH_LAUNCHER | COMPLEX
    std::string source;       // "database" | "scored" | "batch"
    float       confidence  = 0.0f;
    std::string error;

    // From games.json database entry (may be empty for scored results)
    std::string title;
    std::string cycles;       // e.g. "max limit 35000" or "65000"
    int         memsize     = 16;
    bool        ems         = true;
    bool        xms         = true;
    bool        cdMount     = false;
};

// ── Game library entry ────────────────────────────────────────────────────────

struct GameEntry {
    std::string id;
    std::string title;
    std::string zipPath;
    std::string confPath;
    std::string exe;
    std::string source;    // "database" | "scored"
    bool        cdMount  = false;
};

// ── Core API ──────────────────────────────────────────────────────────────────

// Analyze a zip file. Merges game entries: primary dbPath + optional localDbPath
// (same schema as games.json). On duplicate keys, local overlay wins.
AnalyzeResult analyze(const std::string& zipPath,
                      const std::string& dbPath,
                      const std::string& localDbPath = "");

// Extract zip to outDir
bool extractZip(const std::string& zipPath, const std::string& outDir);

// Write a dosbox.conf: optional base profile (video/audio/global) + appended per-game
// overrides (memsize, cycles, EMS/XMS, autoexec). DOSBox Staging merges duplicate
// sections; later keys override earlier ones.
//
// Resolution when baseConfPath and baseProfileName are both empty:
//   1) AUTODOS_BASE_CONF — path to a .conf file
//   2) Profile name from AUTODOS_BASE_PROFILE, else "default"
//   3) Search config/bases/<name>.conf, then XDG_CONFIG_HOME ~/.config, APPDATA\AutoDOS
//   4) Embedded fallback matching legacy AutoDOS globals
//
// If confOutputPath is empty, writes next to the zip: <zip_stem>.conf
bool writeDosboxConf(const std::string& zipPath,
                     const std::string& extractedDir,
                     const AnalyzeResult& result,
                     const std::string& confOutputPath = "",
                     const std::string& baseConfPath = "",
                     const std::string& baseProfileName = "");

// Candidate paths for config/bases/<name>.conf (for diagnostics / CLI bases)
std::vector<std::string> baseProfileCandidates(const std::string& profileName);

// Fingerprint a filename for database lookup
std::string fingerprint(const std::string& filename);

// Add a game entry to the given JSON file (creates parent dirs / minimal file if needed).
// Point dbPath at games.local.json to keep autosync off the upstream-synced copy.
bool addToDatabase(const std::string& dbPath, const AnalyzeResult& result);

} // namespace AutoDOS
