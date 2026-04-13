// autodos.cpp — AutoDOS core library implementation
// Ported from autodos.js (JavaScript) to C++

#include "autodos.h"
#include "miniz.c"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace AutoDOS {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string basename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

static std::string stemOf(const std::string& filename) {
    std::string b = basename(filename);
    size_t dot = b.rfind('.');
    return (dot == std::string::npos) ? b : b.substr(0, dot);
}

static std::string extOf(const std::string& filename) {
    std::string b = basename(filename);
    size_t dot = b.rfind('.');
    return (dot == std::string::npos) ? "" : toUpper(b.substr(dot + 1));
}

static std::string dirOf(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    size_t pos = normalized.rfind('/');
    return (pos == std::string::npos) ? "" : normalized.substr(0, pos);
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ── Blacklist ─────────────────────────────────────────────────────────────────

static const std::vector<std::string> EXE_BLACKLIST = {
    "setup", "install", "uninst", "uninstall", "patch", "update",
    "config", "cfg", "register", "readme", "read", "help",
    "directx", "dxsetup", "vcredist", "dotnet",
    "dos4gw", "cwsdpmi", "himemx", "emm386",
    "fixsave", "fix", "convert", "copy", "move"
};

static bool isBlacklisted(const std::string& stem) {
    std::string lower = toLower(stem);
    for (const auto& b : EXE_BLACKLIST) {
        if (lower == b) return true;
    }
    return false;
}

// ── ZIP entry ─────────────────────────────────────────────────────────────────

struct ZipEntry {
    std::string name;      // full path, normalized slashes
    int         depth = 0;
    std::string ext;
    std::string base;      // uppercase filename
    size_t      compSize = 0;
};

// ── ZIP reader ────────────────────────────────────────────────────────────────

static std::vector<ZipEntry> readZipEntries(const std::string& zipPath) {
    std::vector<ZipEntry> entries;

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0))
        return entries;

    int count = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < count; i++) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        std::string rawName = stat.m_filename;
        // Normalize backslashes
        std::replace(rawName.begin(), rawName.end(), '\\', '/');

        // Split into parts
        std::vector<std::string> parts;
        std::stringstream ss(rawName);
        std::string part;
        while (std::getline(ss, part, '/')) {
            if (!part.empty()) parts.push_back(part);
        }
        if (parts.empty()) continue;

        ZipEntry e;
        e.name     = rawName;
        e.depth    = (int)parts.size() - 1;
        e.base     = toUpper(parts.back());
        e.ext      = extOf(e.base);
        e.compSize = (size_t)stat.m_comp_size;
        entries.push_back(e);
    }

    mz_zip_reader_end(&zip);
    return entries;
}

// ── Normalize entries (strip single top-level wrapper) ───────────────────────

static std::vector<ZipEntry> normalizeEntries(std::vector<ZipEntry> entries) {
    if (entries.empty()) return entries;

    // Find top-level folders
    std::string firstTop;
    bool allSame = true;
    for (const auto& e : entries) {
        size_t slash = e.name.find('/');
        std::string top = (slash != std::string::npos) ? e.name.substr(0, slash) : "";
        if (firstTop.empty()) {
            firstTop = top;
        } else if (top != firstTop) {
            allSame = false;
            break;
        }
    }

    // Do NOT strip top-level folder — we need full relative paths
    // so the conf writer can correctly set the working directory.
    // e.g. mastori/ORION.EXE → dirOf → "mastori" → cd \mastori in conf
    (void)allSame; (void)firstTop;

    return entries;
}

// ── Fingerprint ───────────────────────────────────────────────────────────────

