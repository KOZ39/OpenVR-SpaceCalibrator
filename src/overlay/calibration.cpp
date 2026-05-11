#include "calibration.h"
#include "Eigen/Geometry"
#include "config/configuration_data_versions.h"
#include "log.h"
#include "platform.h"
#include "constants.h"
#include "configuration.h"
#include <GLFW/glfw3.h>

namespace spacecal {

    CalibrationManager* CalibrationManager::s_instance = nullptr;

    // 3x4 pose matrix decomposition
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
    
    Pose_t::Pose_t(const Eigen::AffineCompact3d& transform) {
        rot = transform.rotation();
        trans = transform.translation();
    }

    Pose_t::Pose_t(const vr::DriverPose_t& driverPose) {
        // apply offsets to get worldspace pose
        Eigen::Quaterniond driverToWorldQ(
            driverPose.qWorldFromDriverRotation.w,
            driverPose.qWorldFromDriverRotation.x,
            driverPose.qWorldFromDriverRotation.y,
            driverPose.qWorldFromDriverRotation.z
        );
        Eigen::Vector3d driverToWorldV(
            driverPose.vecWorldFromDriverTranslation[0],
            driverPose.vecWorldFromDriverTranslation[1],
            driverPose.vecWorldFromDriverTranslation[2]
        );

        Eigen::Quaterniond driverRot = driverToWorldQ * Eigen::Quaterniond(
            driverPose.qRotation.w,
            driverPose.qRotation.x,
            driverPose.qRotation.y,
            driverPose.qRotation.z
        );

        Eigen::Vector3d driverPos = driverToWorldV + driverToWorldQ * Eigen::Vector3d(
            driverPose.vecPosition[0],
            driverPose.vecPosition[1],
            driverPose.vecPosition[2]
        );

        Eigen::AffineCompact3d xform = Eigen::Translation3d(driverPos) * driverRot;

        rot = xform.rotation();
        trans = xform.translation();
    }

    TrackingSystemCalibration::DeltaSample_t TrackingSystemCalibration::deltaRotationSamples(const Sample_t& s1, const Sample_t& s2) {
        // Difference in rotation between samples.
        auto dref = s1.reference.rot * s2.reference.rot.transpose();
        auto dtarget = s1.target.rot * s2.target.rot.transpose();

        // When stuck together, the two tracked objects rotate as a pair,
        // therefore their axes of rotation must be equal between any given pair of samples.
        DeltaSample_t ds = {};
        ds.reference = axisFromRotationMatrix3(dref);
        ds.target = axisFromRotationMatrix3(dtarget);

        // Reject samples that were too close to each other.
        double refA = angleFromRotationMatrix3(dref);
        double targetA = angleFromRotationMatrix3(dtarget);
        constexpr double k_ROTATION_ANGLE_THRESHOLD = 0.4;
        constexpr double k_ROTATION_MAGNITUDE_THRESHOLD = 0.1;
        ds.valid = refA > k_ROTATION_ANGLE_THRESHOLD && targetA > k_ROTATION_ANGLE_THRESHOLD && ds.reference.norm() > k_ROTATION_MAGNITUDE_THRESHOLD && ds.target.norm() > k_ROTATION_MAGNITUDE_THRESHOLD;

        ds.reference.normalize();
        ds.target.normalize();
        return ds;
    }

