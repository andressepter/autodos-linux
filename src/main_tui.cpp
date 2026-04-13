// autodos-tui — ncurses UI: DB paths, DOSBox base profile, prepare / launch / sync / export.

#include "autodos.h"
#include "autodos_launch.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ncurses.h>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct State {
    std::string repoRoot;
    std::string primaryDb{"src/games.json"};
    std::string localDb{"games.local.json"};
    std::string profile{"default"};
    std::string zipPath;
    std::string lastConf;
};

static std::string configDir() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::string(xdg) + "/autodos";
    if (const char* h = std::getenv("HOME"))
        return std::string(h) + "/.config/autodos";
    return ".config/autodos";
}

static std::string statePath() { return configDir() + "/tui-state.json"; }

static void loadState(State& s) {
    std::ifstream f(statePath());
    if (!f.is_open()) return;
    try {
        json j;
        f >> j;
        if (j.contains("primaryDb")) s.primaryDb = j["primaryDb"].get<std::string>();
        if (j.contains("localDb")) s.localDb = j["localDb"].get<std::string>();
        if (j.contains("profile")) s.profile = j["profile"].get<std::string>();
        if (j.contains("zipPath")) s.zipPath = j["zipPath"].get<std::string>();
        if (j.contains("lastConf")) s.lastConf = j["lastConf"].get<std::string>();
        if (j.contains("repoRoot")) s.repoRoot = j["repoRoot"].get<std::string>();
    } catch (...) {
    }
}

static void saveState(const State& s) {
    try {
        fs::create_directories(configDir());
    } catch (...) {
    }
    json j = {{"primaryDb", s.primaryDb},
              {"localDb", s.localDb},
              {"profile", s.profile},
              {"zipPath", s.zipPath},
              {"lastConf", s.lastConf},
              {"repoRoot", s.repoRoot}};
    std::ofstream f(statePath());
    if (f.is_open()) f << j.dump(2) << "\n";
}

static std::string detectRepoRoot() {
    if (const char* e = std::getenv("AUTODOS_REPO_ROOT"))
        return e;
    fs::path cwd = fs::current_path();
    for (int i = 0; i < 6; ++i) {
        if (fs::is_regular_file(cwd / "scripts/sync-games-json.sh"))
            return cwd.string();
        if (fs::is_regular_file(cwd / "CMakeLists.txt") &&
            fs::is_directory(cwd / "src"))
            return cwd.string();
        if (!cwd.has_parent_path()) break;
        cwd = cwd.parent_path();
    }
    return fs::current_path().string();
}

static void collectProfileNames(std::vector<std::string>& out) {
    std::set<std::string> names;
    auto scan = [&names](const fs::path& dir) {
        try {
            if (!fs::is_directory(dir)) return;
            for (auto const& e : fs::directory_iterator(dir)) {
                if (!e.is_regular_file()) continue;
                if (e.path().extension() == ".conf")
                    names.insert(e.path().stem().string());
            }
        } catch (...) {
        }
    };
    scan(fs::path("config") / "bases");
    scan(fs::path(configDir()) / "bases");
    out.assign(names.begin(), names.end());
}

static void draw(const State& s, int sel, const std::vector<std::string>& profiles) {
    erase();
    mvprintw(0, 0, " autodos-tui  —  q quit | Enter activate row");
    mvprintw(2, 0, " Repo root: %s", s.repoRoot.c_str());
    mvprintw(3, 0, " Primary DB (upstream sync target): %s", s.primaryDb.c_str());
    mvprintw(4, 0, " Local overlay (optional, overrides primary keys): %s",
             s.localDb.empty() ? "(none)" : s.localDb.c_str());
    std::string profLine = " Base profile: " + s.profile;
    if (!profiles.empty()) {
        profLine += "  [";
        for (size_t i = 0; i < profiles.size() && i < 5; ++i) {
            if (i) profLine += ", ";
            profLine += profiles[i];
        }
        if (profiles.size() > 5) profLine += ", ...";
        profLine += "]";
    }
    mvprintw(5, 0, "%s", profLine.c_str());
    mvprintw(6, 0, " Zip: %s", s.zipPath.empty() ? "(not set)" : s.zipPath.c_str());
    mvprintw(7, 0, " Last .conf: %s", s.lastConf.empty() ? "(none)" : s.lastConf.c_str());

    const char* rows[] = {"Edit primary DB path",
                          "Edit local overlay path (empty = disable)",
                          "Pick base profile (cycle)",
                          "Edit zip path",
                          "Prepare (analyze + extract + write .conf)",
                          "Launch DOSBox (last .conf)",
                          "Sync primary DB from upstream (--merge)",
                          "Export local-only keys for upstream PR",
                          "Save state & quit"};
    int baseRow = 9;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        if ((int)i == sel)
            attron(A_REVERSE);
        mvprintw(baseRow + (int)i, 2, "%s", rows[i]);
        if ((int)i == sel)
            attroff(A_REVERSE);
    }
    mvprintw(LINES - 2, 0, " j/k move ");
    refresh();
}