std::string fingerprint(const std::string& filename) {
    std::string name = basename(filename);
    // Remove extension
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    name = toLower(name);

    // Strip DOSBOX_ prefix
    if (name.substr(0, 7) == "dosbox_" || name.substr(0, 7) == "dosbox-")
        name = name.substr(7);

    // Strip (region), [flags]
    name = std::regex_replace(name, std::regex(R"([\s\-_]*[\(\[][^\)\]]*[\)\]])"), "");

    // Strip version strings
    name = std::regex_replace(name, std::regex(R"([\s\-_]*v\d[\d\.]*)"), "");

    // Replace separators with space then collapse
    name = std::regex_replace(name, std::regex(R"(\s*[-:]\s*)"), " ");
    name = std::regex_replace(name, std::regex(R"([\s\-_:,'\.\!\?]+)"), "");

    // Strip leading article only if first WORD of original was an article
    // (prevents "theme" → strip "the" → "me")
    std::string orig = toLower(basename(filename));
    dot = orig.rfind('.');
    if (dot != std::string::npos) orig = orig.substr(0, dot);
    // Get first word
    size_t wordEnd = orig.find_first_of(" -_");
    std::string firstWord = (wordEnd != std::string::npos) ? orig.substr(0, wordEnd) : orig;
    if (firstWord == "the" || firstWord == "a" || firstWord == "an") {
        if (name.substr(0, firstWord.size()) == firstWord)
            name = name.substr(firstWord.size());
    }

    return name;
}

// ── Game type classifier ──────────────────────────────────────────────────────

static std::string classifyGameType(const std::vector<ZipEntry>& entries) {
    for (const auto& e : entries) {
        if (e.ext == "ISO" || e.ext == "CUE" || e.ext == "BIN" || e.ext == "MDF")
            return "CD_BASED";
    }
    for (const auto& e : entries) {
        if (e.ext == "BAT") {
            std::string lower = toLower(e.base);
            if (lower == "start.bat" || lower == "run.bat" ||
                lower == "go.bat"    || lower == "launch.bat" ||
                lower == "play.bat"  || lower == "game.bat")
                return "BATCH_LAUNCHER";
        }
    }
    for (const auto& e : entries) {
        std::string lower = toLower(e.base);
        if (lower == "install.exe" || lower == "setup.exe" ||
            lower == "install.bat" || lower == "setup.bat")
            return "INSTALLED";
    }
    if (entries.size() > 50) return "COMPLEX";
    return "SIMPLE";
}

// ── Exe scorer ────────────────────────────────────────────────────────────────

static float scoreExe(const ZipEntry& e, const std::string& zipBase, size_t maxCompSize) {
    std::string stem = toLower(stemOf(e.base));

    if (isBlacklisted(stem)) return 0.0f;

    float score = 0.0f;
    if      (e.ext == "EXE") score = 1.0f;
    else if (e.ext == "COM") score = 0.6f;
    else if (e.ext == "BAT") score = 0.7f;
    else return 0.0f;

    // Depth penalty
    score -= e.depth * 0.15f;
    if (score < 0.01f) score = 0.01f;

    // Name matches zip name
    std::string zipStem = toLower(stemOf(zipBase));
    if (stem == zipStem ||
        stem.find(zipStem) == 0 ||
        zipStem.find(stem) == 0) {
        score += 0.3f;
    }

    // Common game exe patterns
    if (stem == "game" || stem == "play" || stem == "start" ||
        stem == "run"  || stem == "main" || stem == "go") {
        score += 0.15f;
    }

    // Size bonus
    if (maxCompSize > 0) {
        score += ((float)e.compSize / (float)maxCompSize) * 0.25f;
    }

    return std::min(score, 1.5f);
}

// ── Database loader ───────────────────────────────────────────────────────────

static json loadGamesFromFile(const std::string& dbPath) {
    std::ifstream f(dbPath);
    if (!f.is_open()) return json::object();
    json data;
    try {
        f >> data;
        return data.value("games", json::object());
    } catch (...) {
        return json::object();
    }
}

static json mergeGamesFromFiles(const std::string& primaryPath, const std::string& localPath) {
    json merged = loadGamesFromFile(primaryPath);
    if (localPath.empty())
        return merged;
    json loc = loadGamesFromFile(localPath);
    for (auto it = loc.begin(); it != loc.end(); ++it)
        merged[it.key()] = it.value();
    return merged;
}

// ── Find ISO in extracted dir ─────────────────────────────────────────────────

static std::string findIsoInDir(const std::string& dir) {
    try {
        const fs::path root(dir);
        if (!fs::exists(root) || !fs::is_directory(root))
            return "";
        for (const auto& entry : fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file())
                continue;
            std::string ext = toUpper(entry.path().extension().string());
            if (ext.size() > 0 && ext[0] == '.')
                ext = ext.substr(1);
            if (ext == "ISO" || ext == "CUE" || ext == "BIN" || ext == "MDF")
                return entry.path().string();
        }
    } catch (...) {
    }
    return {};
}

