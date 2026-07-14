#pragma once

#include <string>
#include <filesystem>

namespace util {
    void init();
    const std::filesystem::path& getSpaceCalibratorInstallDir();
    const std::filesystem::path& getSpaceCalibratorLangsDir();
    const std::filesystem::path& getSpaceCalibratorConfigDir();
    const std::filesystem::path& getSpaceCalibratorLogsDir();

    const std::filesystem::path& getSpaceCalibratorConfigPath();
}

#ifndef _DEBUG
#define ASSERT(cond, msg)
#else
#include <assert.h>
#define ASSERT(cond, msg) assert(cond)
#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))