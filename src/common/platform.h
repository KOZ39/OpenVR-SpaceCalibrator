#pragma once

#include <string>
#include <filesystem>

#ifdef _WIN32
#define OS_WINDOWS 1
#define OS_LINUX 0
#elif defined(__linux__)
#define OS_WINDOWS 0
#define OS_LINUX 1
#else
#error "Unsupported OS"
#endif

namespace platform {
    // %APPDATA% or ~/.config
    std::filesystem::path getUserConfigDir();

    std::string getEnvVariable(const std::string& szEnvVarName);

    bool isAnotherInstanceRunning(bool& bIsRunningViaSteam);
    void shutdownCurrentInstance();

    void showMessageDialog(const std::string& title, const std::string& message);

    void setThreadName(const std::string& threadName);
}