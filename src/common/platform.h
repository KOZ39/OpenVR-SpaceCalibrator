#pragma once

#include <string>
#include <filesystem>

namespace platform {
    // %APPDATA% or ~/.config
    std::filesystem::path getUserConfigDir();

    std::string getEnvVariable(const std::string& szEnvVarName);

    bool isAnotherInstanceRunning(bool& bIsRunningViaSteam);
    void shutdownCurrentInstance();

    void showMessageDialog(const std::string& title, const std::string& message);

    void setThreadName(const std::string& threadName);
}