static bool zipPathHasParentReference(const fs::path& rel) {
    for (const auto& part : rel) {
        if (part == "..")
            return true;
    }
    return false;
}

// ── Main analyze function ─────────────────────────────────────────────────────

AnalyzeResult analyze(const std::string& zipPath,
                      const std::string& dbPath,
                      const std::string& localDbPath) {
    AnalyzeResult result;

    // Read zip entries
    auto rawEntries = readZipEntries(zipPath);
    if (rawEntries.empty()) {
        result.error = "Zip is empty or unreadable";
        return result;
    }

    auto entries     = normalizeEntries(rawEntries);
    std::string fp   = fingerprint(zipPath);
    std::string gt   = classifyGameType(entries);
    result.gameType  = gt;

    // ── Layer 1: Database lookup ──────────────────────────────────────────────
    json db = mergeGamesFromFiles(dbPath, localDbPath);

    json dbEntry;
    if (db.contains(fp)) {
        dbEntry = db[fp];
    } else {
        // Substring match
        for (auto it = db.begin(); it != db.end(); ++it) {
            if (fp.find(it.key()) != std::string::npos) {
                dbEntry = it.value();
                break;
            }
        }
    }

    if (!dbEntry.is_null()) {
        std::string exeName = toUpper(dbEntry.value("exe", ""));
        // Verify exe exists in zip
        for (const auto& e : entries) {
            if (e.base == exeName) {
                result.success    = true;
                result.exe        = e.name;
                result.workDir    = dbEntry.value("work_dir", "");
                result.source     = "database";
                result.confidence = 1.0f;
                result.title      = dbEntry.value("title", "");
                // cycles can be int or string in games.json
                if (dbEntry.contains("cycles")) {
                    auto& cyc = dbEntry["cycles"];
                    if (cyc.is_string())       result.cycles = cyc.get<std::string>();
                    else if (cyc.is_number())  result.cycles = std::to_string(cyc.get<int>());
                    else                       result.cycles = "max limit 80000";
                } else {
                    result.cycles = "max limit 80000";
                }
                result.memsize    = dbEntry.value("memsize", 16);
                result.ems        = dbEntry.value("ems", true);
                result.xms        = dbEntry.value("xms", true);
                result.cdMount    = dbEntry.value("cd_mount", false);
                return result;
            }
        }
    }

    // ── Layer 2: Batch launcher ───────────────────────────────────────────────
    if (gt == "BATCH_LAUNCHER") {
        for (const auto& e : entries) {
            if (e.ext == "BAT") {
                std::string lower = toLower(e.base);
                if (lower == "start.bat" || lower == "run.bat" || lower == "go.bat") {
                    result.success    = true;
                    result.exe        = e.name;
                    result.workDir    = dirOf(e.name);
                    result.source     = "batch";
                    result.confidence = 0.85f;
                    return result;
                }
            }
        }
    }

    // ── Layer 3: Scorer ───────────────────────────────────────────────────────
    std::vector<ZipEntry> exeEntries;
    for (const auto& e : entries) {
        if (e.ext == "EXE" || e.ext == "COM" || e.ext == "BAT")
            exeEntries.push_back(e);
    }

    size_t maxCompSize = 0;
    for (const auto& e : exeEntries) {
        if (e.compSize > maxCompSize) maxCompSize = e.compSize;
    }

    std::string zipBase = basename(zipPath);

    std::vector<std::pair<float, ZipEntry>> scored;
    for (const auto& e : exeEntries) {
        float s = scoreExe(e, zipBase, maxCompSize);
        if (s > 0.0f) scored.push_back({s, e});
    }

    if (scored.empty()) {
        result.error = "No executable files found in zip";
        return result;
    }

    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    const auto& best = scored[0];
    result.success    = true;
    result.exe        = best.second.name;
    result.workDir    = dirOf(best.second.name);
    result.source     = "scored";
    result.confidence = std::min(best.first / 1.5f, 1.0f);

    // Auto-detect CD mount from game type
    if (gt == "CD_BASED") result.cdMount = true;

    return result;
}

