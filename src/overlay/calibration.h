#pragma once

#include "protocol.h"
#include "vr_core.h"
#include "ipc_client.h"
#include <Eigen/Dense>

namespace spacecal {

    constexpr double k_TICK_RATE_HZ = 20.0; // tick rate spacecal's internal logic runs at

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
        inline Pose_t() {}
        Pose_t(const vr::HmdMatrix34_t hmdMatrix);
        Pose_t(const Eigen::AffineCompact3d& transform);
        Pose_t(const vr::DriverPose_t& driverPose);
    };

    // An instantaneous sample. Several of these are collected and passed into the
    // Calibration algorithm to create a mapping from the reference device to the
    // target device.
    struct Sample_t {
        Pose_t reference; // what we are calibrating TO
        Pose_t target;    // the object that shall be calibrated
        double timestamp = 0; // @FIXME: seems to be unused with existing algorithm?
        bool isPoseValid = false; // whether or not the pose is usable for calibrating. this is filled based on multiple data points in vr::DriverPose_t for accuracy
    };

    struct CalibrationDevice {
        vr::TrackedDeviceIndex_t deviceId = vr::k_unTrackedDeviceIndexInvalid;
        ipc::protocol::DeviceQuirks_t quirks = ipc::protocol::DeviceQuirks_t::QUIRK_NONE;
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
        void calibrationTick(const double currentTime);
        void resetCalibrationForDevice(const CalibrationDevice& device); // resets the given device's pose to the raw pose
        // applies the calibration to the VR runtime
        void apply();

        // finds the device id given props, if and only if device id is invalid
        void assignTarget(CalibrationDevice& device);
        // marks a device as invalid; space calibrator will attempt reassigning an id next frame to it
        inline void unassignTarget(CalibrationDevice& device) {
            device.deviceId = vr::k_unTrackedDeviceIndexInvalid;
        }

        bool isActive = false; // enabled in the UI
        bool isValidCalibration = false; // whether we can even use this calibration
        bool hmdIsInSameTrackingSystem = false; // whether the hmd is part of the tracking systems involved in this calibration
        CalibrationSpeed calibrationSpeed = CalibrationSpeed::FAST;
        CalibrationState state = CalibrationState::NONE;

        CalibrationDevice referenceDevice; // what we are calibrating to (ie this will become the ABSOLUTE ORIGIN)
        CalibrationDevice targetDevice; // what we are calibrating (ie this tracking system will be manipulated to match the ref)

        // the calibrated pose
        Eigen::Quaterniond calibratedRotation;
        Eigen::Vector3d calibratedTranslation;
        double calibratedScale = 1.0;

        double wantedUpdateInterval = 1.0;

    private:
        Sample_t collectSample() const;
        inline size_t getSampleCount() const { return (size_t) calibrationSpeed; }

    private:
        double m_lastTick = 0;
        double m_lastScan = 0;
        float m_xPrev = 0;
        float m_yPrev = 0;
        float m_zPrev = 0;

        std::vector<Sample_t> m_samples;

        friend class CalibrationManager;
    };

    // Global manager for calibrations, once instance per VR session
    class CalibrationManager {
    public:
        explicit CalibrationManager();
        void init();
        void start(); // @FIXME: how do i even define a calibration????
        void shutdown();

        // handles updating pending calibrations every frame
        void calibrationTick(const double currentTime);

        // @TODO: Better getter? idk how a calibration should be defined in terms of ui
        TrackingSystemCalibration& getCalibration(const size_t index);
        size_t getCalibrationCount() const;

        const double getWantedUpdateInterval() const;

        [[nodiscard]] static inline CalibrationManager* getInstance() { return s_instance; }
        [[nodiscard]] inline ipc::IpcClient& getIpcClient() { return m_ipcClient; }

    private:

        double m_wantedUpdateInterval = 1.0;

        vr::DriverPose_t m_poses[vr::k_unMaxTrackedDeviceCount] = {};
        std::vector<TrackingSystemCalibration> m_calibrations;
        VRState m_vrState;

        ipc::IpcClient m_ipcClient;

        static CalibrationManager* s_instance;
        friend class ::ipc::IpcClient;
        friend class TrackingSystemCalibration;
    };
} // namespace spacecal