    Eigen::Quaterniond TrackingSystemCalibration::calibrateRotation(const std::vector<Sample_t>& samples) {
        std::vector<DeltaSample_t> deltas;

        for (size_t i = 0; i < samples.size(); i++) {
            for (size_t j = 0; j < i; j++) {
                DeltaSample_t delta = deltaRotationSamples(samples[i], samples[j]);
                if (delta.valid)
                    deltas.push_back(delta);
            }
        }
        LOG_CALIB_INFO("Got {} samples with {} delta samples", samples.size(), deltas.size());

        // Kabsch algorithm
        Eigen::MatrixXd refPoints(deltas.size(), 3), targetPoints(deltas.size(), 3);
        Eigen::Vector3d refCentroid(0, 0, 0), targetCentroid(0, 0, 0);

        for (size_t i = 0; i < deltas.size(); i++) {
            refPoints.row(i) = deltas[i].reference;
            refCentroid += deltas[i].reference;

            targetPoints.row(i) = deltas[i].target;
            targetCentroid += deltas[i].target;
        }

        refCentroid /= (double)deltas.size();
        targetCentroid /= (double)deltas.size();

        for (size_t i = 0; i < deltas.size(); i++) {
            refPoints.row(i) -= refCentroid;
            targetPoints.row(i) -= targetCentroid;
        }

        auto crossCV = refPoints.transpose() * targetPoints;
        auto svd = crossCV.bdcSvd<Eigen::ComputeThinU | Eigen::ComputeThinV>();

        Eigen::Matrix3d i = Eigen::Matrix3d::Identity();
        if ((svd.matrixU() * svd.matrixV().transpose()).determinant() < 0) {
            i(2, 2) = -1;
        }

        Eigen::Matrix3d rot = svd.matrixV() * i * svd.matrixU().transpose();
        rot.transposeInPlace();

        Eigen::Quaterniond rotQuat(rot);
        rotQuat.normalize();

        Eigen::Vector3d euler = rot.canonicalEulerAngles(2, 1, 0) * (180.0 / EIGEN_PI);

        LOG_CALIB_INFO("Calibrated rotation (deg): yaw={:.2f} pitch={:.2f} roll={:.2f}", euler[1], euler[2], euler[0]);
        return rotQuat;
    }

    Eigen::Vector3d TrackingSystemCalibration::calibrateTranslation(const std::vector<Sample_t>& samples, const Eigen::Quaterniond& R) {
        std::vector<std::pair<Eigen::Vector3d, Eigen::Matrix3d>> deltas;

        // rotation is only applied to the target tracking system, as that's the tracking system the calibration is targeting! we do not need to modify the reference pose!
        for (size_t i = 0; i < samples.size(); i++) {
            Sample_t sample_i = samples[i];
            sample_i.target.rot = R * sample_i.target.rot;
            sample_i.target.trans = R * sample_i.target.trans;

            for (size_t j = 0; j < i; j++) {
                Sample_t sample_j = samples[j];
                sample_j.target.rot = R * sample_j.target.rot;
                sample_j.target.trans = R * sample_j.target.trans;

                auto QAi = sample_i.reference.rot.transpose();
                auto QAj = sample_j.reference.rot.transpose();
                auto dQA = QAj - QAi;
                auto CA = QAj * (sample_j.reference.trans - sample_j.target.trans) - QAi * (sample_i.reference.trans - sample_i.target.trans);
                deltas.push_back(std::make_pair(CA, dQA));

                auto QBi = sample_i.target.rot.transpose();
                auto QBj = sample_j.target.rot.transpose();
                auto dQB = QBj - QBi;
                auto CB = QBj * (sample_j.reference.trans - sample_j.target.trans) - QBi * (sample_i.reference.trans - sample_i.target.trans);
                deltas.push_back(std::make_pair(CB, dQB));
            }
        }

        Eigen::VectorXd constants(deltas.size() * 3);
        Eigen::MatrixXd coefficients(deltas.size() * 3, 3);

        for (size_t i = 0; i < deltas.size(); i++) {
            for (int axis = 0; axis < 3; axis++) {
                constants(i * 3 + axis) = deltas[i].first(axis);
                coefficients.row(i * 3 + axis) = deltas[i].second.row(axis);
            }
        }

        Eigen::Vector3d trans = coefficients.bdcSvd<Eigen::ComputeThinU | Eigen::ComputeThinV>().solve(constants);
        LOG_CALIB_INFO("Calibrated translation (cm): x={:.2f} y={:.2f} z={:.2f}", trans[0] * 100.0, trans[1] * 100.0, trans[2] * 100.0);
        return trans;
    }

