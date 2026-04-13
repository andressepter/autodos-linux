// Windows: spawn DOSBox detached (same behavior as legacy autodos.cpp).

#include "autodos_launch.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>

namespace AutoDOS {

bool launchDosBox(const std::string& dosboxPath, const std::string& confPath) {
    std::string cmd = "\"" + dosboxPath + "\" -conf \"" + confPath + "\"";
    STARTUPINFOA        si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr, FALSE,
                             DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok != FALSE;
}

} // namespace AutoDOS
