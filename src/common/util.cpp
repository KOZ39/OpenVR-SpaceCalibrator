#include "util.h"
#include "platform.h"
#if _DEBUG
#include <cassert>
#endif
#if OS_WINDOWS
#include <windows.h>
#elif OS_LINUX
#include <limits.h>
#include <unistd.h>

#else
#error "Unsupported platform"
#endif

namespace util {

namespace fs = ::std::filesystem;

constexpr const char* k_szSpaceCalibratorDirName = "space-calibrator";

fs::path k_spaceCalibratorInstallDirectory {};
fs::path k_spaceCalibratorLangsDirectory {};
fs::path k_spaceCalibratorImagesDir {};
fs::path k_spaceCalibratorConfigDirectory {};
fs::path k_spaceCalibratorLogsDirectory {};
fs::path k_spaceCalibratorDumpsDirectory {};

fs::path k_spaceCalibratorConfigFile {};

void init()
{
    k_spaceCalibratorInstallDirectory = platform::getExeDir();
    k_spaceCalibratorConfigDirectory = platform::getUserConfigDir() / k_szSpaceCalibratorDirName;
    k_spaceCalibratorConfigFile = k_spaceCalibratorConfigDirectory / "config.json";
    k_spaceCalibratorLogsDirectory = k_spaceCalibratorConfigDirectory / "logs";
    k_spaceCalibratorDumpsDirectory = k_spaceCalibratorConfigDirectory / "dumps";

    k_spaceCalibratorLangsDirectory = k_spaceCalibratorInstallDirectory / "assets" / "lang";
    k_spaceCalibratorImagesDir = k_spaceCalibratorInstallDirectory / "assets" / "images";

    // create dirs if they dont exist
    if (!std::filesystem::is_directory(k_spaceCalibratorConfigDirectory)) {
        std::filesystem::create_directories(k_spaceCalibratorConfigDirectory);
    }
    if (!std::filesystem::is_directory(k_spaceCalibratorLogsDirectory)) {
        std::filesystem::create_directories(k_spaceCalibratorLogsDirectory);
    }
    if (!std::filesystem::is_directory(k_spaceCalibratorDumpsDirectory)) {
        std::filesystem::create_directories(k_spaceCalibratorDumpsDirectory);
    }
}

const fs::path& getSpaceCalibratorInstallDir()
{
#if _DEBUG
    assert(!k_spaceCalibratorInstallDirectory.empty());
#endif
    return k_spaceCalibratorInstallDirectory;
}

const fs::path& getSpaceCalibratorLangsDir()
{
#if _DEBUG
    assert(!k_spaceCalibratorLangsDirectory.empty());
#endif
    return k_spaceCalibratorLangsDirectory;
}

const fs::path& getSpaceCalibratorImagesDir()
{
#if _DEBUG
    assert(!k_spaceCalibratorImagesDir.empty());
#endif
    return k_spaceCalibratorImagesDir;
}

const fs::path& getSpaceCalibratorConfigDir()
{
#if _DEBUG
    assert(!k_spaceCalibratorConfigDirectory.empty());
#endif
    return k_spaceCalibratorConfigDirectory;
}

const fs::path& getSpaceCalibratorConfigPath()
{
#if _DEBUG
    assert(!k_spaceCalibratorConfigFile.empty());
#endif
    return k_spaceCalibratorConfigFile;
}

const fs::path& getSpaceCalibratorLogsDir()
{
#if _DEBUG
    assert(!k_spaceCalibratorLogsDirectory.empty());
#endif
    return k_spaceCalibratorLogsDirectory;
}

const fs::path& getSpaceCalibratorDumpsDir()
{
#if _DEBUG
    assert(!k_spaceCalibratorDumpsDirectory.empty());
#endif
    return k_spaceCalibratorDumpsDirectory;
}
}