// ── Extract zip ───────────────────────────────────────────────────────────────

bool extractZip(const std::string& zipPath, const std::string& outDir) {
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) return false;

    int count = (int)mz_zip_reader_get_num_files(&zip);
    const fs::path root(outDir);

    for (int i = 0; i < count; i++) {
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

        std::string name = stat.m_filename;
        std::replace(name.begin(), name.end(), '\\', '/');
        if (name.empty()) continue;
        if (name.size() > 1 && name[1] == ':') continue;

        fs::path rel = fs::path(name).lexically_normal();
        if (zipPathHasParentReference(rel)) continue;
        if (rel.is_absolute()) continue;

        fs::path outPath = root / rel;
        std::error_code ec;
        fs::create_directories(outPath.parent_path(), ec);

        try {
            mz_zip_reader_extract_to_file(&zip, i, outPath.string().c_str(), 0);
        } catch (...) {
        }
    }

    mz_zip_reader_end(&zip);
    return true;
}

// ── Write DOSBox conf ─────────────────────────────────────────────────────────

static std::string toDosCdPath(std::string s) {
    std::replace(s.begin(), s.end(), '/', '\\');
    return s;
}

// Legacy global sections (no per-game keys). Kept in sync with config/bases/default.conf.
static std::string builtInBaseConf() {
    return R"([sdl]
fullscreen=true
fullresolution=desktop
output=openglnb

[dosbox]
machine=svga_s3

[mixer]
rate=44100
blocksize=1024
prebuffer=20

[render]
frameskip=0
aspect=true

)";
}

static std::vector<fs::path> profileSearchPathsOrdered(const std::string& name) {
    std::vector<fs::path> c;
    c.push_back(fs::path("config") / "bases" / (name + ".conf"));
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        c.push_back(fs::path(xdg) / "autodos" / "bases" / (name + ".conf"));
    if (const char* home = std::getenv("HOME"))
        c.push_back(fs::path(home) / ".config" / "autodos" / "bases" / (name + ".conf"));
    return c;
}

std::vector<std::string> baseProfileCandidates(const std::string& profileName) {
    std::vector<std::string> out;
    for (const auto& p : profileSearchPathsOrdered(profileName))
        out.push_back(p.generic_string());
    return out;
}

static std::string firstExistingProfileFile(const std::string& name) {
    for (const auto& p : profileSearchPathsOrdered(name)) {
        std::error_code ec;
        if (fs::is_regular_file(p, ec))
            return p.string();
    }
    return {};
}

static bool readEntireFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open())
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool loadBaseConfText(const std::string& explicitFile,
                             const std::string& explicitProfile,
                             std::string& outText,
                             std::string& error) {
    outText.clear();
    error.clear();

    if (!explicitFile.empty()) {
        if (!readEntireFile(explicitFile, outText)) {
            error = "cannot read base conf: " + explicitFile;
            return false;
        }
        return true;
    }

    if (const char* envPath = std::getenv("AUTODOS_BASE_CONF")) {
        if (envPath[0] != '\0' && readEntireFile(envPath, outText))
            return true;
    }

    std::string prof = explicitProfile;
    if (prof.empty()) {
        if (const char* ep = std::getenv("AUTODOS_BASE_PROFILE"))
            prof = ep;
    }
    if (prof.empty())
        prof = "default";

    std::string found = firstExistingProfileFile(prof);
    if (!found.empty() && readEntireFile(found, outText))
        return true;

    outText = builtInBaseConf();
    return true;
}

