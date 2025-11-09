#pragma once

#include "vr_core.h"
#include <Eigen/Dense>

namespace spacecal {

    // An instantaneous pose, SteamVR poses are mapped to this for ease of use with
    // Eigen
    struct Pose_t {
        Eigen::Matrix3d rot;
        Eigen::Vector3d trans;
    };

    // An instantaneous sample. Several of these are collected and passed into the
    // Calibration algorithm to create a mapping from the reference device to the
    // target device.
    struct Sample_t {
        Pose_t reference;
        Pose_t target;
        double timestamp;
    };

    // An instance of the calibration algoritm, that handles collecting samples, and
    // deducing a mapping between calibrations of two tracking systems. It is
    // separated from the Calibration Manager to allow one to generate multiple
    // calibrations for multiple combinations of playspaces.
    class TrackingSystemCalibration {
    public:
        void init();
    };

    class CalibrationManager {
    public:
        void init();

    private:
        VRState m_vrState;
    };
} // namespace spacecal