static std::string trim(std::string s) {
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    return s.substr(i);
}

// If allowEmptyClear, user may clear with an empty line.
static std::string promptLine(const char* label, const std::string& current, bool allowEmptyClear = false) {
    echo();
    curs_set(1);
    char buf[4096];
    std::memset(buf, 0, sizeof(buf));
    mvprintw(LINES - 1, 0, "%s", label);
    clrtoeol();
    mvprintw(LINES - 1, 0, "%s [%s] ", label, current.c_str());
    int col = (int)std::strlen(label) + (int)current.size() + 3;
    int r   = mvgetnstr(LINES - 1, col, buf, (int)sizeof(buf) - 1);
    noecho();
    curs_set(0);
    if (r == ERR)
        return current;
    std::string s = trim(buf);
    if (s.empty() && allowEmptyClear)
        return "";
    if (s.empty())
        return current;
    return s;
}

static void cycleProfile(State& s, const std::vector<std::string>& profiles) {
    if (profiles.empty()) {
        s.profile = promptLine("Profile name", s.profile);
        return;
    }
    auto it = std::find(profiles.begin(), profiles.end(), s.profile);
    if (it == profiles.end())
        s.profile = profiles.front();
    else {
        ++it;
        if (it == profiles.end()) it = profiles.begin();
        s.profile = *it;
    }
}

static int runPrepare(State& s) {
    if (s.zipPath.empty() || !fs::exists(s.zipPath))
        return -1;
    if (!fs::exists(s.primaryDb) && s.localDb.empty()) {
        // allow local-only
    }
    AutoDOS::AnalyzeResult r =
        AutoDOS::analyze(s.zipPath, s.primaryDb, s.localDb);
    if (!r.success)
        return -2;
    std::string outDir = fs::path(s.zipPath).stem().string();
    std::error_code ec;
    fs::create_directories(outDir, ec);
    if (!AutoDOS::extractZip(s.zipPath, outDir))
        return -3;
    std::string confOut;
    if (!AutoDOS::writeDosboxConf(s.zipPath, outDir, r, confOut, "", s.profile))
        return -4;
    s.lastConf = confOut.empty() ? s.zipPath.substr(0, s.zipPath.rfind('.')) + ".conf" : confOut;
    if (r.source == "scored") {
        AutoDOS::AnalyzeResult ar = r;
        if (ar.title.empty()) ar.title = fs::path(s.zipPath).stem().string();
        std::string target = s.localDb.empty() ? s.primaryDb : s.localDb;
        AutoDOS::addToDatabase(target, ar);
    }
    return 0;
}

static int runSync(const State& s) {
    fs::path sh = fs::path(s.repoRoot) / "scripts/sync-games-json.sh";
    if (!fs::is_regular_file(sh))
        return -1;
    std::string old = fs::current_path().string();
    std::error_code ec;
    fs::path dest = fs::absolute(s.primaryDb, ec);
    if (ec)
        dest = fs::path(s.primaryDb);
    if (chdir(s.repoRoot.c_str()) != 0)
        return -2;
    setenv("DEST", dest.string().c_str(), 1);
    int st = std::system("sh ./scripts/sync-games-json.sh --merge");
    chdir(old.c_str());
    return (st == 0) ? 0 : -3;
}

