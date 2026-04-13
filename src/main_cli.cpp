// autodos-cli — headless AutoDOS engine (Linux / macOS / Windows console).

#include "autodos.h"
#include "autodos_launch.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void printUsage() {
    std::cerr
        << "autodos-cli — AutoDOS engine (no GUI)\n\n"
        << "Usage:\n"
        << "  autodos-cli analyze <zip> [--db PATH]\n"
        << "      Print a short analysis summary (exit 0 if an executable was chosen).\n\n"
        << "  autodos-cli extract <zip> <outdir>\n"
        << "      Extract zip to outdir.\n\n"
        << "  autodos-cli prepare <zip> [--db PATH] [--dir OUTDIR] [--conf PATH]\n"
        << "      Analyze, extract, write dosbox.conf (default: OUTDIR = ./<zip_stem>/,\n"
        << "      conf next to zip unless --conf is set).\n\n"
        << "  autodos-cli launch [--dosbox PATH] <conf>\n"
        << "      Start DOSBox with -conf (default PATH: dosbox, or AUTODOS_DOSBOX env).\n\n"
        << "Environment:\n"
        << "  AUTODOS_DB       Default games.json path for --db when omitted\n";
}

static std::string defaultDbPath() {
    const char* e = std::getenv("AUTODOS_DB");
    if (e && e[0])
        return e;
    return "games.json";
}

static int cmdAnalyze(int argc, char** argv) {
    std::string db = defaultDbPath();
    std::string zip;

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db = argv[++i];
            continue;
        }
    }
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--db") == 0) {
            ++i;
            continue;
        }
        if (argv[i][0] == '-')
            continue;
        zip = argv[i];
        break;
    }

    if (zip.empty()) {
        std::cerr << "analyze: missing <zip>\n";
        return 2;
    }
    if (!fs::exists(zip)) {
        std::cerr << "analyze: zip not found: " << zip << "\n";
        return 2;
    }

    AutoDOS::AnalyzeResult r = AutoDOS::analyze(zip, db);
    if (!r.success) {
        std::cerr << "analyze: failed: " << r.error << "\n";
        return 1;
    }

    std::cout << "success: yes\n"
              << "title: " << r.title << "\n"
              << "exe: " << r.exe << "\n"
              << "work_dir: " << r.workDir << "\n"
              << "source: " << r.source << "\n"
              << "confidence: " << r.confidence << "\n"
              << "game_type: " << r.gameType << "\n"
              << "cycles: " << r.cycles << "\n"
              << "memsize: " << r.memsize << "\n"
              << "ems: " << (r.ems ? "true" : "false") << "\n"
              << "xms: " << (r.xms ? "true" : "false") << "\n"
              << "cd_mount: " << (r.cdMount ? "true" : "false") << "\n";
    return 0;
}

static int cmdExtract(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "extract: need <zip> <outdir>\n";
        return 2;
    }
    std::string zip  = argv[0];
    std::string outd = argv[1];
    if (!fs::exists(zip)) {
        std::cerr << "extract: zip not found: " << zip << "\n";
        return 2;
    }
    std::error_code ec;
    fs::create_directories(outd, ec);
    if (!AutoDOS::extractZip(zip, outd)) {
        std::cerr << "extract: failed\n";
        return 1;
    }
    std::cout << "extract: ok -> " << outd << "\n";
    return 0;
}

static int cmdPrepare(int argc, char** argv) {
    std::string db;
    std::string zip;
    std::string outDir;
    std::string confOut;

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            outDir = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--conf") == 0 && i + 1 < argc) {
            confOut = argv[++i];
            continue;
        }
        if (argv[i][0] == '-')
            continue;
        if (zip.empty())
            zip = argv[i];
    }
    if (db.empty())
        db = defaultDbPath();
    if (zip.empty()) {
        std::cerr << "prepare: missing <zip>\n";
        return 2;
    }
    if (!fs::exists(zip)) {
        std::cerr << "prepare: zip not found: " << zip << "\n";
        return 2;
    }

    if (outDir.empty())
        outDir = fs::path(zip).stem().string();

    AutoDOS::AnalyzeResult r = AutoDOS::analyze(zip, db);
    if (!r.success) {
        std::cerr << "prepare: analyze failed: " << r.error << "\n";
        return 1;
    }

    std::error_code ec;
    fs::create_directories(outDir, ec);
    if (!AutoDOS::extractZip(zip, outDir)) {
        std::cerr << "prepare: extract failed\n";
        return 1;
    }

    const std::string confArg = confOut.empty() ? std::string() : confOut;
    if (!AutoDOS::writeDosboxConf(zip, outDir, r, confArg)) {
        std::cerr << "prepare: write dosbox.conf failed\n";
        return 1;
    }

    std::string written = confOut.empty()
        ? zip.substr(0, zip.rfind('.')) + ".conf"
        : confOut;
    std::cout << "prepare: ok\n"
              << "  extracted: " << outDir << "\n"
              << "  conf: " << written << "\n";
    return 0;
}

static int cmdLaunch(int argc, char** argv) {
    std::string dosbox = "dosbox";
    const char* envExe = std::getenv("AUTODOS_DOSBOX");
    if (envExe && envExe[0])
        dosbox = envExe;

    std::vector<char*> args;
    for (int i = 0; i < argc; ++i)
        args.push_back(argv[i]);

    for (size_t i = 0; i < args.size(); ++i) {
        if (std::strcmp(args[i], "--dosbox") == 0 && i + 1 < args.size()) {
            dosbox = args[i + 1];
            args.erase(args.begin() + static_cast<long>(i),
                       args.begin() + static_cast<long>(i) + 2);
            break;
        }
    }

    if (args.size() != 1) {
        std::cerr << "launch: need [--dosbox PATH] <conf>\n";
        return 2;
    }
    std::string conf = args[0];
    if (!fs::exists(conf)) {
        std::cerr << "launch: conf not found: " << conf << "\n";
        return 2;
    }

    if (!AutoDOS::launchDosBox(dosbox, conf)) {
        std::cerr << "launch: failed to start DOSBox: " << dosbox << "\n";
        return 1;
    }
    std::cout << "launch: started " << dosbox << " -conf " << conf << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const char* cmd = argv[1];
    int subArgc     = argc - 2;
    char** subArgv  = (subArgc > 0) ? argv + 2 : argv + argc;

    if (std::strcmp(cmd, "analyze") == 0)
        return cmdAnalyze(subArgc, subArgv);
    if (std::strcmp(cmd, "extract") == 0)
        return cmdExtract(subArgc, subArgv);
    if (std::strcmp(cmd, "prepare") == 0)
        return cmdPrepare(subArgc, subArgv);
    if (std::strcmp(cmd, "launch") == 0)
        return cmdLaunch(subArgc, subArgv);

    printUsage();
    return 2;
}
