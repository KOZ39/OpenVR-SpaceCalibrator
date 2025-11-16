#include <stdio.h>
#include "tracked_device_provider.h"

#if defined( _WIN32 )
#define SPACECALIBRATORDRIVER_EXPORT extern "C" __declspec( dllexport )
#define SPACECALIBRATORDRIVER_IMPORT extern "C" __declspec( dllimport )
#elif defined( __GNUC__ ) || defined( COMPILER_GCC ) || defined( __APPLE__ )
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