#include "calibration.h"
#include "log.h"
#include "platform.h"

namespace spacecal {

    CalibrationManager* CalibrationManager::s_instance = nullptr;

    // pose -> hmdmatrix
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
    
    void TrackingSystemCalibration::calibrationTick(const double deltaTime) {
        if (!vr::VRSystem())
            return;

        if ((deltaTime - m_lastTick) < 0.05)
            return;

        m_lastTick = deltaTime;

        // original code checks hmd specifically, we should check that the hmd and ref arent at 0 0 0
        // check that ref is not at origin
        auto p = m_ipcServer.devicePoses[vr::k_unTrackedDeviceIndex_Hmd].vecPosition;
        if (hmdIsInSameTrackingSystem) {
            
        }
        m_xPrev = (float)p[0];
        m_yPrev = (float)p[1];
        m_zPrev = (float)p[2];

    }
    
    void TrackingSystemCalibration::reset() {
        // @TODO: 

    }
    
    void TrackingSystemCalibration::apply() {
        // @TODO: 

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

    void CalibrationManager::calibrationTick(const double deltaTime) {
        // @TODO: 


        for (auto& calibration : m_calibrations) {
            calibration.calibrationTick(deltaTime);
        }
    }

    TrackingSystemCalibration& CalibrationManager::getCalibration(const size_t index) {
        return m_calibrations[index];
    }

    size_t CalibrationManager::getCalibrationCount() const {
        return m_calibrations.size();
    }
}