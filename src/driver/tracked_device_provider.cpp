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
        hooking::DisableHooks();
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

    void ServerTrackedDeviceProvider::BlendTransform(const DeviceTransformation_t device) const {
        std::chrono::high_resolution_clock::time_point timestamp = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = timestamp - device.last_poll;
        double lerp = elapsed_seconds.count();

        // lerp *= GetTransformRate(device.currentRate);
        if (lerp > 1.0)
            lerp = 1.0;
        if (lerp < 0 || isnan(lerp))
            lerp = 0;

        // device.transform = device.transform.interpolateAround(lerp, device.targetTransform, deviceWorldPose.translation);
    }

    bool ServerTrackedDeviceProvider::HandleDevicePoseUpdated(vr::TrackedDeviceIndex_t unWhichDevice, const vr::DriverPose_t& newPose) {

        // bounds check, as sometimes the id is invalid?
        if (!IsDeviceIndexValid(unWhichDevice))
            return true;

        m_ipcServer.UpdatePose(unWhichDevice, newPose);
        
        // @TODO: Update pose with calibration

        return true;
    }

    void ServerTrackedDeviceProvider::ResetCalibration() {

    }
    void ServerTrackedDeviceProvider::SetAlignmentSpeedParams(ipc::protocol::Command_SetAlignmentSpeedParams_t& params) {

    }
    void ServerTrackedDeviceProvider::SetDeviceTransform(ipc::protocol::Command_SetDeviceTransform_t& transform) {

    }
}