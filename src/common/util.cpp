#include "util.h"
#include "platform.h"
#if _DEBUG
#include <cassert>
#endif

namespace util {

    namespace fs = ::std::filesystem;

    constexpr const char* k_szSpaceCalibratorDirName = "space-calibrator";

    fs::path k_spaceCalibratorDirectory{};
    fs::path k_spaceCalibratorLogsDirectory{};
    
    void init() {
        k_spaceCalibratorDirectory = platform::getUserConfigDir() / k_szSpaceCalibratorDirName;
        k_spaceCalibratorLogsDirectory = k_spaceCalibratorDirectory / "logs";

        // create dirs if they dont exist
        if (!std::filesystem::is_directory(k_spaceCalibratorDirectory)) {
            std::filesystem::create_directories(k_spaceCalibratorDirectory);
        }
        if (!std::filesystem::is_directory(k_spaceCalibratorLogsDirectory)) {
            std::filesystem::create_directories(k_spaceCalibratorLogsDirectory);
        }
    }

    const fs::path& getSpaceCalibratorDir() {
#if _DEBUG
        assert(!k_spaceCalibratorDirectory.empty());
#endif
        return k_spaceCalibratorDirectory;
    }
    const fs::path& getSpaceCalibratorLogsDir() {
#if _DEBUG
        assert(!k_spaceCalibratorLogsDirectory.empty());
#endif
        return k_spaceCalibratorLogsDirectory;
    }
}