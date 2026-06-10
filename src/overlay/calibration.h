#pragma once

#include "protocol.h"
#include "vr_core.h"
#include "ipc_client.h"
#include <Eigen/Dense>

namespace spacecal {

    constexpr double k_TICK_RATE_HZ = 20.0; // tick rate spacecal's internal logic runs at
    constexpr double k_MAX_RETARGETING_RMS_ERROR_THRESHOLD = 0.1;
    constexpr double k_MAX_AXIS_VARIANCE_THRESHOLD = 0.001;
    constexpr double k_ROTATION_ANGLE_THRESHOLD = 0.4;
    constexpr double k_ROTATION_MAGNITUDE_THRESHOLD = 0.1;
    constexpr double k_MAX_INVALID_CALIBRATION_TIME_SEC = 60.0; // 60s between invalid calibrations
    constexpr size_t k_MIN_DELTA_SAMPLE_COUNT = 200;

    enum class CalibrationState {
        // calibration is inactive
        NONE,
        // calibration is starting
        START,
        // collecting samples
        SAMPLE,
        // user is editing calibration
        EDITING,
        // continuous calibration mode
        CONTINUOUS,
        // the calibration was set to continuous in a previous session, but the user hasn't selected it. we won't compute for now
        CONTINUOUS_IDLE,
    };

    // numeric value corresponds to the sample count for the given speed, the enum is just "naming" and avoids expensive lookup :3
    enum class CalibrationSpeed {
        FAST = 100,
        SLOW = 250,
        VERY_SLOW = 500,
    };

    enum class CalibrationError {
        None,
        LackOfRotationalVariance, // move around more
        RmsErrorTooHigh, // the RMS error was too poor to be worth using
        WorseRmsThanLast, // the RMS error was worse than the last calibration attempt
        AxisVarianceTooHigh, // the axis variance was unacceptably high
        WorseAxisVarianceThanLast, // the axis variance was worse than the last calibration attempt
        BadRelativeCalibration, // maths fucked up, try again soz
        Unknown,
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
        void startContinuous();
        void reset();
        inline void clearSamples() { m_samples.clear(); }
        void calibrationTick(const double currentTime);
        void resetCalibrationForDevice(const CalibrationDevice& device); // resets the given device's pose to the raw pose
        void apply(); // applies the calibration to the VR runtime

        // finds the device id given props, if and only if device id is invalid
        void assignTarget(CalibrationDevice& device);
        // marks a device as invalid; space calibrator will attempt reassigning an id next frame to it
        inline void unassignTarget(CalibrationDevice& device) {
            device.deviceId = vr::k_unTrackedDeviceIndexInvalid;
        }

        [[nodiscard]] inline const float getCalibrationProgress() const {
            return getSampleCount() == 0 ? 0.0f : (float)((double)m_samples.size() / (double)getSampleCount());
        }

        [[nodiscard]] inline const bool isContinuousCalibration() const {
            return state == CalibrationState::CONTINUOUS_IDLE || state == CalibrationState::CONTINUOUS;
        }

        [[nodiscard]] inline const bool isValidCalibration() const {
            return calibrationError == CalibrationError::None;
        }

        bool isActive = false; // enabled in the UI
        CalibrationError calibrationError = CalibrationError::Unknown; // error state of the last calibration, to be used by ui
        bool hmdIsInReferenceTrackingSystem = false; // whether the hmd is part of the reference device tracking system
        bool isRelativeCalibration = false; // whether the calibration is stored such that its coordinate system is relative to the reference device. this hides tracking anomalies from the reference device and keeps calibrations "valid" for longer
        bool hideContinuousTracker = false;
        CalibrationSpeed calibrationSpeed = CalibrationSpeed::FAST;
        CalibrationState state = CalibrationState::NONE;

        CalibrationDevice referenceDevice; // what we are calibrating to (ie this will become the ABSOLUTE ORIGIN)
        CalibrationDevice targetDevice; // what we are calibrating (ie this tracking system will be manipulated to match the ref)

        // the calibrated pose
        Eigen::Quaterniond calibratedRotation = Eigen::Quaterniond::Identity();
        Eigen::Vector3d calibratedTranslation = Eigen::Vector3d::Zero();
        double calibratedScale = 1.0;

