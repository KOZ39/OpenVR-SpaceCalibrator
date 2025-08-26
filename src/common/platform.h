#pragma once

#include <string>
#include <filesystem>

namespace platform {
    // %APPDATA% or ~/.config
    std::filesystem::path getUserConfigDir();
}