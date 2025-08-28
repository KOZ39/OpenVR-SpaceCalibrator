#include "tracked_device_provider.h"

#include "util.h"
#include "log.h"
#include "interface_hook_injector.h"
#include <openvr_driver.h>

namespace spacecal {
    vr::EVRInitError ServerTrackedDeviceProvider::Init(vr::IVRDriverContext* pDriverContext) {

        util::init();
        logging::Init(/* isOverlay */ false);

        LOG_OPENVR_INFO("Starting SpaceCalibrator-Nova. Linked with OpenVR driver API {}.{}.{}", vr::k_nSteamVRVersionMajor, vr::k_nSteamVRVersionMinor, vr::k_nSteamVRVersionBuild);

        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
        m_ipcServer.Connect(this);
        hooking::InjectHooks(this, pDriverContext);

        return vr::VRInitError_None;
    }
    void ServerTrackedDeviceProvider::Cleanup() {
        m_ipcServer.Shutdown();
        VR_CLEANUP_SERVER_DRIVER_CONTEXT();
    }

    const char* const* ServerTrackedDeviceProvider::GetInterfaceVersions() {
        return vr::k_InterfaceVersions;
    }
    void ServerTrackedDeviceProvider::RunFrame() {}
    bool ServerTrackedDeviceProvider::ShouldBlockStandbyMode() {
        return false;
    }

    // @TODO: should spacecal inform the overlay to ignore calibrations during standby?
    void ServerTrackedDeviceProvider::EnterStandby() {}
    void ServerTrackedDeviceProvider::LeaveStandby() {}

    bool ServerTrackedDeviceProvider::HandleDevicePoseUpdated(uint32_t unWhichDevice, const vr::DriverPose_t& newPose) {
        return true;
    }
}