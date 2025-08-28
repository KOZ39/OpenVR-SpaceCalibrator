#include <stdio.h>
#include "tracked_device_provider.h"

#ifdef OPENVRSPACECALIBRATORDRIVER_EXPORTS
#define OPENVRSPACECALIBRATORDRIVER_API extern "C" __declspec(dllexport)
#else
#define OPENVRSPACECALIBRATORDRIVER_API extern "C" __declspec(dllimport)
#endif

spacecal::ServerTrackedDeviceProvider g_server;

OPENVRSPACECALIBRATORDRIVER_API void* HmdDriverFactory(const char* pInterfaceName, int* pReturnCode)
{
    if (std::strcmp(vr::IServerTrackedDeviceProvider_Version, pInterfaceName) == 0) {
        return &g_server;
    }

    if (pReturnCode) {
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    }
    return nullptr;
}