#include "platform.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <combaseapi.h>

namespace platform {
    std::filesystem::path getUserConfigDir() {

        PWSTR path_pwstr = NULL;

        HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path_pwstr);

        if (SUCCEEDED(hr)) {
            return std::filesystem::path(path_pwstr);
            CoTaskMemFree(path_pwstr);
        }

        return std::filesystem::path();
    }
}
#endif