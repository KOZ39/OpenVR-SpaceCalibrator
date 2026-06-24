#pragma once

#include <openvr_driver.h>
#include "ipc_server.h"
#include <chrono>

namespace spacecal {

    struct DeviceTransformation_t {
        bool hideDevice = false;
        std::chrono::high_resolution_clock::time_point last_poll;
    };

    struct DeviceCalibration_t {
        vr::HmdQuaternion_t calibrationRotation;
        vr::HmdVector3d_t calibrationPosition;
    };

    class ServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider {
    public:
        vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
        void Cleanup() override;
        const char* const* GetInterfaceVersions() override;
        void RunFrame() override;
        bool ShouldBlockStandbyMode() override;
        void EnterStandby() override;
        void LeaveStandby() override;

        void BlendTransform(const DeviceTransformation_t device) const;
        bool HandleDevicePoseUpdated(vr::TrackedDeviceIndex_t unWhichDevice, vr::DriverPose_t& newPose);
        void ResetCalibration();
        void RequestVirtualDesktopProps();
        void SetAlignmentSpeedParams(ipc::protocol::Command_SetAlignmentSpeedParams_t& params);
        void HandleQuirks(ipc::protocol::DeviceQuirks_t quirks, vr::DriverPose_t& pose);

        [[nodiscard]] inline bool IsDeviceIndexValid(const vr::TrackedDeviceIndex_t index) const {
            return index < vr::k_unMaxTrackedDeviceCount && index != vr::k_unTrackedDeviceIndexInvalid && index != vr::k_unTrackedDeviceIndexOther;
        }

        [[nodiscard]] inline bool IsPoseValid(const vr::DriverPose_t pose) const {
            return pose.deviceIsConnected && pose.poseIsValid && pose.result == vr::ETrackingResult::TrackingResult_Running_OK;
        }

        [[nodiscard]] inline bool IsPoseValid(const vr::TrackedDevicePose_t pose) const {
            return pose.bDeviceIsConnected && pose.bPoseIsValid && pose.eTrackingResult == vr::ETrackingResult::TrackingResult_Running_OK;
        }
    private:
        void applyCalibrationToPose(vr::DriverPose_t& pose, vr::HmdQuaternion_t rotation, vr::HmdVector3d_t pos, double scale, bool calibrateMotionVecs);

    private:
        ipc::Server m_ipcServer;
        ipc::protocol::Command_SetDeviceTransform_t m_transforms[vr::k_unMaxTrackedDeviceCount] = {};
        ipc::protocol::Command_SetAlignmentSpeedParams_t m_alignmentParams = {};

        ipc::protocol::SharedData_HmdMetadata m_hmdMetaData = {};
        
        DeviceCalibration_t m_cachedCalibrations[vr::k_unMaxTrackedDeviceCount] = {}; // cache of calibrations for relative calibration
        vr::DriverPose_t m_poses[vr::k_unMaxTrackedDeviceCount] = {}; // raw poses
        friend class ipc::Server;
    };
}