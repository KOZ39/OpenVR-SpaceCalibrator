#pragma once

#include <string>
#include <filesystem>

namespace util {
    void init();
    const std::filesystem::path& getSpaceCalibratorDir();
    const std::filesystem::path& getSpaceCalibratorLogsDir();
}