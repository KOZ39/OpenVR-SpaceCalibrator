#include "virtual_desktop.h"
#include "platform.h"
#include "log.h"

#if OS_WINDOWS
#include <openvr_driver.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>

// Virtual Desktop functionality is only on Windows, on Linux these are no-ops as the functionality is stubbed.

// provided by driver_oculus
typedef int (*ovr_GetInt_t)(void* session, const char* propertyName, int defaultVal);
typedef bool (*ovr_GetBool_t)(void* session, const char* propertyName, bool defaultVal);
ovr_GetInt_t ovr_GetInt = nullptr;
ovr_GetBool_t ovr_GetBool = nullptr;
HMODULE g_hModuleDriverOculus = 0;
#endif // OS_WINDOWS

namespace hmd {
#if OS_WINDOWS

    std::string getVirtualDesktopInstallPath() {
        // Check registry for legacy config
        // HKEY_LOCAL_MACHINE\SOFTWARE\Virtual Desktop, Inc.\Virtual Desktop Streamer
        constexpr const char* RegistryKey = "SOFTWARE\\Virtual Desktop, Inc.\\Virtual Desktop Streamer";

        DWORD size = 0;
        auto result = RegGetValueA(HKEY_LOCAL_MACHINE, RegistryKey, "Path", RRF_RT_REG_SZ, 0, 0, &size);
        if (result != ERROR_SUCCESS)
        {
            char* message;
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, result, LANG_USER_DEFAULT, (LPSTR)&message, 0, NULL);
            LOG_ERROR("{}", message);
            return "";
        }

        std::string str;
        str.resize(size);

        result = RegGetValueA(HKEY_LOCAL_MACHINE, RegistryKey, "Path", RRF_RT_REG_SZ, 0, &str[0], &size);
        if (result != ERROR_SUCCESS)
        {
            char* message;
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, result, LANG_USER_DEFAULT, (LPSTR)&message, 0, NULL);
            LOG_ERROR("{}", result);
            return "";
        }

        str.resize(size - 1);
        return str;
    }

    bool initVD() {
        if (g_hModuleDriverOculus && ovr_GetInt && ovr_GetBool) {
            return true; // already initialised
        }
        g_hModuleDriverOculus = GetModuleHandleW(L"VirtualDesktop.LibOVRRT64_1.dll"); // should be loaded already

        // if not loaded try loading the dll manually!
        if (!g_hModuleDriverOculus) {
            std::wstring szVdRegPath = (std::filesystem::path(getVirtualDesktopInstallPath()) / "VirtualDesktop.LibOVRRT64_1.dll").wstring();
            g_hModuleDriverOculus = LoadLibraryW(szVdRegPath.c_str());
        }
        
        // fuck it fallback to hardcoded path
        if (!g_hModuleDriverOculus) {
            g_hModuleDriverOculus = LoadLibraryW(L"C:\\Program Files\\Virtual Desktop Streamer\\VirtualDesktop.LibOVRRT64_1.dll");
        }
        if (!g_hModuleDriverOculus) {
            g_hModuleDriverOculus = LoadLibraryW(L"C:\\Program Files (x86)\\Virtual Desktop Streamer\\VirtualDesktop.LibOVRRT64_1.dll");
        }

        if (g_hModuleDriverOculus) {
            ovr_GetInt = (ovr_GetInt_t)GetProcAddress(g_hModuleDriverOculus, "ovr_GetInt");
            ovr_GetBool = (ovr_GetBool_t)GetProcAddress(g_hModuleDriverOculus, "ovr_GetBool");

            if (ovr_GetInt && ovr_GetBool) {
                return true;
            } else {
                LOG_HOOKING_WARN("Couldn't hook into VirtualDesktop.LibOVRRT64_1.dll; cannot communicate with VirtualDesktop if it's being used.");
            }
        } else {
            LOG_HOOKING_WARN("Couldn't hook into VirtualDesktop.LibOVRRT64_1.dll; cannot communicate with VirtualDesktop if it's being used.");
        }
        LOG_HOOKING_INFO("Successfully hooked into VirtualDesktop.LibOVRRT64_1.dll! Virtual Desktop specific behaviour is available for this session.");
        return false;
    }
    
    ipc::protocol::VD_HmdModel VD_getHmdModel() {
        if (ovr_GetInt) {
            return (ipc::protocol::VD_HmdModel)ovr_GetInt(nullptr, "HmdModel", 0);
        }
        return ipc::protocol::VD_HmdModel::VD_HmdModel_None;
    }

    bool VD_isStageTrackingEnabled() {
        if (ovr_GetBool) {
            return ovr_GetBool(nullptr, "StageTracking", false);
        }
        return false;
    }

#else // OS_WINDOWS

    bool initVD() {
        LOG_HOOKING_WARN("VirtualDesktop is unavailable, as this is a Linux build.");
        return false;
    }

    ipc::protocol::VD_HmdModel VD_getHmdModel() {
        return ipc::protocol::VD_HmdModel::VD_HmdModel_None;
    }

    bool VD_isStageTrackingEnabled() {
        return false;
    }
#endif // OS_LINUX
}