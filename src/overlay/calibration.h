#pragma once

#include "protocol.h"
#include "vr_core.h"
#include <Eigen/Dense>

// forward decl
namespace ipc {
    class IpcClient;
}

namespace spacecal {

    enum class CalibrationState {
        // calibration is inactive
        NONE,
        // calibration is starting
        START,
        // collecting samples
        SAMPLE,
        // calibrating orientation
        ROTATION,
        // calibrating position
        TRANSLATION,
        // calibrating scale
        SCALE,
        // user is editing calibration
        EDITING,
        // collecting samples for continuous
        CONTINUOUS_SAMPLE,
        // continuous calibration, but idle
        CONTINUOUS_IDLE,
    };

    // numeric value corresponds to the sample count for the given speed, the enum is just "naming" and avoids expensive lookup :3
    enum class CalibrationSpeed {
        FAST = 100,
        SLOW = 250,
        VERY_SLOW = 500,
    };

    // An instantaneous pose, SteamVR poses are mapped to this for ease of use with
    // Eigen
    struct Pose_t {
        Eigen::Matrix3d rot;
        Eigen::Vector3d trans;
        Pose_t(const vr::HmdMatrix34_t hmdMatrix);
    };

    // An instantaneous sample. Several of these are collected and passed into the
    // Calibration algorithm to create a mapping from the reference device to the
    // target device.
    struct Sample_t {
        Pose_t reference; // what we are calibrating TO
        Pose_t target;    // the object that shall be calibrated
        double timestamp; // @FIXME: seems to be unused with existing algorithm?
    };

    struct CalibrationDevice {
        vr::TrackedDeviceIndex_t deviceId = vr::k_unTrackedDeviceIndexInvalid;
        std::string trackingSystem;
        std::string deviceModel;
        std::string deviceSerialNumber;
    };

    class CalibrationManager;

    // An instance of the calibration algorithm, that handles collecting samples, and
    // deducing a mapping between calibrations of two tracking systems. It is
    // separated from the Calibration Manager to allow one to generate multiple
    // calibrations for multiple combinations of playspaces.
    class TrackingSystemCalibration {
    public:
        void init();
        void start();
        void calibrationTick(const double deltaTime);
        void reset();
        // applies the calibration to the VR runtime
        void apply();

        bool isActive = false; // enabled in the UI
        bool isValidCalibration = false; // whether we can even use this calibration
        bool hmdIsInSameTrackingSystem = false; // whether the hmd is part of the tracking systems involved in this calibration
        CalibrationSpeed calibrationSpeed = CalibrationSpeed::FAST;
        CalibrationState state = CalibrationState::NONE;

        CalibrationDevice referenceDevice; // what we are calibrating to (ie this will become the ABSOLUTE ORIGIN)
        CalibrationDevice targetDevice; // what we are calibrating (ie this tracking system will be manipulated to match the ref)

        // the calibrated pose
        Eigen::Vector3d calibratedRotation;
        Eigen::Vector3d calibratedTranslation;
        double calibratedScale;

    private:
        double m_lastTick;
        std::vector<Sample_t> m_samples;

        friend class CalibrationManager;
    };

    // Global manager for calibrations, once instance per VR session
    class CalibrationManager {
    public:
        explicit CalibrationManager();
        void init();
        void start(); // how do i even define a calibration????

        // handles updating pending calibrations every frame
        void calibrationTick(const double deltaTime);

        // @TODO: Better getter? idk how a calibration should be defined in terms of ui
        TrackingSystemCalibration& getCalibration(const size_t index);
        size_t getCalibrationCount() const;

        [[nodiscard]] static inline CalibrationManager* getInstance() { return s_instance; }

    private:

        vr::DriverPose_t m_poses[vr::k_unMaxTrackedDeviceCount] = {};
        std::vector<TrackingSystemCalibration> m_calibrations;
        VRState m_vrState;

        static CalibrationManager* s_instance;
        friend class ::ipc::IpcClient;
    };
} // namespace spacecal