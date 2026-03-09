#include "calibration.h"
#include "log.h"
#include "platform.h"
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

    inline Eigen::Vector3d axisFromRotationMatrix3(Eigen::Matrix3d rot) {
        return Eigen::Vector3d(rot(2, 1) - rot(1, 2), rot(0, 2) - rot(2, 0), rot(1, 0) - rot(0, 1));
    }

    inline double angleFromRotationMatrix3(Eigen::Matrix3d rot) {
        return acos((rot(0, 0) + rot(1, 1) + rot(2, 2) - 1.0) / 2.0);
    }

    struct DeltaSample_t {
        bool valid = false;
        Eigen::Vector3d reference;
        Eigen::Vector3d target;
    };

    DeltaSample_t deltaRotationSamples(const Sample_t& s1, const Sample_t& s2) {
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
        ds.valid = refA > 0.4 && targetA > 0.4 && ds.reference.norm() > 0.01 && ds.target.norm() > 0.01;

        ds.reference.normalize();
        ds.target.normalize();
        return ds;
    }

    Eigen::Quaterniond calibrateRotation(const std::vector<Sample_t>& samples)
    {
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

        LOG_CALIB_INFO("Calibrated rotation: yaw={.2f} pitch={.2f} roll={.2f}", euler[1], euler[2], euler[0]);
        return rotQuat;
    }

    Eigen::Vector3d calibrateTranslation(const std::vector<Sample_t>& samples)
    {
        std::vector<std::pair<Eigen::Vector3d, Eigen::Matrix3d>> deltas;

        for (size_t i = 0; i < samples.size(); i++) {
            for (size_t j = 0; j < i; j++) {
                auto QAi = samples[i].reference.rot.transpose();
                auto QAj = samples[j].reference.rot.transpose();
                auto dQA = QAj - QAi;
                auto CA = QAj * (samples[j].reference.trans - samples[j].target.trans) - QAi * (samples[i].reference.trans - samples[i].target.trans);
                deltas.push_back(std::make_pair(CA, dQA));

                auto QBi = samples[i].target.rot.transpose();
                auto QBj = samples[j].target.rot.transpose();
                auto dQB = QBj - QBi;
                auto CB = QBj * (samples[j].reference.trans - samples[j].target.trans) - QBi * (samples[i].reference.trans - samples[i].target.trans);
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
        auto transcm = trans * 100.0;

        LOG_CALIB_INFO("Calibrated translation: x={.2f} y={.2f} z={.2f}", transcm[0], transcm[1], transcm[2]);
        return transcm;
    }


    Sample_t TrackingSystemCalibration::collectSample() const {
        vr::DriverPose_t reference, target;
        reference.poseIsValid = false;
        target.poseIsValid = false;

        bool bIsTrackingOk = true;

        if (referenceDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
            reference = CalibrationManager::getInstance()->m_poses[this->referenceDevice.deviceId];
            if (!(reference.deviceIsConnected && reference.poseIsValid && reference.result != vr::ETrackingResult::TrackingResult_Running_OK)) {
                // dont spam logs
                if (reference.deviceIsConnected) {
                    LOG_CALIB_WARN("Reference device is not tracking\n");
                }
                bIsTrackingOk = false;
            }
        }
        if (targetDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
            target = CalibrationManager::getInstance()->m_poses[this->targetDevice.deviceId];
            if (!(target.deviceIsConnected && target.poseIsValid && target.result != vr::ETrackingResult::TrackingResult_Running_OK)) {
                // dont spam logs
                if (target.deviceIsConnected) {
                    LOG_CALIB_WARN("Target device is not tracking\n");
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

    void TrackingSystemCalibration::start() {
        state = CalibrationState::START;
        wantedUpdateInterval = 0.0;
        assignTarget(referenceDevice);
        assignTarget(targetDevice);
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
            if (targetDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
                auto targetPose = CalibrationManager::getInstance()->m_poses[targetDevice.deviceId].vecPosition;
                if ((targetPose[0] == 0.0 && targetPose[1] == 0.0 && targetPose[2] == 0.0) || (m_xPrev == targetPose[0] && m_yPrev == targetPose[1] && m_zPrev == targetPose[2])) {
                    // std::cerr << "HMD tracking didn't update, skipping update" << std::endl;
                    return;
                }
            }
            if (referenceDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
                auto refPose = CalibrationManager::getInstance()->m_poses[referenceDevice.deviceId].vecPosition;
                if ((refPose[0] == 0.0 && refPose[1] == 0.0 && refPose[2] == 0.0) || (m_xPrev == refPose[0] && m_yPrev == refPose[1] && m_zPrev == refPose[2])) {
                    // std::cerr << "HMD tracking didn't update, skipping update" << std::endl;
                    return;
                }
            }
        }
        m_xPrev = (float)p[0];
        m_yPrev = (float)p[1];
        m_zPrev = (float)p[2];

        if (state == CalibrationState::NONE) {
            wantedUpdateInterval = 1.0;
            if ((currentTime - m_lastTick) >= 1.0) {
                assignTarget(referenceDevice);
                assignTarget(targetDevice);
                m_lastTick = currentTime;
            }
            return;
        }

        if (state == CalibrationState::EDITING) {
            wantedUpdateInterval = 0.1;
            if ((currentTime - m_lastTick) >= 0.1) {
                assignTarget(referenceDevice);
                assignTarget(targetDevice);
                m_lastTick = currentTime;
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
            
            auto sample = collectSample();
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

        auto sample = collectSample();
        if (sample.isPoseValid) {
            // we want to completely ignore poor samples during calibration
            m_samples.push_back(sample);
        }

        // @TODO: progress tracking

        // @TODO: rest of calibration algorithm
        if (m_samples.size() == getSampleCount()) {
            // @TODO: handle calibrating now

            if (state == CalibrationState::ROTATION)
            {
                calibratedRotation = calibrateRotation(m_samples);

                ipc::protocol::Command_SetDeviceTransform_t args = {};
                args.unOpenvrDeviceId = targetDevice.deviceId;
                args.enabled(true);
                args.quirks = targetDevice.quirks;
                args.updateRotation(true);
                args.rotation.x = calibratedRotation.x();
                args.rotation.y = calibratedRotation.y();
                args.rotation.z = calibratedRotation.z();
                args.rotation.w = calibratedRotation.w();
                CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);

                state = CalibrationState::TRANSLATION;
            }
            else if (state == CalibrationState::TRANSLATION)
            {
                calibratedTranslation = calibrateTranslation(m_samples);

                ipc::protocol::Command_SetDeviceTransform_t args = {};
                args.unOpenvrDeviceId = targetDevice.deviceId;
                args.enabled(true);
                args.quirks = targetDevice.quirks;
                args.updateTranslation(true);
                args.translation.v[0] = calibratedTranslation.x();
                args.translation.v[1] = calibratedTranslation.y();
                args.translation.v[2] = calibratedTranslation.z();
                CalibrationManager::getInstance()->m_ipcClient.SetDeviceTransform(args);

                isValidCalibration = true;
                // SaveProfile(ctx);
                LOG_CALIB_INFO("Finished calibration, profile saved");

                state = CalibrationState::NONE;
            }

            // @TODO: see if we can omit this; bc this make the algorithm a two-step alg
            //        ideally we should re-use the samples, especially because we're going
            //        to introduce another phase later for scale calibration
            m_samples.clear();
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

        args.unOpenvrDeviceId = device.deviceId;
        args.enabled(false);
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
        if (s_instance != nullptr) {
            LOG_FATAL("Tried creating CalibrationManager more than once! Breaking singleton. Aborting...");
        }
        s_instance = this;

        // @FIXME: Need to figure out how to handle multiple calibrations
        TrackingSystemCalibration mainCalibration;
        mainCalibration.hmdIsInSameTrackingSystem = true;
        m_calibrations.push_back(mainCalibration);
    }

    void CalibrationManager::init() {
        // @TODO: 
        if (!m_ipcClient.IsConnected()) {
            m_ipcClient.Connect();
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
        // @TODO: 


        for (auto& calibration : m_calibrations) {
            calibration.start();
        }
    }

    void CalibrationManager::calibrationTick(const double currentTime) {
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