    bool TrackingSystemCalibration::makeCalibrationLocal() {
        if (isRelativeCalibration && !m_samples.empty()) {
            Sample_t& lastSample = m_samples.back();
            if (hmdIsInReferenceTrackingSystem) {
                lastSample.reference = Pose_t(CalibrationManager::getInstance()->m_poses[vr::k_unTrackedDeviceIndex_Hmd]);
            }

            // take calibration as a worldspace transformation matrix; take its inverse and apply it to the last known worldspace pose of the ref device
            Eigen::Affine3d worldCalib = Eigen::Translation3d(calibratedTranslation) * calibratedRotation;
            Eigen::Affine3d refPose = Eigen::Translation3d(lastSample.reference.trans) * lastSample.reference.rot;
            Eigen::Affine3d localCalib = refPose.inverse() * worldCalib;

            calibratedRotation = Eigen::Quaterniond(localCalib.rotation());
            calibratedTranslation = localCalib.translation();

            Eigen::Vector3d euler = calibratedRotation.toRotationMatrix().canonicalEulerAngles(2, 1, 0) * (180.0 / EIGEN_PI);
            LOG_CALIB_INFO("Converted to local space. rotation (deg): yaw={:.2f} pitch={:.2f} roll={:.2f} ; translation (cm): x={:.2f} y={:.2f} z={:.2f}",
                euler[1], euler[2], euler[0],
                calibratedTranslation[0] * 100.0, calibratedTranslation[1] * 100.0, calibratedTranslation[2] * 100.0
            );
            return true;
        }
        return false;
    }

    Sample_t TrackingSystemCalibration::collectSample() const {
        vr::DriverPose_t reference, target;
        reference.poseIsValid = false;
        target.poseIsValid = false;

        bool bIsTrackingOk = true;

        if (referenceDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
            reference = CalibrationManager::getInstance()->m_poses[this->referenceDevice.deviceId];
            if (!(reference.deviceIsConnected && reference.poseIsValid && reference.result == vr::ETrackingResult::TrackingResult_Running_OK)) {
                // dont spam logs
                if (reference.deviceIsConnected) {
                    LOG_CALIB_WARN("Reference device is not tracking");
                }
                bIsTrackingOk = false;
            }
        }
        if (targetDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
            target = CalibrationManager::getInstance()->m_poses[this->targetDevice.deviceId];
            if (!(target.deviceIsConnected && target.poseIsValid && target.result == vr::ETrackingResult::TrackingResult_Running_OK)) {
                // dont spam logs
                if (target.deviceIsConnected) {
                    LOG_CALIB_WARN("Target device is not tracking");
                }
                bIsTrackingOk = false;
            }
        }

        if (!bIsTrackingOk) {
            return Sample_t { .isPoseValid = bIsTrackingOk };
        }

        return Sample_t {
            .reference = Pose_t(reference),
            .target = Pose_t(target),
            .timestamp = glfwGetTime(),
            .isPoseValid = bIsTrackingOk
        };
    }

    void TrackingSystemCalibration::init() {
        // @TODO: 

    }
    
    void TrackingSystemCalibration::reset() {
        // @TODO: 
        m_samples.clear();
    }

    void TrackingSystemCalibration::start() {
        reset();
        state = CalibrationState::START;
        wantedUpdateInterval = 0.0;
        assignTarget(referenceDevice);
        assignTarget(targetDevice);

        // update state
        const auto& hmdDevice = VRState::getInstance()->getVrDevice(vr::k_unTrackedDeviceIndex_Hmd);
        hmdIsInReferenceTrackingSystem = hmdDevice.szTrackingSystemId == referenceDevice.trackingSystem;
    }
    
