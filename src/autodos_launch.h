#pragma once
// Platform-specific process spawning (Win32 / POSIX). UI and CLI link this.

#include <string>

namespace AutoDOS {

bool launchDosBox(const std::string& dosboxPath, const std::string& confPath);

} // namespace AutoDOS
