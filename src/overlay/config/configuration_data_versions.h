#pragma once

#include <inttypes.h>
#include <string>

namespace spacecal {
    namespace config {
        namespace versioned {

            enum class DataVersions : uint32_t {
                Legacy = 0xFFFFFFFFU, // pushrax / bd versions
                _0 = 0,               // nova v1

                Current = _0,
                Count,
            };

            // this is a pre-nova config. do NOT write to this, as we upgrade it to a nova
            // calibration upon first load
            struct ConfigurationLegacy {
                struct LegacyTrackingDevice {
                    std::string model;
                    std::string serial;
                    std::string tracking_system;
                };

                struct LegacyAlignmentParams {
                    double align_speed_large;
                    double align_speed_small;
                    double align_speed_tiny;
                    float continuousCalibrationThreshold;
                    double thr_rot_large;
                    double thr_rot_small;
                    double thr_rot_tiny;
                    double thr_trans_large;
                    double thr_trans_small;
                    double thr_trans_tiny;
                } alignmentParams;

                bool autostart_continuous_calibration;
                bool ignore_outliers;
                bool lock_relative_position;
                bool quash_target_in_continuous; // hide tracker (why was it called quash in
                // legacy??)
                bool relative_pos_calibrated;
                bool require_trigger_press_to_apply;
                bool static_calibration;
                uint32_t calibration_speed; // enum: FAST: 0, SLOW: 1, VERY_SLOW: 2
                double continuous_calibration_target_offset_x;
                double continuous_calibration_target_offset_y;
                double continuous_calibration_target_offset_z;

                int jitter_threshold;
                float max_relative_error_threshold;
                LegacyTrackingDevice reference_device;
                std::string reference_tracking_system;
                struct LegacyRelativeTransform {
                    double x;
                    double y;
                    double z;
                    double yaw;
                    double pitch;
                    double roll;
                } relative_transform;
                double scale;
                LegacyTrackingDevice target_device;
                std::string target_tracking_system;
                double x;
                double y;
                double z;
                double yaw;
                double roll;
                double pitch;
            };

            struct Configuration_0 {
                spacecal::config::versioned::DataVersions dataVersion = spacecal::config::versioned::DataVersions::_0;

                enum class AnchorMode : uint32_t {
                    FixedWorld = 0,         // Static offset (Legacy style)
                    HmdRelative = 1         // Dynamic: Entire System follows the HMD
                };

                struct TrackingDevice {
                    std::string model;
                    std::string serial;
                    std::string tracking_system;
                };
                struct Transform {
                    float x, y, z; // pos
                    float yaw, pitch, roll; // rot
                };

                bool isActive = false;

                Transform refToTargetTransform;
                Transform calibratedTransform;

                Transform anchorTransform;
                AnchorMode anchorMode = AnchorMode::FixedWorld;

                std::string hmdTrackingSystem;

                TrackingDevice referenceDevice;
                TrackingDevice targetDevice;
            };

            typedef Configuration_0 Configuration_Latest;
        } // namespace versioned
    } // namespace config
} // namespace spacecal