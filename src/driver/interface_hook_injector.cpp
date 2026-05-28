#include "hooking.h"
#include "interface_hook_injector.h"
#include "tracked_device_provider.h"

namespace hooking {

    static spacecal::ServerTrackedDeviceProvider* g_mainDriver = nullptr;

    static Hook<void* (*)(vr::IVRDriverContext*, const char*, vr::EVRInitError*)>
        GetGenericInterfaceHook("IVRDriverContext::GetGenericInterface");
    static Hook<void(*)(vr::IVRServerDriverHost*, uint32_t, const vr::DriverPose_t&, uint32_t)>
        TrackedDevicePoseUpdatedHook005("IVRServerDriverHost005::TrackedDevicePoseUpdated");
    static Hook<void(*)(vr::IVRServerDriverHost*, uint32_t, const vr::DriverPose_t&, uint32_t)>
        TrackedDevicePoseUpdatedHook006("IVRServerDriverHost006::TrackedDevicePoseUpdated");

    static void DetourTrackedDevicePoseUpdated005(vr::IVRServerDriverHost* _this, uint32_t unWhichDevice, const vr::DriverPose_t* newPose, uint32_t unPoseStructSize) {
        // LOG_HOOKING_DEBUG("ServerTrackedDeviceProvider::DetourTrackedDevicePoseUpdated_005({})", unWhichDevice);
        const vr::DriverPose_t* pNewPose = newPose; // somehow newPose is nullptr sometimes??????
        if (pNewPose && unPoseStructSize == sizeof(vr::DriverPose_t)) {
            vr::DriverPose_t pose = *newPose;
            if (g_mainDriver->HandleDevicePoseUpdated(unWhichDevice, pose)) {
                TrackedDevicePoseUpdatedHook005.originalFunc(_this, unWhichDevice, pose, unPoseStructSize);
            }
        } else {
            if (newPose)
                TrackedDevicePoseUpdatedHook005.originalFunc(_this, unWhichDevice, *newPose, unPoseStructSize);
        }
    }

    static void DetourTrackedDevicePoseUpdated006(vr::IVRServerDriverHost* _this, uint32_t unWhichDevice, const vr::DriverPose_t* newPose, uint32_t unPoseStructSize) {
        // LOG_HOOKING_DEBUG("ServerTrackedDeviceProvider::DetourTrackedDevicePoseUpdated_006({})", unWhichDevice);
        const vr::DriverPose_t* pNewPose = newPose; // somehow newPose is nullptr sometimes??????
        if (pNewPose && unPoseStructSize == sizeof(vr::DriverPose_t)) {
            vr::DriverPose_t pose = *newPose;
            if (g_mainDriver->HandleDevicePoseUpdated(unWhichDevice, pose)) {
                TrackedDevicePoseUpdatedHook006.originalFunc(_this, unWhichDevice, pose, unPoseStructSize);
            }
        } else {
            if (newPose)
                TrackedDevicePoseUpdatedHook006.originalFunc(_this, unWhichDevice, *newPose, unPoseStructSize);
        }
    }

    static void* DetourGetGenericInterface(vr::IVRDriverContext* _this, const char* pchInterfaceVersion, vr::EVRInitError* peError) {
        LOG_HOOKING_DEBUG("ServerTrackedDeviceProvider::DetourGetGenericInterface({})", pchInterfaceVersion);
        auto originalInterface = GetGenericInterfaceHook.originalFunc(_this, pchInterfaceVersion, peError);

        if (std::strcmp("IVRServerDriverHost_005", pchInterfaceVersion) == 0) {
            if (!IHook::Exists(TrackedDevicePoseUpdatedHook005.name)) {
                TrackedDevicePoseUpdatedHook005.CreateHookInObjectVTable(originalInterface, 1, reinterpret_cast<void*>(&DetourTrackedDevicePoseUpdated005));
                IHook::Register(&TrackedDevicePoseUpdatedHook005);
            }
        }
        else if (std::strcmp("IVRServerDriverHost_006", pchInterfaceVersion) == 0) {
            if (!IHook::Exists(TrackedDevicePoseUpdatedHook006.name)) {
                TrackedDevicePoseUpdatedHook006.CreateHookInObjectVTable(originalInterface, 1, reinterpret_cast<void*>(&DetourTrackedDevicePoseUpdated006));
                IHook::Register(&TrackedDevicePoseUpdatedHook006);
            }
        }

        return originalInterface;
    }

    void InjectHooks(spacecal::ServerTrackedDeviceProvider* driver, vr::IVRDriverContext* pDriverContext) {
        g_mainDriver = driver;

#if OS_WINDOWS
        MH_STATUS err = MH_Initialize();
        if (err == MH_OK) {
            GetGenericInterfaceHook.CreateHookInObjectVTable(pDriverContext, 0, reinterpret_cast<void*>(&DetourGetGenericInterface));
            IHook::Register(&GetGenericInterfaceHook);
            LOG_HOOKING_INFO("Space Calibrator hooked into OpenVR PoseUpdate successfully");
        } else if (err == MH_ERROR_ALREADY_INITIALIZED) {
            LOG_HOOKING_ERROR("MH_Initialize error: {}; how did this happen??? (probably SteamVR crash loop lol)", MH_StatusToString(err));
        } else {
            LOG_HOOKING_ERROR("MH_Initialize error: {}", MH_StatusToString(err));
        }
#elif OS_LINUX
        if (GetGenericInterfaceHook.CreateHookInObjectVTable(pDriverContext, 0, reinterpret_cast<void*>(&DetourGetGenericInterface))) {
            IHook::Register(&GetGenericInterfaceHook);
            LOG_HOOKING_INFO("Space Calibrator hooked into OpenVR PoseUpdate successfully");
        } else {
            LOG_HOOKING_ERROR("Failed to hook into OpenVR PoseUpdate.");
        }
#endif
    }

    void DisableHooks() {
        IHook::DestroyAll();
#if OS_WINDOWS
        MH_Uninitialize();
#endif
        LOG_HOOKING_INFO("Removed all hooks successfully");
    }
}