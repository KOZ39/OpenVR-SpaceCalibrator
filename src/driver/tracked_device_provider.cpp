#include "tracked_device_provider.h"

#include "Eigen/Geometry"
#include "util.h"
#include "log.h"
#include "interface_hook_injector.h"
#include "vrmath.h"
#include "virtual_desktop.h"
#include <math.h>
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

    inline Eigen::Quaterniond eigenFromHmdQuat(const vr::HmdQuaternion_t& quat) {
        return Eigen::Quaterniond(quat.w, quat.x, quat.y, quat.z);
    }
    inline Eigen::Translation3d eigenTransFromHmdVec(const double pos[3]) {
        return Eigen::Translation3d(pos[0], pos[1], pos[2]);
    }
    inline Eigen::Vector3d eigenVecFromHmdVec(const double pos[3]) {
        return Eigen::Vector3d(pos[0], pos[1], pos[2]);
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
                if (IsDeviceIndexValid(transform.unRelativeTargetOpenvrDeviceId) && m_poses[transform.unRelativeTargetOpenvrDeviceId].poseIsValid &&
                    IsDeviceIndexValid(transform.unRelativeReferenceOpenvrDeviceId) && m_poses[transform.unRelativeReferenceOpenvrDeviceId].poseIsValid) {
                    const vr::DriverPose_t& targetPose = m_poses[transform.unRelativeTargetOpenvrDeviceId];
                    const vr::DriverPose_t& refPose = m_poses[transform.unRelativeReferenceOpenvrDeviceId];

                    // world-space pose of ref device
                    Eigen::Quaterniond refWorldFromDriverRot = eigenFromHmdQuat(refPose.qWorldFromDriverRotation);
                    Eigen::Vector3d    refWorldFromDriverPos = eigenVecFromHmdVec(refPose.vecWorldFromDriverTranslation);

                    Eigen::Quaterniond refRot = refWorldFromDriverRot * eigenFromHmdQuat(refPose.qRotation);
                    Eigen::Vector3d    refPos = refWorldFromDriverPos + refWorldFromDriverRot * eigenVecFromHmdVec(refPose.vecPosition);

                    Eigen::Affine3d refWorldNow = Eigen::Translation3d(refPos) * refRot;

                    // world-space pose of target device
                    Eigen::Quaterniond targetWorldFromDriverRot = eigenFromHmdQuat(targetPose.qWorldFromDriverRotation);
                    Eigen::Vector3d    targetWorldFromDriverPos = eigenVecFromHmdVec(targetPose.vecWorldFromDriverTranslation);

                    Eigen::Quaterniond targetRot = targetWorldFromDriverRot * eigenFromHmdQuat(targetPose.qRotation);
                    Eigen::Vector3d    targetPos = targetWorldFromDriverPos + targetWorldFromDriverRot * eigenVecFromHmdVec(targetPose.vecPosition);

                    Eigen::Affine3d targetWorldNow = Eigen::Translation3d(targetPos) * targetRot;

                    // world-space poses at calibration time
                    Eigen::Affine3d refWorldThen =
                        eigenTransFromHmdVec(transform.referenceTranslation.v) *
                        eigenFromHmdQuat(transform.referenceRotation);

                    // local-space calibration transform
                    Eigen::Affine3d calibrationLocal =
                        eigenTransFromHmdVec(transform.translation.v) *
                        eigenFromHmdQuat(transform.rotation);

                    // apply transformation matrices
                    Eigen::Affine3d worldCalib = refWorldNow * refWorldThen.inverse() * targetWorldNow * calibrationLocal;

                    // decompose to worldspace calibration pose data
                    Eigen::Quaterniond worldCalibRot(worldCalib.rotation());
                    worldCalibRot.normalize();
                    vr::HmdQuaternion_t worldCalibRotVr = { worldCalibRot.w(), worldCalibRot.x(), worldCalibRot.y(), worldCalibRot.z() };
                    
                    // apply like classic world-space calibration
                    modifiedPose.qWorldFromDriverRotation = worldCalibRotVr * modifiedPose.qWorldFromDriverRotation;

                    modifiedPose.vecPosition[0] *= transform.scale;
                    modifiedPose.vecPosition[1] *= transform.scale;
                    modifiedPose.vecPosition[2] *= transform.scale;

                    vr::HmdVector3d_t rotatedTranslation = quaternionRotateVector(worldCalibRotVr, modifiedPose.vecWorldFromDriverTranslation);
                    modifiedPose.vecWorldFromDriverTranslation[0] = rotatedTranslation.v[0] + worldCalib.translation().x();
                    modifiedPose.vecWorldFromDriverTranslation[1] = rotatedTranslation.v[1] + worldCalib.translation().y();
                    modifiedPose.vecWorldFromDriverTranslation[2] = rotatedTranslation.v[2] + worldCalib.translation().z();
                } else {
                    // @TODO: use last re-constructed world calib?
                }
            } else {
                // classic world-space application   
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
        }

        return true;
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
            Eigen::Quaterniond qRotation = eigenFromHmdQuat(pose.qRotation);
            Eigen::Vector3d vecAngularVel = eigenVecFromHmdVec(pose.vecAngularVelocity);
            Eigen::Vector3d localAngularVel = qRotation.inverse() * vecAngularVel;
            pose.vecAngularVelocity[0] = localAngularVel.x();
            pose.vecAngularVelocity[1] = localAngularVel.y();
            pose.vecAngularVelocity[2] = localAngularVel.z();
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

    void ServerTrackedDeviceProvider::SetDeviceTransform(ipc::protocol::Command_SetDeviceTransform_t& transform) {
        m_transforms[transform.unTargetOpenVrDeviceId].unTargetOpenVrDeviceId = transform.unTargetOpenVrDeviceId;
        m_transforms[transform.unTargetOpenVrDeviceId].enabled(transform.enabled());
        m_transforms[transform.unTargetOpenVrDeviceId].hideContinuousTracker(transform.hideContinuousTracker());
        m_transforms[transform.unTargetOpenVrDeviceId].lerpCalibrations(transform.lerpCalibrations());
        m_transforms[transform.unTargetOpenVrDeviceId].relativeCoordSystem(transform.relativeCoordSystem());
        m_transforms[transform.unTargetOpenVrDeviceId].quirks = transform.quirks;

        // @FIXME: i dont like how much this branches, we can probably make this branchless but thats a problem for future me
        if (transform.updateRotation()) {
            m_transforms[transform.unTargetOpenVrDeviceId].updateRotation(transform.updateRotation());
            m_transforms[transform.unTargetOpenVrDeviceId].rotation = transform.rotation;
        }
        if (transform.updateTranslation()) {
            m_transforms[transform.unTargetOpenVrDeviceId].updateTranslation(transform.updateTranslation());
            m_transforms[transform.unTargetOpenVrDeviceId].translation = transform.translation;
        }
        if (transform.updateScale()) {
            m_transforms[transform.unTargetOpenVrDeviceId].updateScale(transform.updateScale());
            m_transforms[transform.unTargetOpenVrDeviceId].scale = transform.scale;
        }
    }
}