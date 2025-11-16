#include "tracked_device_provider.h"

#include "util.h"
#include "log.h"
#include "interface_hook_injector.h"
#include "vrmath.h"
#include <Eigen/Dense>
#include <openvr_driver.h>

namespace spacecal {
    vr::EVRInitError ServerTrackedDeviceProvider::Init(vr::IVRDriverContext* pDriverContext) {
        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

        util::init();
        logging::Init(/* isOverlay */ false);

        LOG_OPENVR_INFO("Starting SpaceCalibrator-Nova. Linked with OpenVR driver API {}.{}.{}", vr::k_nSteamVRVersionMajor, vr::k_nSteamVRVersionMinor, vr::k_nSteamVRVersionBuild);

        m_alignmentParams.thr_rot_tiny = 0.1f * (EIGEN_PI / 180.0f);
        m_alignmentParams.thr_rot_small = 1.0f * (EIGEN_PI / 180.0f);
        m_alignmentParams.thr_rot_large = 5.0f * (EIGEN_PI / 180.0f);

        m_alignmentParams.thr_trans_tiny = 0.1f / 1000.0; // mm
        m_alignmentParams.thr_trans_small = 1.0f / 1000.0; // mm
        m_alignmentParams.thr_trans_large = 20.0f / 1000.0; // mm

        m_alignmentParams.align_speed_tiny = 0.05f;
        m_alignmentParams.align_speed_small = 0.2f;
        m_alignmentParams.align_speed_large = 2.0f;

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

    inline vr::HmdVector3d_t quaternionRotateVector(const vr::HmdQuaternion_t& quat, const double(&vector)[3]) {
        vr::HmdQuaternion_t vectorQuat = { 0.0, vector[0], vector[1] , vector[2] };
        auto rotatedVectorQuat = quat * vectorQuat * -quat;
        return { rotatedVectorQuat.x, rotatedVectorQuat.y, rotatedVectorQuat.z };
    }

    bool ServerTrackedDeviceProvider::HandleDevicePoseUpdated(vr::TrackedDeviceIndex_t unWhichDevice, const vr::DriverPose_t& newPose) {

        // bounds check, as sometimes the id is invalid?
        if (!IsDeviceIndexValid(unWhichDevice))
            return true;

        m_ipcServer.UpdatePose(unWhichDevice, newPose);

        vr::DriverPose_t modifiedPose = newPose;

        auto& transform = m_transforms[unWhichDevice];
        
        if (transform.hideContinuousTracker()) {
            modifiedPose.vecPosition[0] = -modifiedPose.vecWorldFromDriverTranslation[0];
            modifiedPose.vecPosition[1] = -modifiedPose.vecWorldFromDriverTranslation[1] + 9001; // put it 9001m above the origin
            modifiedPose.vecPosition[2] = -modifiedPose.vecWorldFromDriverTranslation[2];
        }
        else if (transform.enabled()) {
            HandleQuirks(transform.quirks, modifiedPose);

            modifiedPose.qWorldFromDriverRotation = transform.rotation * modifiedPose.qWorldFromDriverRotation;

            modifiedPose.vecPosition[0] *= transform.scale;
            modifiedPose.vecPosition[1] *= transform.scale;
            modifiedPose.vecPosition[2] *= transform.scale;

            vr::HmdVector3d_t rotatedTranslation = quaternionRotateVector(transform.rotation, modifiedPose.vecWorldFromDriverTranslation);
            modifiedPose.vecWorldFromDriverTranslation[0] = rotatedTranslation.v[0] + transform.translation.v[0];
            modifiedPose.vecWorldFromDriverTranslation[1] = rotatedTranslation.v[1] + transform.translation.v[1];
            modifiedPose.vecWorldFromDriverTranslation[2] = rotatedTranslation.v[2] + transform.translation.v[2];

            // @TODO: Lerping for continuous calibration?
        }

        return true;
    }

    void ServerTrackedDeviceProvider::HandleQuirks(ipc::protocol::DeviceQuirks_t quirks, vr::DriverPose_t& pose) {

        if (ENUM_HAS_FLAGS(quirks, ipc::protocol::DeviceQuirks_t::QUIRK_SCALE)) {
            // m_poses->vecAngularVelocity
        }

        if (ENUM_HAS_FLAGS(quirks, ipc::protocol::DeviceQuirks_t::QUIRK_RECOMPUTE_VELOCITY)) {
            // m_poses->vecAngularVelocity
        }

        if (ENUM_HAS_FLAGS(quirks, ipc::protocol::DeviceQuirks_t::QUIRK_RECOMPUTE_ANGULAR_VELOCITY)) {
            // m_poses->vecAngularVelocity
        }

        // this improves pose prediction on some vendors' devices (eg Pico)
        if (ENUM_HAS_FLAGS(quirks, ipc::protocol::DeviceQuirks_t::QUIRK_HAS_WORLDSPACE_ANGULAR_VELOCITY)) {
            // m_poses->vecAngularVelocity
            // pose.vecAngularVelocity = Vector3.Transform(pose.vecAngularVelocity, Quaternion.Inverse(pose.qRotation));
        }
    }

    void ServerTrackedDeviceProvider::ResetCalibration() {

    }
    void ServerTrackedDeviceProvider::SetAlignmentSpeedParams(ipc::protocol::Command_SetAlignmentSpeedParams_t& params) {
        m_alignmentParams = params;
    }
    void ServerTrackedDeviceProvider::SetDeviceTransform(ipc::protocol::Command_SetDeviceTransform_t& transform) {
        m_transforms[transform.unOpenvrDeviceId] = transform;
    }
}