static std::string buildGameOverrideConf(const AnalyzeResult& result, const std::string& extractedDir) {
    std::string exeFull = result.exe;
    std::replace(exeFull.begin(), exeFull.end(), '\\', '/');
    size_t lastSlash = exeFull.rfind('/');
    std::string exeName = (lastSlash != std::string::npos) ? exeFull.substr(lastSlash + 1) : exeFull;
    std::string exeSubDir = (lastSlash != std::string::npos) ? exeFull.substr(0, lastSlash) : "";

    std::string cdDir = result.workDir.empty() ? exeSubDir : result.workDir;
    std::string cdDos = toDosCdPath(cdDir);

    std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
    int         memsize = result.memsize > 0 ? result.memsize : 16;

    std::string mountHost = fs::path(extractedDir).make_preferred().string();

    std::ostringstream autoexec;
    autoexec << "@echo off\r\n";
    autoexec << "mount C \"" << mountHost << "\"\r\n";

    if (result.cdMount) {
        std::string isoPath = findIsoInDir(extractedDir);
        if (!isoPath.empty()) {
            std::string isoHost = fs::path(isoPath).make_preferred().string();
            autoexec << "imgmount D \"" << isoHost << "\" -t iso\r\n";
        }
    }

    autoexec << "C:\r\n";
    if (!cdDos.empty()) {
        if (cdDos[0] == '\\')
            autoexec << "cd " << cdDos << "\r\n";
        else
            autoexec << "cd \\" << cdDos << "\r\n";
    }
    autoexec << exeName << "\r\n";
    autoexec << "exit\r\n";

    std::ostringstream tail;
    tail << "\r\n; --- AutoDOS per-game (append; overrides same keys in base) ---\r\n";
    tail << "[dosbox]\r\n";
    tail << "memsize=" << memsize << "\r\n\r\n";
    tail << "[cpu]\r\n";
    tail << "core=dynamic\r\n";
    tail << "cputype=pentium_slow\r\n";
    tail << "cycles=" << cycles << "\r\n";
    tail << "cycleup=500\r\n";
    tail << "cycledown=20\r\n\r\n";
    tail << "[dos]\r\n";
    tail << "ems=" << (result.ems ? "true" : "false") << "\r\n";
    tail << "xms=" << (result.xms ? "true" : "false") << "\r\n\r\n";
    tail << "[autoexec]\r\n";
    tail << autoexec.str();
    tail << "\r\n";
    return tail.str();
}

bool writeDosboxConf(const std::string& zipPath,
                     const std::string& extractedDir,
                     const AnalyzeResult& result,
                     const std::string& confOutputPath,
                     const std::string& baseConfPath,
                     const std::string& baseProfileName) {
    std::string confPath = confOutputPath.empty()
        ? zipPath.substr(0, zipPath.rfind('.')) + ".conf"
        : confOutputPath;

    std::string baseText;
    std::string err;
    if (!loadBaseConfText(baseConfPath, baseProfileName, baseText, err))
        return false;

    if (!baseText.empty() && baseText.back() != '\n' && baseText.back() != '\r')
        baseText += "\r\n";

    std::string full = std::move(baseText) + buildGameOverrideConf(result, extractedDir);

    std::ofstream f(confPath);
    if (!f.is_open())
        return false;
    f << full;
    return true;
}

// ── Add to database ───────────────────────────────────────────────────────────

bool addToDatabase(const std::string& dbPath, const AnalyzeResult& result) {
    try {
        fs::path p(dbPath);
        if (p.has_parent_path())
            fs::create_directories(p.parent_path());
    } catch (...) {
    }

    json data;
    std::ifstream fin(dbPath);
    if (fin.is_open()) {
        try { fin >> data; } catch (...) {}
        fin.close();
    }
    if (!data.contains("games") || !data["games"].is_object())
        data["games"] = json::object();

    std::string key = fingerprint(result.title.empty() ? result.exe : result.title);
    if (key.empty()) return false;

    json& games = data["games"];

    // Don't overwrite manual entries
    if (games.contains(key) && games[key].value("source", "") == "manual")
        return false;

    games[key] = {
        {"title",         result.title},
        {"exe",           basename(result.exe)},
        {"cycles",        result.cycles.empty() ? "max limit 80000" : result.cycles},
        {"memsize",       result.memsize},
        {"ems",           result.ems},
        {"xms",           result.xms},
        {"cd_mount",      result.cdMount},
        {"work_dir",      result.workDir},
        {"install_first", false},
        {"source",        "autosync"},
    };

    data["_meta"]["games"] = games.size();

    std::ofstream fout(dbPath);
    if (!fout.is_open()) return false;
    fout << data.dump(2);
    return true;
}

} // namespace AutoDOS
