#pragma once

#include "vr_core.h"

namespace spacecal {

    // An instantaneous pose, SteamVR poses are mapped to this for ease of use with
    // Eigen
    struct Pose_t {};

    // An instantaneous sample. Several of these are collected and passed into the
    // Calibration algorithm to create a mapping from the reference device to the
    // target device.
    struct Sample_t {};

    // An instance of the calibration algoritm, that handles collecting samples, and
    // deducing a mapping between calibrations of two tracking systems. It is
    // separated from the Calibration Manager to allow one to generate multiple
    // calibrations for multiple combinations of playspaces.
    class TrackingSystemCalibration {
    public:
    };

    class CalibrationManager {
    public:
        void init();

    private:
        VRState m_vrState;
    };
} // namespace spacecal