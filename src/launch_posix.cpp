// POSIX: fork + exec; child detaches (setsid) like a GUI launcher would.

#include "autodos_launch.h"

#include <cstdlib>
#include <unistd.h>

#include <string>

namespace AutoDOS {

bool launchDosBox(const std::string& dosboxPath, const std::string& confPath) {
    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        setsid();
        execlp(dosboxPath.c_str(), dosboxPath.c_str(), "-conf", confPath.c_str(),
               static_cast<char*>(nullptr));
        _exit(127);
    }
    return true;
}

} // namespace AutoDOS
