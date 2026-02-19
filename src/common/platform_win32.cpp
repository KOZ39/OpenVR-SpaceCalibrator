#include "platform.h"

#if OS_WINDOWS
#include "constants.h"

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
            auto thePath = std::filesystem::path(path_pwstr);
            CoTaskMemFree(path_pwstr);
            return thePath;
        }

        return std::filesystem::path();
    }

    std::string getEnvVariable(const std::string& szEnvVarName) {
        char szStringBuffer[256] = {};
        DWORD result = GetEnvironmentVariableA(szEnvVarName.c_str(), szStringBuffer, sizeof(szStringBuffer));
        if (result > 0 && szStringBuffer[0] != 0) {
            return szStringBuffer;
        }
        return "";
    }

    constexpr const char* s_STEAM_MUTEX_KEY = "Global\\MUTEX__SpaceCalibrator_Steam";
    HANDLE s_hSteamMutex = INVALID_HANDLE_VALUE;
    bool s_isGitHubVersionInstalled = false;

    bool isAnotherInstanceRunning(bool& bIsRunningViaSteam) {

        bIsRunningViaSteam = false;
        std::string szSteamAppIdEnv = getEnvVariable("SteamAppId");
        if (spacecal::c_SPACE_CALIBRATOR_STEAM_APP_ID == szSteamAppIdEnv ||
            spacecal::c_STEAMVR_STEAM_APP_ID == szSteamAppIdEnv) {
            // We got launched via the Steam client UI.
            s_hSteamMutex = CreateMutexA(NULL, FALSE, s_STEAM_MUTEX_KEY);
            bIsRunningViaSteam = true;
            if (s_hSteamMutex == nullptr) {
                s_hSteamMutex = INVALID_HANDLE_VALUE;
            }
            else {
                // mutex opened, check if we opened one, if so exit
                if (GetLastError() == ERROR_ALREADY_EXISTS) {
                    CloseHandle(s_hSteamMutex);
                    s_hSteamMutex = INVALID_HANDLE_VALUE;
                    return true;
                }
            }
            return false;
        }

        return false;
    }
    void shutdownCurrentInstance() {
        if (s_hSteamMutex != INVALID_HANDLE_VALUE && s_hSteamMutex != nullptr) {
            CloseHandle(s_hSteamMutex);
            s_hSteamMutex = nullptr;
        }
    }

    void showMessageDialog(const std::string& title, const std::string& message) {
        MessageBoxA(nullptr, message.c_str(), title.c_str(), 0);
    }

    void setThreadName(const std::string& threadName) {
        std::wstring szWinThreadName(threadName.size(), L' ');
        szWinThreadName.resize(std::mbstowcs(&szWinThreadName[0], threadName.c_str(), threadName.size()));
        SetThreadDescription(GetCurrentThread(), szWinThreadName.c_str());
    }
}
#endif