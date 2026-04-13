#pragma once
// Spawn DOSBox detached (fork + setsid + execlp). See docs/UPSTREAM_LEGACY.md for removed Win32 code.

#include <string>

namespace AutoDOS {

bool launchDosBox(const std::string& dosboxPath, const std::string& confPath);

} // namespace AutoDOS
