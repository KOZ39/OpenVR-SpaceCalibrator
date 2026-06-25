#include "tracked_device_provider.h"

#include "util.h"
#include "log.h"
#include "interface_hook_injector.h"
#include "vrmath.h"
#include "virtual_desktop.h"
#include <math.h>
#include <Eigen/Geometry>
#include <Eigen/Dense>
#include <openvr_driver.h>

namespace spacecal {
    vr::EVRInitError ServerTrackedDeviceProvider::Init(vr::IVRDriverContext* pDriverContext) {
        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

        util::init();
        logging::Init(/* isOverlay */ false);
        hmd::initVD();

        LOG_OPENVR_INFO("Starting SpaceCalibrator-Nova. Compiled against OpenVR driver API {}.{}.{}", vr::k_nSteamVRVersionMajor, vr::k_nSteamVRVersionMinor, vr::k_nSteamVRVersionBuild);

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

    inline vr::HmdVector3d_t quaternionRotateVector(const vr::HmdQuaternion_t& quat, const double(&vector)[3]) {
        vr::HmdQuaternion_t vectorQuat = { 0.0, vector[0], vector[1] , vector[2] };
        auto rotatedVectorQuat = quat * vectorQuat * -quat;
        return { rotatedVectorQuat.x, rotatedVectorQuat.y, rotatedVectorQuat.z };
    }
    inline void copyVec3(double dest[3], const vr::HmdVector3d_t& src) {
        ASSERT(dest != nullptr, "dest was not a valid memory address!");
        dest[0] = src.v[0];
        dest[1] = src.v[1];
        dest[2] = src.v[2];
    }

    inline vr::HmdQuaternion_t hmdQuatFromEigen(const Eigen::Quaterniond& quat) {
        return vr::HmdQuaternion_t{ quat.w(), quat.x(), quat.y(), quat.z() };
    }
    inline vr::HmdVector3d_t hmdVecFromEigenVec(const Eigen::Vector3d& pos) {
        return vr::HmdVector3d_t{ .v = { pos.x(), pos.y(), pos.z() } };
    }
    inline vr::HmdVector3d_t hmdVecFromEigenVec(const Eigen::Translation3d& pos) {
        return vr::HmdVector3d_t{ .v = { pos.x(), pos.y(), pos.z() } };
    }

    inline Eigen::Quaterniond eigenFromHmdQuat(const vr::HmdQuaternion_t& quat) {
        return Eigen::Quaterniond(quat.w, quat.x, quat.y, quat.z);
    }
    inline Eigen::Translation3d eigenTransFromHmdVec(const double pos[3]) {
        return Eigen::Translation3d(pos[0], pos[1], pos[2]);
    }
    inline Eigen::Vector3d eigenVecFromHmdVec(const double pos[3]) {
        return Eigen::Vector3d(pos[0], pos[1], pos[2]);
    }

    // lerps between two calibrations by factor %
    // @NOTE: factor is unclamped, input should be 0-1
    inline DeviceCalibration_t lerpCalibration(const DeviceCalibration_t& from, const DeviceCalibration_t& to, double factor) {
        return {
            .calibrationRotation = from.calibrationRotation.slerp(factor, to.calibrationRotation),
            .calibrationPosition = from.calibrationPosition + factor * (to.calibrationPosition - from.calibrationPosition)
        };
    }

    inline Eigen::Affine3d getWorldFromDriverPose(const vr::DriverPose_t& pose, float fPredictedSecondsToPhotonsFromNow = 0.0f) {
        Eigen::Quaterniond baseRot = eigenFromHmdQuat(pose.qRotation);
        Eigen::Vector3d    basePos = eigenVecFromHmdVec(pose.vecPosition);

        Eigen::Quaterniond worldFromDriverRot = eigenFromHmdQuat(pose.qWorldFromDriverRotation);
        Eigen::Vector3d    worldFromDriverPos = eigenVecFromHmdVec(pose.vecWorldFromDriverTranslation);

        Eigen::Quaterniond rot = worldFromDriverRot * baseRot;
        Eigen::Vector3d    pos = worldFromDriverPos + worldFromDriverRot * basePos;

        return Eigen::Translation3d(pos) * rot;
    }

    bool ServerTrackedDeviceProvider::HandleDevicePoseUpdated(vr::TrackedDeviceIndex_t unWhichDevice, vr::DriverPose_t& newPose) {

        // bounds check, as sometimes the id is invalid?
        if (!IsDeviceIndexValid(unWhichDevice))
            return true;

        m_ipcServer.UpdatePose(unWhichDevice, newPose);

        vr::DriverPose_t& modifiedPose = newPose;

        auto& transform = m_transforms[unWhichDevice];
        
        if (transform.hideContinuousTracker()) {
            modifiedPose.vecPosition[0] = -modifiedPose.vecWorldFromDriverTranslation[0];
            modifiedPose.vecPosition[1] = -modifiedPose.vecWorldFromDriverTranslation[1] + 9001; // put it 9001m above the origin
            modifiedPose.vecPosition[2] = -modifiedPose.vecWorldFromDriverTranslation[2];
        } else if (transform.enabled()) {
            HandleQuirks(transform.quirks, modifiedPose);

            if (transform.relativeCoordSystem()) {
                if (IsDeviceIndexValid(transform.unRelativeReferenceOpenvrDeviceId) && IsPoseValid(m_poses[transform.unRelativeReferenceOpenvrDeviceId]) &&
                    IsDeviceIndexValid(transform.unRelativeTargetOpenvrDeviceId) && IsPoseValid(m_poses[transform.unRelativeTargetOpenvrDeviceId])) {
                    const vr::DriverPose_t& refPose = m_poses[transform.unRelativeReferenceOpenvrDeviceId];
                    const vr::DriverPose_t& targetPose = m_poses[transform.unRelativeTargetOpenvrDeviceId];

                    // world-space pose of ref device (H')
                    Eigen::Affine3d refWorldNow = getWorldFromDriverPose(refPose);
                    // world-space pose of target device (T')
                    Eigen::Affine3d targetWorldNow = getWorldFromDriverPose(targetPose);

                    // local-space calibration transform (C_L)
                    Eigen::Affine3d calibrationLocal =
                        eigenTransFromHmdVec(transform.translation.v) *
                        eigenFromHmdQuat(transform.rotation);

                    // To compute the calibration for now, we need to compute where we expect the tracker to be (by applying the local calibration),
                    // and then computing a mapping from the tracker's raw pose to the expected pose to get a calibration thats valid for now.

                    // T_H = H' * C_L
                    Eigen::Affine3d expectedTargetWorld = refWorldNow * calibrationLocal;

                    // C' = (T')^-1 * T_H
                    Eigen::Affine3d worldCalib = expectedTargetWorld * targetWorldNow.inverse();

                    // decompose to worldspace calibration pose data
                    Eigen::Quaterniond worldCalibRot(worldCalib.rotation());
                    worldCalibRot.normalize();

                    // keep track of calibrations so that we can keep using them even when either the ref or target device lose tracking
                    // we write directly to blend target for free smoothing
                    DeviceCalibration_t newCalib = { worldCalibRot, worldCalib.translation() };
                    if (!m_cachedCalibrations[unWhichDevice].hasCalibration) {
                        m_cachedCalibrations[unWhichDevice] = { worldCalibRot, worldCalib.translation(), true };
                    } else {
                        m_cachedCalibrations[unWhichDevice] = lerpCalibration(m_cachedCalibrations[unWhichDevice], newCalib, 0.05); // @TODO: adjust fudge factor
                    }
                } 

                applyCalibrationToPose(
                    modifiedPose,
                    hmdQuatFromEigen(m_cachedCalibrations[unWhichDevice].calibrationRotation),
                    hmdVecFromEigenVec(m_cachedCalibrations[unWhichDevice].calibrationPosition),
                    transform.scale,
                    transform.calibrateMotionVecs()
                );
            } else {
                // classic world-space application
                applyCalibrationToPose(modifiedPose, transform.rotation, transform.translation, transform.scale, transform.calibrateMotionVecs());
            }
        }

        return true;
    }

    void ServerTrackedDeviceProvider::applyCalibrationToPose(vr::DriverPose_t& pose, vr::HmdQuaternion_t rotation, vr::HmdVector3d_t pos, double scale, bool calibrateMotionVecs) {
        // apply like classic world-space calibration
        pose.qWorldFromDriverRotation = rotation * pose.qWorldFromDriverRotation;

        pose.vecPosition[0] *= scale;
        pose.vecPosition[1] *= scale;
        pose.vecPosition[2] *= scale;

        vr::HmdVector3d_t rotatedTranslation = quaternionRotateVector(rotation, pose.vecWorldFromDriverTranslation);
        pose.vecWorldFromDriverTranslation[0] = rotatedTranslation.v[0] + pos.v[0];
        pose.vecWorldFromDriverTranslation[1] = rotatedTranslation.v[1] + pos.v[1];
        pose.vecWorldFromDriverTranslation[2] = rotatedTranslation.v[2] + pos.v[2];

        if (calibrateMotionVecs) {
            // velocity
            // SRT order
            pose.vecVelocity[0] *= scale;
            pose.vecVelocity[1] *= scale;
            pose.vecVelocity[2] *= scale;
            vr::HmdVector3d_t rotatedVelocityRel = quaternionRotateVector(rotation, pose.vecVelocity);
            copyVec3(pose.vecVelocity, rotatedVelocityRel);

            // acceleration
            // SRT order
            pose.vecAcceleration[0] *= scale;
            pose.vecAcceleration[1] *= scale;
            pose.vecAcceleration[2] *= scale;
            vr::HmdVector3d_t rotatedAccelerationRel = quaternionRotateVector(rotation, pose.vecAcceleration);
            copyVec3(pose.vecAcceleration, rotatedAccelerationRel);

            // angular velocity
            vr::HmdVector3d_t rotatedAngularVelRel = quaternionRotateVector(rotation, pose.vecAngularVelocity);
            copyVec3(pose.vecAngularVelocity, rotatedAngularVelRel);

            // angular accel
            vr::HmdVector3d_t rotatedAngularAccel = quaternionRotateVector(rotation, pose.vecAngularAcceleration);
            copyVec3(pose.vecAngularAcceleration, rotatedAngularAccel);
        }
    }

    void ServerTrackedDeviceProvider::HandleQuirks(ipc::protocol::DeviceQuirks_t quirks, vr::DriverPose_t& pose) {
        // @TOOD: implement quirks handling

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
            // convert quat from world space to local space
            vr::HmdQuaternion_t qInverse = -pose.qRotation; // invert quaternion, i should probably make this an explicit function bc i dont like operators being used like this
            vr::HmdVector3d_t localAngularVel = quaternionRotateVector(qInverse, pose.vecAngularVelocity);
            copyVec3(pose.vecAngularVelocity, localAngularVel);
        }
    }

    void ServerTrackedDeviceProvider::ResetCalibration() {
        // @TODO: idk what would be more correct than "random fucking offsets"
    }
    
    void ServerTrackedDeviceProvider::RequestVirtualDesktopProps() {
        // this simply states whether or not the vd driver is loaded. it doesnt tell us if the active hmd IS vd
        m_hmdMetaData.isVirtualDesktopAvailable = vr::VRDriverManager()->GetDriverHandle("virtualdesktop") != vr::k_ulInvalidDriverHandle;
        // NOTE: DO NOT INVOKE IF VD IS NOT THE ACTIVE HMD! WILL CAUSE CRASH!!
        if (m_hmdMetaData.isVirtualDesktopAvailable) {
            m_hmdMetaData.VD_hmdModel = hmd::VD_getHmdModel();
            m_hmdMetaData.VD_stageTrackingEnabled = hmd::VD_isStageTrackingEnabled();
        }
    }

    void ServerTrackedDeviceProvider::SetAlignmentSpeedParams(ipc::protocol::Command_SetAlignmentSpeedParams_t& params) {
        m_alignmentParams = params;
    }
}