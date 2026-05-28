#include <stdio.h>
#include <cstring>
#include "tracked_device_provider.h"
#include "platform.h"

#if OS_WINDOWS
#define SPACECALIBRATORDRIVER_EXPORT extern "C" __declspec( dllexport )
#define SPACECALIBRATORDRIVER_IMPORT extern "C" __declspec( dllimport )
#elif COMPILER_GCC || OS_LINUX
#define SPACECALIBRATORDRIVER_EXPORT extern "C" __attribute__( ( visibility( "default" ) ) )
#define SPACECALIBRATORDRIVER_IMPORT extern "C"
#else
#error "Unsupported Platform."
#endif

spacecal::ServerTrackedDeviceProvider g_server;

SPACECALIBRATORDRIVER_EXPORT void* HmdDriverFactory(const char* pInterfaceName, int* pReturnCode)
{
    if (std::strcmp(vr::IServerTrackedDeviceProvider_Version, pInterfaceName) == 0) {
        return &g_server;
    }

    if (pReturnCode) {
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    }
    return nullptr;
}