#pragma once

#include <inttypes.h>
#include <string>
#include <vector>
#include <unordered_map>

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
                    double align_speed_large = 0.0;
                    double align_speed_small = 0.0;
                    double align_speed_tiny = 0.0;
                    float continuousCalibrationThreshold = 0.0f;
                    double thr_rot_large = 0.0;
                    double thr_rot_small = 0.0;
                    double thr_rot_tiny = 0.0;
                    double thr_trans_large = 0.0;
                    double thr_trans_small = 0.0;
                    double thr_trans_tiny = 0.0;
                } alignmentParams;

                bool autostart_continuous_calibration = false;
                bool ignore_outliers = false;
                bool lock_relative_position = false;
                bool quash_target_in_continuous = false; // hide tracker (why was it called quash in legacy??)
                bool relative_pos_calibrated = false;
                bool require_trigger_press_to_apply = false;
                bool static_calibration = false;
                uint32_t calibration_speed = 0; // enum: FAST: 0, SLOW: 1, VERY_SLOW: 2
                double continuous_calibration_target_offset_x = 0.0;
                double continuous_calibration_target_offset_y = 0.0;
                double continuous_calibration_target_offset_z = 0.0;

                int jitter_threshold = 0;
                float max_relative_error_threshold = 0.0;
                LegacyTrackingDevice reference_device;
                std::string reference_tracking_system;
                struct LegacyRelativeTransform {
                    double x = 0.0;
                    double y = 0.0;
                    double z = 0.0;
                    double yaw = 0.0;
                    double pitch = 0.0;
                    double roll = 0.0;
                } relative_transform;
                double scale = 1.0;
                LegacyTrackingDevice target_device;
                std::string target_tracking_system;
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;
                double yaw = 0.0;
                double roll = 0.0;
                double pitch = 0.0;
            };

            struct Configuration_0 {
                spacecal::config::versioned::DataVersions dataVersion = spacecal::config::versioned::DataVersions::_0;

                enum class AnchorMode : uint32_t {
                    FixedWorld = 0,         // Worldspace calibration
                    HmdRelative = 1         // Relative to reference device (typically HMD)
                };

                struct TrackingDevice {
                    std::string model;
                    std::string serial;
                    std::string tracking_system;
                };
                struct Transform {
                    float x = 0.0f, y = 0.0f, z = 0.0f; // pos, meters
                    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f; // rot, deg
                };

                struct ContinuousCalibrationData {
                    bool is_active = false;
                    bool hide_reference_tracker = false;
                };

                struct Calibration_t {
                    bool is_active = false;

                    Transform calibrated_transform;
                    float scale = 1.0f;

                    AnchorMode anchor_mode = AnchorMode::FixedWorld;
                    ContinuousCalibrationData continuous;

                    bool calibrate_motion_vectors = true;
                    bool attempt_auto_fix_playspace_jumps = true;
                    bool enforce_minimum_rotation_variance = true;
                    uint64_t calibration_speed = 0;

                    TrackingDevice reference_device;
                    TrackingDevice target_device;
                };

                struct BaseStationManagementData {
                    bool auto_power_management_enabled = false;
                    bool auto_turn_on_during_startup = true;
                    bool auto_turn_off_during_shutdown = true;
                    bool off_should_use_standby = false;
                    std::unordered_map<std::string, std::string> nicknames; // KV pair -> "LHB XXXXXXXX" to "my nick name"
                };

                std::string ui_locale = "system";
                bool advanced_settings = false;

                BaseStationManagementData base_stations;

                std::vector<Calibration_t> calibrations;
            };

            typedef Configuration_0 Configuration_Latest;
        } // namespace versioned
    } // namespace config
} // namespace spacecal