static int runExport(const State& s) {
    if (s.localDb.empty() || !fs::is_regular_file(s.localDb))
        return -4;
    fs::path py = fs::path(s.repoRoot) / "scripts/export_pr_games_fragment.py";
    if (!fs::is_regular_file(py))
        return -1;
    std::ostringstream cmd;
    cmd << "python3 \"" << py.string() << "\" --upstream-url "
           "https://raw.githubusercontent.com/makuka97/autoDos/main/src/games.json "
           "--local \""
        << s.localDb << "\" --out games.pr-fragment.json";
    return std::system(cmd.str().c_str()) == 0 ? 0 : -2;
}

int main(int argc, char** argv) {
    (void)argc;
    if (argv && argv[0] && std::getenv("AUTODOS_REPO_ROOT") == nullptr) {
        try {
            fs::path exe = fs::absolute(argv[0]);
            if (exe.has_filename()) {
                fs::path guess = exe.parent_path().parent_path();
                if (fs::is_regular_file(guess / "scripts/sync-games-json.sh"))
                    setenv("AUTODOS_REPO_ROOT", guess.string().c_str(), 1);
            }
        } catch (...) {
        }
    }
    State s;
    loadState(s);
    if (s.repoRoot.empty())
        s.repoRoot = detectRepoRoot();

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);

    std::vector<std::string> profiles;
    collectProfileNames(profiles);

    int sel     = 0;
    int maxSel  = 8;
    bool dirty  = false;
    std::string msg;

    for (;;) {
        if (!msg.empty()) {
            draw(s, sel, profiles);
            mvprintw(LINES - 3, 0, "%s", msg.c_str());
            msg.clear();
        } else
            draw(s, sel, profiles);

        int c = getch();
        if (c == 'q' || c == 'Q') {
            saveState(s);
            break;
        }
        if (c == KEY_UP || c == 'k') {
            sel = (sel - 1 + maxSel + 1) % (maxSel + 1);
            continue;
        }
        if (c == KEY_DOWN || c == 'j') {
            sel = (sel + 1) % (maxSel + 1);
            continue;
        }
        if (c != '\n' && c != '\r' && c != ' ')
            continue;

        switch (sel) {
        case 0:
            s.primaryDb = promptLine("Primary DB", s.primaryDb);
            dirty       = true;
            break;
        case 1:
            s.localDb = promptLine("Local overlay (empty line clears)", s.localDb, true);
            dirty     = true;
            break;
        case 2:
            cycleProfile(s, profiles);
            dirty = true;
            break;
        case 3:
            s.zipPath = promptLine("Zip path", s.zipPath);
            dirty     = true;
            break;
        case 4: {
            int e = runPrepare(s);
            if (e == 0) {
                msg = "Prepare OK. Conf: " + s.lastConf;
                dirty = true;
            } else if (e == -1)
                msg = "Prepare failed: zip missing or invalid path";
            else if (e == -2)
                msg = "Prepare failed: analyze";
            else if (e == -3)
                msg = "Prepare failed: extract";
            else
                msg = "Prepare failed: write conf";
            break;
        }
        case 5:
            if (s.lastConf.empty() || !fs::exists(s.lastConf))
                msg = "No last .conf";
            else {
                const char* dbx = std::getenv("AUTODOS_DOSBOX");
                std::string exe = dbx && dbx[0] ? dbx : "dosbox";
                if (AutoDOS::launchDosBox(exe, s.lastConf))
                    msg = "Launched DOSBox";
                else
                    msg = "Launch failed";
            }
            break;
        case 6: {
            int e = runSync(s);
            msg   = (e == 0) ? "Synced primary DB (--merge)" : "Sync failed (see terminal / paths)";
            break;
        }
        case 7: {
            int e = runExport(s);
            if (e == 0)
                msg = "Wrote games.pr-fragment.json (open PR on makuka97/autoDos)";
            else if (e == -4)
                msg = "Set a local overlay file with entries first";
            else
                msg = "Export failed";
            break;
        }
        case 8:
            saveState(s);
            endwin();
            return 0;
        }
        if (dirty) {
            saveState(s);
            dirty = false;
        }
    }

    endwin();
    return 0;
}
