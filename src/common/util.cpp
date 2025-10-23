#include "util.h"
#include "platform.h"
#if _DEBUG
#include <cassert>
#endif
#if _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace util {

    namespace fs = ::std::filesystem;

    constexpr const char* k_szSpaceCalibratorDirName = "space-calibrator";

    fs::path k_spaceCalibratorInstallDirectory{};
    fs::path k_spaceCalibratorLangsDirectory{};
    fs::path k_spaceCalibratorConfigDirectory{};
    fs::path k_spaceCalibratorLogsDirectory{};

    void init() {
        {
#if _WIN32
            wchar_t path_buf[MAX_PATH];
            DWORD size = GetModuleFileNameW(NULL, path_buf, MAX_PATH);
            if (size > 0 && size <= MAX_PATH) {
                k_spaceCalibratorInstallDirectory = fs::path(path_buf).parent_path();
            }
#else
            try {
                k_spaceCalibratorInstallDirectory = fs::canonical("/proc/self/exe");
            }
            catch (const fs::filesystem_error& e) {}
#endif
        }

        k_spaceCalibratorConfigDirectory = platform::getUserConfigDir() / k_szSpaceCalibratorDirName;
        k_spaceCalibratorLogsDirectory = k_spaceCalibratorConfigDirectory / "logs";

        k_spaceCalibratorLangsDirectory = k_spaceCalibratorInstallDirectory / "langs";

        // create dirs if they dont exist
        if (!std::filesystem::is_directory(k_spaceCalibratorConfigDirectory)) {
            std::filesystem::create_directories(k_spaceCalibratorConfigDirectory);
        }
        if (!std::filesystem::is_directory(k_spaceCalibratorLogsDirectory)) {
            std::filesystem::create_directories(k_spaceCalibratorLogsDirectory);
        }
    }

    const fs::path& getSpaceCalibratorInstallDir() {
#if _DEBUG
        assert(!k_spaceCalibratorInstallDirectory.empty());
#endif
        return k_spaceCalibratorInstallDirectory;
    }

    const fs::path& getSpaceCalibratorLangsDir() {
#if _DEBUG
        assert(!k_spaceCalibratorLangsDirectory.empty());
#endif
        return k_spaceCalibratorLangsDirectory;
    }

    const fs::path& getSpaceCalibratorConfigDir() {
#if _DEBUG
        assert(!k_spaceCalibratorConfigDirectory.empty());
#endif
        return k_spaceCalibratorConfigDirectory;
    }

    const fs::path& getSpaceCalibratorLogsDir() {
#if _DEBUG
        assert(!k_spaceCalibratorLogsDirectory.empty());
#endif
        return k_spaceCalibratorLogsDirectory;
    }
}