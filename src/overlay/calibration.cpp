#include "calibration.h"
#include "log.h"
#include "platform.h"

namespace spacecal {

    CalibrationManager* CalibrationManager::s_instance = nullptr;

    // hmdmatrix -> pose
    Pose_t::Pose_t(const vr::HmdMatrix34_t hmdMatrix) {
        // [r] [r] [r] [t]
        // [r] [r] [r] [t]
        // [r] [r] [r] [t]
        //    where r is a 3x3 rot matrix, t is a 3 element vector
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                rot(i, j) = hmdMatrix.m[i][j];
            }
        }
        trans = Eigen::Vector3d(hmdMatrix.m[0][3], hmdMatrix.m[1][3], hmdMatrix.m[2][3]);
    }

    void TrackingSystemCalibration::init() {
        // @TODO: 

    }

    void TrackingSystemCalibration::start() {
        // @TODO: 

    }
    
    void TrackingSystemCalibration::calibrationTick(const double currentTime) {
        if (!vr::VRSystem())
            return;

        if ((currentTime - m_lastTick) < (1.0 / k_TICK_RATE_HZ))
            return;

        m_lastTick = currentTime;

        // original code checks hmd specifically, we should check that the hmd and ref arent at 0 0 0
        // check that ref is not at origin
        auto p = CalibrationManager::getInstance()->m_poses[vr::k_unTrackedDeviceIndex_Hmd].vecPosition;
        if (hmdIsInSameTrackingSystem) {
            
        }
        m_xPrev = (float)p[0];
        m_yPrev = (float)p[1];
        m_zPrev = (float)p[2];


    }
    
    void TrackingSystemCalibration::reset() {
        // @TODO: 

    }
    
    // applies calibration to all devices under this tracking system
    void TrackingSystemCalibration::apply() {
        this->isActive = isValidCalibration;

        for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            auto device = VRState::getInstance()->getVrDevice(i);
            if (device.eDeviceClass != vr::TrackedDeviceClass_Invalid) {

                if (i == vr::k_unTrackedDeviceIndex_Hmd) {
                    if (device.szTrackingSystemId != referenceDevice.trackingSystem) {
                        // if the hmd the hmd's tracking system is not what was saved, handles users changing streamer / VR headset properly by not applying calibration
                        isActive = false;
                    }
                    continue;
                }

                // if this is not the target tracking system
                if (device.szTrackingSystemId != targetDevice.trackingSystem) {
                    continue;
                }

                ipc::protocol::Command_SetDeviceTransform_t args = {};

                args.unOpenvrDeviceId = i;
                args.enabled(device.bIsConnected);

                if (device.bIsConnected) {
                    args.quirks = ipc::protocol::DeviceQuirks_t::QUIRK_NONE;

                    args.updateTranslation(true);
                    args.translation.v[0] = calibratedTranslation.x();
                    args.translation.v[1] = calibratedTranslation.y();
                    args.translation.v[2] = calibratedTranslation.z();

                    args.updateRotation(true);
                    args.rotation.x = calibratedRotation.x();
                    args.rotation.y = calibratedRotation.y();
                    args.rotation.z = calibratedRotation.z();
                    args.rotation.w = calibratedRotation.w();

                    args.updateScale(true);
                    args.scale = calibratedScale;
                }

                CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);
            }
        }
    }

    CalibrationManager::CalibrationManager() {
        // @FIXME: Need to figure out how to handle multiple calibrations
        TrackingSystemCalibration mainCalibration;
        mainCalibration.hmdIsInSameTrackingSystem = true;
        m_calibrations.push_back(mainCalibration);
    }

    void CalibrationManager::init() {
        // @TODO: 


        for (auto& calibration : m_calibrations) {
            calibration.init();
        }
    }

    void CalibrationManager::start() {
        // @TODO: 


        for (auto& calibration : m_calibrations) {
            calibration.start();
        }
    }

    void CalibrationManager::calibrationTick(const double currentTime) {
        // @TODO: 
        m_ipcClient.PollPoses();

        double wantedInterval = 0;
        size_t countedCalibrations = 0;

        for (auto& calibration : m_calibrations) {
            calibration.calibrationTick(currentTime);
            if (calibration.isActive) {
                wantedInterval += calibration.wantedUpdateInterval;
                countedCalibrations++;
            }
        }

        if (countedCalibrations > 0) {
            m_wantedUpdateInterval = wantedInterval / countedCalibrations;
        }
    }

    TrackingSystemCalibration& CalibrationManager::getCalibration(const size_t index) {
        return m_calibrations[index];
    }

    size_t CalibrationManager::getCalibrationCount() const {
        return m_calibrations.size();
    }

    const double CalibrationManager::getWantedUpdateInterval() const {
        return m_wantedUpdateInterval;
    }
}