        double wantedUpdateInterval = 1.0;

    private:
        Sample_t collectSample() const;
        inline size_t getSampleCount() const { return (size_t) calibrationSpeed; }
        [[nodiscard]] inline size_t getMaxSampleHistorySize() const { return (size_t) calibrationSpeed * 5; }

        // a delta sample, used for calibration internal maths
        struct DeltaSample_t {
            bool valid = false;
            Eigen::Vector3d reference;
            Eigen::Vector3d target;
        };

        // math funcs
        inline Eigen::Vector3d axisFromRotationMatrix3(Eigen::Matrix3d rot) {
            return Eigen::Vector3d(rot(2, 1) - rot(1, 2), rot(0, 2) - rot(2, 0), rot(1, 0) - rot(0, 1));
        }

        inline double angleFromRotationMatrix3(Eigen::Matrix3d rot) {
            return acos((rot(0, 0) + rot(1, 1) + rot(2, 2) - 1.0) / 2.0);
        }

        DeltaSample_t deltaRotationSamples(const Sample_t& s1, const Sample_t& s2);
        Eigen::Quaterniond calibrateRotation(const std::vector<Sample_t>& samples);
        Eigen::Vector3d calibrateTranslation(const std::vector<Sample_t>& samples, const Eigen::Quaterniond& calibratedRotation);
        bool makeCalibrationLocal(Eigen::Quaterniond& rotation, Eigen::Vector3d& translation);
        CalibrationError computeCalibrationOneshot(double currentTime, bool bForceCalibration); // computes instantaneous calibration

        Pose_t applyTransform(const Pose_t& originalPose, const Eigen::AffineCompact3d& transform) const;
        double retargetingErrorRMS(const Eigen::Vector3d& hmdToTargetPos, const Eigen::AffineCompact3d& calibration) const;
        Eigen::Vector3d computeRefToTargetOffset(const Eigen::AffineCompact3d& calibration) const;
        Eigen::Vector4d computeAxisVariance(const Eigen::Quaterniond& rotation, const Eigen::Vector3d& translation) const;
        bool validateCalibration(const Eigen::Quaterniond& rotation, const Eigen::Vector3d& translation, double& rmsError, Eigen::Vector3d& posOffset);

        // we collect a series of **VALID** calibrations' worth of samples to improve RMS error accuracy
        void trackCollectedSamplesForErrorTracking();

    private:
        Eigen::Quaterniond m_calibRelative_refRotation = Eigen::Quaterniond::Identity();
        Eigen::Vector3d m_calibRelative_refTranslation = Eigen::Vector3d::Zero();

        double m_lastTick = 0.0;
        double m_lastScan = 0.0;
        
        float m_xTargetPrev = 0.0f;
        float m_yTargetPrev = 0.0f;
        float m_zTargetPrev = 0.0f;
        
        float m_xRefPrev = 0.0f;
        float m_yRefPrev = 0.0f;
        float m_zRefPrev = 0.0f;

        // @TODO: required? we recompute each time so maybe not even required ig
        double m_lastRmsError = INFINITY;
        double m_lastAxisVariance = 0.0;
        double m_lastSuccessfulCalibTime = 0.0;

        std::vector<Sample_t> m_samples;
        std::deque<Sample_t> m_sampleHistory; // used ONLY for RMS validation

        friend class CalibrationManager;
    };

    // Global manager for calibrations, once instance per VR session
    class CalibrationManager {
    public:
        explicit CalibrationManager();
        void init();
        void start(); // @FIXME: how do i even define a calibration????
        void shutdown();

        void apply(); // applies all calibrations to all devices

        bool loadConfig();
        void saveConfig() const;

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
        bool m_needToApplyTransformsAfterInit = false;

        vr::DriverPose_t m_poses[vr::k_unMaxTrackedDeviceCount] = {};
        std::vector<TrackingSystemCalibration> m_calibrations;
        VRState m_vrState;

        ipc::IpcClient m_ipcClient;

        static CalibrationManager* s_instance;
        friend class ::ipc::IpcClient;
        friend class TrackingSystemCalibration;
    };
} // namespace spacecal