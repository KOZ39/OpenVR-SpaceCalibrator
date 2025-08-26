#pragma once

#include <openvr_driver.h>

namespace spacecal {
    class ServerTrackedDeviceProvider;
}

namespace hooking {
    void InjectHooks(spacecal::ServerTrackedDeviceProvider* driver, vr::IVRDriverContext* pDriverContext);
    void DisableHooks();
}