    void TrackingSystemCalibration::calibrationTick(const double currentTime) {
        if (!vr::VRSystem())
            return;

        if ((currentTime - m_lastTick) < (1.0 / k_TICK_RATE_HZ))
            return;

        m_lastTick = currentTime;

        // original code checks hmd specifically, we should check that the hmd and ref arent at 0 0 0
        // check that ref is not at origin
        if (hmdIsInReferenceTrackingSystem) {
            if (targetDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
                auto targetPose = CalibrationManager::getInstance()->m_poses[targetDevice.deviceId].vecPosition;
                if ((targetPose[0] == 0.0 && targetPose[1] == 0.0 && targetPose[2] == 0.0) ||
                    (m_xTargetPrev == targetPose[0] && m_yTargetPrev == targetPose[1] && m_zTargetPrev == targetPose[2])) {
                    // LOG_CALIB_WARN("HMD tracking didn't update, skipping update");
                    return;
                }
                m_xTargetPrev = (float)targetPose[0];
                m_yTargetPrev = (float)targetPose[1];
                m_zTargetPrev = (float)targetPose[2];
            }
            if (referenceDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
                auto refPose = CalibrationManager::getInstance()->m_poses[referenceDevice.deviceId].vecPosition;
                if ((refPose[0] == 0.0 && refPose[1] == 0.0 && refPose[2] == 0.0) ||
                    (m_xRefPrev == refPose[0] && m_yRefPrev == refPose[1] && m_zRefPrev == refPose[2])) {
                    // LOG_CALIB_WARN("HMD tracking didn't update, skipping update");
                    return;
                }
                m_xRefPrev = (float)refPose[0];
                m_yRefPrev = (float)refPose[1];
                m_zRefPrev = (float)refPose[2];
            }
        }

        if (state == CalibrationState::NONE) {
            wantedUpdateInterval = 1.0;
            if ((currentTime - m_lastScan) >= 1.0) {
                assignTarget(referenceDevice);
                assignTarget(targetDevice);
                m_lastScan = currentTime;
            }
            return;
        }

        if (state == CalibrationState::EDITING) {
            wantedUpdateInterval = 0.1;
            if ((currentTime - m_lastScan) >= 0.1) {
                assignTarget(referenceDevice);
                assignTarget(targetDevice);
                m_lastScan = currentTime;
            }
            return;
        }

        if (state == CalibrationState::START) {
            bool ok = true;

            LOG_CALIB_INFO("Beginning calibration...");
            LOG_CALIB_INFO("  Reference device: ID: {}, tracking system: {}, model: {} serial: {}", referenceDevice.deviceId, referenceDevice.trackingSystem, referenceDevice.deviceModel, referenceDevice.deviceSerialNumber);
            LOG_CALIB_INFO("  Target device: ID: {}, tracking system: {}, model: {} serial: {}", targetDevice.deviceId, targetDevice.trackingSystem, targetDevice.deviceModel, targetDevice.deviceSerialNumber);

            if (referenceDevice.deviceId >= vr::k_unMaxTrackedDeviceCount) {
                LOG_CALIB_ERROR("Missing reference device");
                ok = false;
            }
            if (targetDevice.deviceId >= vr::k_unMaxTrackedDeviceCount) {
                LOG_CALIB_ERROR("Missing target device");
                ok = false;
            }
            
            Sample_t sample = collectSample();
            if (!sample.isPoseValid) {
                ok = false;
            }

            if (!ok) {
                state = CalibrationState::NONE;
                LOG_CALIB_ERROR("Aborting calibration!");
                return;
            }

            resetCalibrationForDevice(targetDevice);
            state = CalibrationState::ROTATION;
            wantedUpdateInterval = 0.0;

            LOG_CALIB_INFO("Starting calibration...");
            return;
        }

        // prevent overfitting with possibly poor data
        if (m_samples.size() < getSampleCount()) {
            auto sample = collectSample();
            if (sample.isPoseValid) {
                // we want to completely ignore poor samples during calibration
                m_samples.push_back(sample);
            }
        }

        if (m_samples.size() == getSampleCount()) {
            switch (state) {
            case CalibrationState::ROTATION:
            {
                calibratedRotation = calibrateRotation(m_samples);

                ipc::protocol::Command_SetDeviceTransform_t args = {};
                args.unTargetOpenVrDeviceId = targetDevice.deviceId;
                args.unReferenceOpenvrDeviceId = vr::k_unTrackedDeviceIndex_Hmd;
                args.enabled(true);
                args.relativeCoordSystem(false);
                args.quirks = targetDevice.quirks;
                args.updateRotation(true);
                args.rotation.x = calibratedRotation.x();
                args.rotation.y = calibratedRotation.y();
                args.rotation.z = calibratedRotation.z();
                args.rotation.w = calibratedRotation.w();
                CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);

                state = CalibrationState::TRANSLATION;
                break;
            }
            case CalibrationState::TRANSLATION:
            {
                // apply samples to avoid sampling twice
                calibratedTranslation = calibrateTranslation(m_samples, calibratedRotation);

                ipc::protocol::Command_SetDeviceTransform_t args = {};
                args.unTargetOpenVrDeviceId = targetDevice.deviceId;
                args.unReferenceOpenvrDeviceId = referenceDevice.deviceId;
                args.enabled(true);
                args.relativeCoordSystem(false);
                args.quirks = targetDevice.quirks;
                args.updateTranslation(true);
                args.translation.v[0] = calibratedTranslation.x();
                args.translation.v[1] = calibratedTranslation.y();
                args.translation.v[2] = calibratedTranslation.z();
                CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);

                bool bMakeLocalSuccess = makeCalibrationLocal();

                isValidCalibration = true;
                // SaveProfile(ctx);
                LOG_CALIB_INFO("Finished calibration, profile saved");

                apply();
                CalibrationManager::getInstance()->saveConfig();

                state = CalibrationState::NONE;
                break;
            }
            case CalibrationState::SCALE:
            {
                // @TODO: robust measure of scale algorithm?
                // apply samples to avoid sampling twice
                calibratedTranslation = calibrateTranslation(m_samples, calibratedRotation);

                isValidCalibration = true;
                // SaveProfile(ctx);
                LOG_CALIB_INFO("Finished calibration, profile saved");

                apply();

                state = CalibrationState::NONE;
                break;
            }
            default:
                break;
            }
        }
    }
    
    void TrackingSystemCalibration::assignTarget(CalibrationDevice& device) {
        if (device.deviceId == vr::k_unTrackedDeviceIndexInvalid) {
            auto theDevice = VRState::getInstance()->findVrDevice(device.trackingSystem, device.deviceModel, device.deviceSerialNumber);
            if (theDevice.bIsConnected && theDevice.dwDeviceIndex < vr::k_unMaxTrackedDeviceCount) {
                device.deviceId = theDevice.dwDeviceIndex;
            }
        }
    }

    void TrackingSystemCalibration::resetCalibrationForDevice(const CalibrationDevice& device) {
        vr::HmdVector3d_t posOrigin = { .v = { 0, 0, 0 } };
        vr::HmdQuaternion_t identityQuat = { .w = 1, .x = 0,.y = 0,.z = 0 };

        ipc::protocol::Command_SetDeviceTransform_t args = {};

        args.unTargetOpenVrDeviceId = device.deviceId;
        args.unReferenceOpenvrDeviceId = vr::k_unTrackedDeviceIndex_Hmd;
        args.enabled(false);
        args.relativeCoordSystem(false);
        args.updateTranslation(true);
        args.translation = posOrigin;
        args.updateRotation(true);
        args.rotation = identityQuat;
        args.updateScale(true);
        args.scale = calibratedScale;

        CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);
    }
    
    // applies calibration to all devices under this tracking system
    void TrackingSystemCalibration::apply() {
        this->isActive = isValidCalibration;

        auto hmdDevice = VRState::getInstance()->getVrDevice(vr::k_unTrackedDeviceIndex_Hmd);
        if (this->hmdIsInReferenceTrackingSystem && hmdDevice.szTrackingSystemId != referenceDevice.trackingSystem) {
            // if the hmd the hmd's tracking system is not what was saved, handles users changing streamer / VR headset properly by not applying calibration
            this->isActive = false;
            return;
        }

        for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            auto device = VRState::getInstance()->getVrDevice(i);
            if (device.eDeviceClass != vr::TrackedDeviceClass_Invalid) {
                // if this is not the target tracking system
                if (device.szTrackingSystemId != targetDevice.trackingSystem) {
                    continue;
                }

                ipc::protocol::Command_SetDeviceTransform_t args = {};

                args.unTargetOpenVrDeviceId = i;
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

                    // for relative calibrations
                    args.relativeCoordSystem(isRelativeCalibration);
                    args.unReferenceOpenvrDeviceId = hmdIsInReferenceTrackingSystem ? vr::k_unTrackedDeviceIndex_Hmd : referenceDevice.deviceId;

                    args.hideContinuousTracker(isContinuousCalibration() && hideContinuousTracker);
                    args.lerpCalibrations(isContinuousCalibration());
                }

                CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);
            }
        }
    }

    CalibrationManager::CalibrationManager() {
        if (s_instance != nullptr) {
            LOG_FATAL("Tried creating CalibrationManager more than once! Breaking singleton. Aborting...");
        }
        s_instance = this;

        // @FIXME: Need to figure out how to handle multiple calibrations

        // try loading calibrations from disk, if fail, fallback to a default one
        if (!loadConfig()) {
            TrackingSystemCalibration mainCalibration;
            mainCalibration.hmdIsInReferenceTrackingSystem = true;
            m_calibrations.push_back(mainCalibration);
        }
    }

    void CalibrationManager::init() {
        // @TODO: 
        if (!m_ipcClient.IsConnected()) {
            m_ipcClient.Connect();

            // initialise memory properly
            m_ipcClient.RequestVirtualDesktopProps();
            m_ipcClient.PollPoses();
        }

        for (auto& calibration : m_calibrations) {
            calibration.init();
        }
    }
    
    void CalibrationManager::shutdown() {
        if (m_ipcClient.IsConnected()) {
            m_ipcClient.Shutdown();
        }
    }

    void CalibrationManager::start() {
        // @TODO: maybe more logic here?

        for (auto& calibration : m_calibrations) {
            calibration.start();
        }
    }

    void CalibrationManager::calibrationTick(const double currentTime) {
        m_ipcClient.RequestVirtualDesktopProps();
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
            // @NOTE: is max better here?
            m_wantedUpdateInterval = wantedInterval / countedCalibrations;
        }
    }

    void CalibrationManager::apply() {
        // @TODO: maybe more logic here?

        for (auto& calibration : m_calibrations) {
            calibration.apply();
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

    bool CalibrationManager::loadConfig() {
        using namespace spacecal::config::versioned;
        const Configuration* pConfig = spacecal::ConfigurationManager::getInstance()->getConfiguration();

        // no calibrations found
        if (pConfig->calibrations.size() == 0) {
            return false;
        }

        // we need N calibrations
        this->m_calibrations.resize(pConfig->calibrations.size());

        // now load the calibration data
        for (size_t i = 0; i < pConfig->calibrations.size(); i++) {
            this->m_calibrations[i].isActive = pConfig->calibrations[i].is_active;

            // copy device props
            this->m_calibrations[i].targetDevice.deviceModel            = pConfig->calibrations[i].target_device.model;
            this->m_calibrations[i].targetDevice.deviceSerialNumber     = pConfig->calibrations[i].target_device.serial;
            this->m_calibrations[i].targetDevice.trackingSystem         = pConfig->calibrations[i].target_device.tracking_system;
            
            this->m_calibrations[i].referenceDevice.deviceModel         = pConfig->calibrations[i].reference_device.model;
            this->m_calibrations[i].referenceDevice.deviceSerialNumber  = pConfig->calibrations[i].reference_device.serial;
            this->m_calibrations[i].referenceDevice.trackingSystem      = pConfig->calibrations[i].reference_device.tracking_system;

            this->m_calibrations[i].isRelativeCalibration               = pConfig->calibrations[i].anchor_mode == Configuration_Latest::AnchorMode::HmdRelative;
            this->m_calibrations[i].calibrationSpeed                    = (CalibrationSpeed) pConfig->calibrations[i].calibration_speed;

            // calibrated transform mapping
            // @TODO: should we make a helper func?
            this->m_calibrations[i].calibratedTranslation.x()           = pConfig->calibrations[i].calibrated_transform.x;
            this->m_calibrations[i].calibratedTranslation.y()           = pConfig->calibrations[i].calibrated_transform.y;
            this->m_calibrations[i].calibratedTranslation.z()           = pConfig->calibrations[i].calibrated_transform.z;
            
            double rollRad  = pConfig->calibrations[i].calibrated_transform.roll  * (M_PI / 180.0);
            double pitchRad = pConfig->calibrations[i].calibrated_transform.pitch * (M_PI / 180.0);
            double yawRad   = pConfig->calibrations[i].calibrated_transform.yaw   * (M_PI / 180.0);

            this->m_calibrations[i].calibratedRotation =
                Eigen::AngleAxisd(yawRad, Eigen::Vector3d::UnitZ()) *
                Eigen::AngleAxisd(pitchRad, Eigen::Vector3d::UnitY()) *
                Eigen::AngleAxisd(rollRad, Eigen::Vector3d::UnitX());
        }

        return true;
    }

    void CalibrationManager::saveConfig() const {
        using namespace spacecal::config::versioned;
        Configuration* pConfig = spacecal::ConfigurationManager::getInstance()->getConfiguration();

        // we need N calibrations
        pConfig->calibrations.resize(this->m_calibrations.size());
        
        for (size_t i = 0; i < pConfig->calibrations.size(); i++) {
            pConfig->calibrations[i].is_active = this->m_calibrations[i].isActive;

            // copy device props
            pConfig->calibrations[i].target_device.model                = this->m_calibrations[i].targetDevice.deviceModel;
            pConfig->calibrations[i].target_device.serial               = this->m_calibrations[i].targetDevice.deviceSerialNumber;
            pConfig->calibrations[i].target_device.tracking_system      = this->m_calibrations[i].targetDevice.trackingSystem;
            
            pConfig->calibrations[i].reference_device.model             = this->m_calibrations[i].referenceDevice.deviceModel;
            pConfig->calibrations[i].reference_device.serial            = this->m_calibrations[i].referenceDevice.deviceSerialNumber;
            pConfig->calibrations[i].reference_device.tracking_system   = this->m_calibrations[i].referenceDevice.trackingSystem;

            pConfig->calibrations[i].anchor_mode                        = this->m_calibrations[i].isRelativeCalibration ? Configuration_Latest::AnchorMode::HmdRelative : Configuration_Latest::AnchorMode::FixedWorld;
            pConfig->calibrations[i].calibration_speed                  = (uint64_t)this->m_calibrations[i].calibrationSpeed;

            // calibrated transform mapping
            // @TODO: should we make a helper func?
            pConfig->calibrations[i].calibrated_transform.x             = (float)this->m_calibrations[i].calibratedTranslation.x();
            pConfig->calibrations[i].calibrated_transform.y             = (float)this->m_calibrations[i].calibratedTranslation.y();
            pConfig->calibrations[i].calibrated_transform.z             = (float)this->m_calibrations[i].calibratedTranslation.z();

            Eigen::Vector3d euler = this->m_calibrations[i].calibratedRotation.toRotationMatrix().canonicalEulerAngles(2, 1, 0);
            pConfig->calibrations[i].calibrated_transform.yaw           = static_cast<float>(euler[0] * 180.0 / M_PI);
            pConfig->calibrations[i].calibrated_transform.pitch         = static_cast<float>(euler[1] * 180.0 / M_PI);
            pConfig->calibrations[i].calibrated_transform.roll          = static_cast<float>(euler[2] * 180.0 / M_PI);
        }

        ConfigurationError err = spacecal::ConfigurationManager::getInstance()->saveConfiguration();
        if (err != ConfigurationError::Ok) {
            LOG_WARNING("Failed to write calibration, got {}", (uint32_t)err);
        }
    }
}