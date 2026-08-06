#include "configuration.h"
#include "calibration.h"
#include "log.h"
#include "util.h"
#include "platform.h"

BEGIN_EXTERNAL_HEADERS
#include <filesystem>
#include <glaze/glaze.hpp>
END_EXTERNAL_HEADERS

namespace spacecal {

    ConfigurationManager* ConfigurationManager::s_instance = nullptr;

#if OS_WINDOWS
    std::string getLegacySettingsString() {
        // Check registry for legacy config
        // HKEY_CURRENT_USER\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator
        constexpr const char* RegistryKey = "Software\\OpenVR-SpaceCalibrator";

        DWORD size = 0;
        auto result = RegGetValueA(HKEY_CURRENT_USER_LOCAL_SETTINGS, RegistryKey, "Config", RRF_RT_REG_SZ, 0, 0, &size);
        if (result != ERROR_SUCCESS)
        {
            char* message;
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, result, LANG_USER_DEFAULT, (LPSTR)&message, 0, NULL);
            LOG_ERROR("{}", message);
            return "";
        }

        std::string str;
        str.resize(size);

        result = RegGetValueA(HKEY_CURRENT_USER_LOCAL_SETTINGS, RegistryKey, "Config", RRF_RT_REG_SZ, 0, &str[0], &size);
        if (result != ERROR_SUCCESS)
        {
            char* message;
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, result, LANG_USER_DEFAULT, (LPSTR)&message, 0, NULL);
            LOG_ERROR("{}", result);
            return "";
        }

        str.resize(size - 1);
        return str;
    }
#endif

    ConfigurationError ConfigurationManager::init() {
        if (s_instance != nullptr) {
            LOG_FATAL("Tried creating ConfigurationManager more than once! Breaking singleton. Aborting...");
            return ConfigurationError::SingletonExists;
        }
        s_instance = this;
        m_configPath = util::getSpaceCalibratorConfigPath().string();

        // Load config from disk. If it does not exist, create a new config
        ConfigurationError err = loadConfiguration();
        if (err != ConfigurationError::Ok) {
            saveConfiguration();
        }
        return err;
    }

    ConfigurationError ConfigurationManager::loadConfiguration() {

        LOG_INFO("Attempting to load calibration...");

        bool isLegacyConfig = false;
        std::string jsonConfigRaw;
        if (!std::filesystem::is_regular_file(m_configPath)) {

#if OS_WINDOWS
            LOG_WARN("Failed to find config at {}!", m_configPath);
            LOG_INFO("Attempting to load from legacy registry key...");

            jsonConfigRaw = getLegacySettingsString();
            if (jsonConfigRaw.empty()) {
                LOG_WARNING("Failed to locate configuration file \"{0}\" on disk, and legacy registry key was absent. Using default settings...", m_configPath);
                return ConfigurationError::FileNotExist;
            }
            isLegacyConfig = true;
#else
            LOG_WARNING("Failed to locate configuration file \"{0}\" on disk. Using default settings...", m_configPath);
            return ConfigurationError::FileNotExist;
#endif
        }

        constexpr glz::opts options{
            .comments = true,
            .error_on_unknown_keys = false,
            .error_on_missing_keys = false,
            .error_on_const_read = false
        };

        // Get data version to decode config correctly
        if (jsonConfigRaw.empty()) {
            FILE* pFile = fopen(m_configPath.c_str(), "rb");
            fseek(pFile, 0, SEEK_END);
            size_t fileSize = ftell(pFile);
            jsonConfigRaw.resize(fileSize);
            rewind(pFile);
            fread(&jsonConfigRaw[0], 1, fileSize, pFile);
            fclose(pFile);
        }

        glz::generic json{};
        glz::error_ctx deserialiseErrorVersion = glz::read<glz::set_json<options>()>(json, std::forward<std::string>(jsonConfigRaw), glz::context{});
        if (deserialiseErrorVersion.ec != glz::error_code::none) {
            // FUCK
            LOG_WARNING("Failed to parse configuration file \"{0}\". {1}", m_configPath, deserialiseErrorVersion.custom_error_message);
            return ConfigurationError::JsonCorrupt;
        }

        uint32_t version = isLegacyConfig ?
            (uint32_t)spacecal::config::versioned::DataVersions::Legacy :
            (uint32_t)spacecal::config::versioned::DataVersions::Current;

        if (json.contains("dataVersion") && !json.at("dataVersion").is_null() && json.at("dataVersion").is_number()) {
            version = (uint32_t)json.at("dataVersion").get_number();
            // Legacy is the u32 max, so treat it as a special case here
            if (version == (uint32_t)spacecal::config::versioned::DataVersions::Legacy) {
                LOG_INFO("Config version :: Legacy");
            } else {
                LOG_INFO("Config version :: {}", version);
            }
        } else {
            LOG_WARNING("Configuration file at \"{0}\" did not have a dataVersion field, is it malformed?", m_configPath);
        }

        bool hasUpgradedConfig = false;
        if (version < (uint32_t)spacecal::config::versioned::DataVersions::Current || version == (uint32_t)spacecal::config::versioned::DataVersions::Legacy) {
            // perform upgrade on config
            hasUpgradedConfig = upgradeConfigToLatest(version, &json);
            if (hasUpgradedConfig) {
                saveConfiguration();
            }
        }

        if (!hasUpgradedConfig) {
            glz::error_ctx deserialiseError = glz::read_file_jsonc<options>(m_config, m_configPath, std::string{});
            if (deserialiseError.ec != glz::error_code::none) {
                // FUCK
                LOG_WARNING("Failed to parse configuration file \"{0}\". {1}", m_configPath, deserialiseError.custom_error_message);
                return ConfigurationError::JsonCorrupt;
            }
        }

        // @FIXME: remove once we support multiple calibrations in the UI
        // ensure at least one calibration exists until we add support in the ui for dynamically making new ones
        if (m_config.calibrations.empty()) {
            LOG_WARN("Config loaded with 0 calibrations. Creating initial calibration entry...");
            m_config.calibrations.emplace_back();
        }

        return ConfigurationError::Ok;
    }

    ConfigurationError ConfigurationManager::saveConfiguration() const {
        constexpr glz::opts options{
            .comments = true,
            .error_on_unknown_keys = false,
            .prettify = true,
            .indentation_char = ' ',
            .indentation_width = 4,
            .error_on_missing_keys = false,
            .error_on_const_read = false,
        };
        glz::error_ctx serialiseError = glz::write_file_json<options>(m_config, m_configPath.c_str(), std::string{});
        if (serialiseError.ec != glz::error_code::none) {
            // FUCK
            LOG_WARNING("Failed to write configuration file \"{0}\". {1}", m_configPath, serialiseError.custom_error_message);
            return ConfigurationError::SerialiseFail;
        }

        LOG_INFO("Configuration file saved to \"{0}\"", m_configPath);

        return ConfigurationError::Ok;
    }

    bool ConfigurationManager::upgradeConfigToLatest(const uint32_t readVersion, glz::generic* jsonData) {
        using namespace spacecal::config::versioned;
        if (!jsonData)
            return false;

        constexpr glz::opts options{
            .comments = true,
            .error_on_unknown_keys = false,
            .error_on_missing_keys = false,
            .error_on_const_read = false
        };

        DataVersions version = (DataVersions)readVersion;

        if (version == DataVersions::Legacy) {
            LOG_INFO("Upgrading legacy (pre 2.0) config...");
        } else {
            LOG_INFO("Upgrading config version {}...", readVersion);
        }

        // upgrade pre 2.0 to nova config
        if (version == DataVersions::Legacy) {
            // we dont know which version of pre-nova spacecal may have been used so we're not going to take any chances here

            // since pre-nova doesn't support multiple playspaces we create an implicit calibration
            m_config.calibrations.resize(1);

            if (jsonData->contains("calibration_speed") && !jsonData->at("calibration_speed").is_null() && jsonData->at("calibration_speed").is_number()) {
                m_config.calibrations[0].calibration_speed = (uint32_t)jsonData->at("calibration_speed").get_number();
                // remap to new range
                switch (m_config.calibrations[0].calibration_speed) {
                    case 0: // FAST
                        m_config.calibrations[0].calibration_speed = (uint32_t)spacecal::CalibrationSpeed::FAST;
                        break;
                    case 1: // SLOW
                        m_config.calibrations[0].calibration_speed = (uint32_t)spacecal::CalibrationSpeed::SLOW;
                        break;
                    case 2: // VERY_SLOW
                        m_config.calibrations[0].calibration_speed = (uint32_t)spacecal::CalibrationSpeed::VERY_SLOW;
                        break;
                }
            }

            // ref device
            if (jsonData->contains("reference_device") && !jsonData->at("reference_device").is_null() && jsonData->at("reference_device").is_object()) {
                const auto& refDeviceJson = jsonData->at("reference_device").get_object();
                if (refDeviceJson.contains("model") && !refDeviceJson.at("model").is_null() && refDeviceJson.at("model").is_string()) {
                    m_config.calibrations[0].reference_device.model = refDeviceJson.at("model").get_string();
                }
                if (refDeviceJson.contains("serial") && !refDeviceJson.at("serial").is_null() && refDeviceJson.at("serial").is_string()) {
                    m_config.calibrations[0].reference_device.serial = refDeviceJson.at("serial").get_string();
                }
                if (refDeviceJson.contains("tracking_system") && !refDeviceJson.at("tracking_system").is_null() && refDeviceJson.at("tracking_system").is_string()) {
                    m_config.calibrations[0].reference_device.tracking_system = refDeviceJson.at("tracking_system").get_string();
                }
            }

            // target device
            if (jsonData->contains("target_device") && !jsonData->at("target_device").is_null() && jsonData->at("target_device").is_object()) {
                const auto& targetDeviceJson = jsonData->at("target_device").get_object();
                if (targetDeviceJson.contains("model") && !targetDeviceJson.at("model").is_null() && targetDeviceJson.at("model").is_string()) {
                    m_config.calibrations[0].target_device.model = targetDeviceJson.at("model").get_string();
                }
                if (targetDeviceJson.contains("serial") && !targetDeviceJson.at("serial").is_null() && targetDeviceJson.at("serial").is_string()) {
                    m_config.calibrations[0].target_device.serial = targetDeviceJson.at("serial").get_string();
                }
                if (targetDeviceJson.contains("tracking_system") && !targetDeviceJson.at("tracking_system").is_null() && targetDeviceJson.at("tracking_system").is_string()) {
                    m_config.calibrations[0].target_device.tracking_system = targetDeviceJson.at("tracking_system").get_string();
                }
            }

            // calibration
            if (jsonData->contains("x") && !jsonData->at("x").is_null() && jsonData->at("x").is_number()) {
                m_config.calibrations[0].calibrated_transform.x = (float) jsonData->at("x").get_number();
            }
            if (jsonData->contains("y") && !jsonData->at("y").is_null() && jsonData->at("y").is_number()) {
                m_config.calibrations[0].calibrated_transform.y = (float)jsonData->at("y").get_number();
            }
            if (jsonData->contains("z") && !jsonData->at("z").is_null() && jsonData->at("z").is_number()) {
                m_config.calibrations[0].calibrated_transform.z = (float)jsonData->at("z").get_number();
            }
            if (jsonData->contains("yaw") && !jsonData->at("yaw").is_null() && jsonData->at("yaw").is_number()) {
                m_config.calibrations[0].calibrated_transform.yaw = (float)jsonData->at("yaw").get_number();
            }
            if (jsonData->contains("pitch") && !jsonData->at("pitch").is_null() && jsonData->at("pitch").is_number()) {
                m_config.calibrations[0].calibrated_transform.pitch = (float)jsonData->at("pitch").get_number();
            }
            if (jsonData->contains("roll") && !jsonData->at("roll").is_null() && jsonData->at("roll").is_number()) {
                m_config.calibrations[0].calibrated_transform.roll = (float)jsonData->at("roll").get_number();
            }

            // continuous calibration
            if (jsonData->contains("autostart_continuous_calibration") && !jsonData->at("autostart_continuous_calibration").is_null() && jsonData->at("autostart_continuous_calibration").is_boolean()) {
                m_config.calibrations[0].continuous.is_active = jsonData->at("autostart_continuous_calibration").get_boolean();
            }
            if (jsonData->contains("quash_target_in_continuous") && !jsonData->at("quash_target_in_continuous").is_null() && jsonData->at("quash_target_in_continuous").is_boolean()) {
                m_config.calibrations[0].continuous.hide_reference_tracker = jsonData->at("quash_target_in_continuous").get_boolean();
            }
            version = DataVersions::_0;
        }

        if (version == DataVersions::_0) {
            // parse into struct and re-map
            Configuration_0 calib = {};
            Configuration_1 nextConfig = {};
            auto ec = glz::read_json<Configuration_0>(calib, *jsonData);
            if (!ec) {
                nextConfig.dataVersion = DataVersions::_1;
                
                nextConfig.advanced_settings = calib.advanced_settings;
                nextConfig.ignore_stage_tracking_warning = calib.ignore_stage_tracking_warning;
                nextConfig.ui_locale = calib.ui_locale;
                
                nextConfig.base_stations.auto_power_management_enabled = calib.base_stations.auto_power_management_enabled;
                nextConfig.base_stations.auto_turn_on_during_startup = calib.base_stations.auto_turn_on_during_startup;
                nextConfig.base_stations.auto_turn_off_during_shutdown = calib.base_stations.auto_turn_off_during_shutdown;
                nextConfig.base_stations.off_should_use_standby = calib.base_stations.off_should_use_standby;
                nextConfig.base_stations.nicknames = std::move(calib.base_stations.nicknames);

                nextConfig.calibrations.resize(calib.calibrations.size());
                for (size_t i = 0; i < calib.calibrations.size(); i++) {
                    nextConfig.calibrations[i].target_device.model = calib.calibrations[i].target_device.model;
                    nextConfig.calibrations[i].target_device.serial = calib.calibrations[i].target_device.serial;
                    nextConfig.calibrations[i].target_device.tracking_system = calib.calibrations[i].target_device.tracking_system;

                    nextConfig.calibrations[i].reference_device.model = calib.calibrations[i].reference_device.model;
                    nextConfig.calibrations[i].reference_device.serial = calib.calibrations[i].reference_device.serial;
                    nextConfig.calibrations[i].reference_device.tracking_system = calib.calibrations[i].reference_device.tracking_system;

                    nextConfig.calibrations[i].is_active = calib.calibrations[i].is_active;
                    nextConfig.calibrations[i].scale = calib.calibrations[i].scale;
                    nextConfig.calibrations[i].anchor_mode = (Configuration_1::AnchorMode) calib.calibrations[i].anchor_mode;
                    nextConfig.calibrations[i].calibrate_motion_vectors = calib.calibrations[i].calibrate_motion_vectors;
                    nextConfig.calibrations[i].attempt_auto_fix_playspace_jumps = calib.calibrations[i].attempt_auto_fix_playspace_jumps;
                    nextConfig.calibrations[i].enforce_minimum_rotation_variance = calib.calibrations[i].enforce_minimum_rotation_variance;
                    nextConfig.calibrations[i].calibration_speed = calib.calibrations[i].calibration_speed;

                    nextConfig.calibrations[i].continuous.is_active = calib.calibrations[i].continuous.is_active;
                    nextConfig.calibrations[i].continuous.hide_reference_tracker = calib.calibrations[i].continuous.hide_reference_tracker;

                    nextConfig.calibrations[i].calibrated_transform.x = calib.calibrations[i].calibrated_transform.x;
                    nextConfig.calibrations[i].calibrated_transform.y = calib.calibrations[i].calibrated_transform.y;
                    nextConfig.calibrations[i].calibrated_transform.z = calib.calibrations[i].calibrated_transform.z;
                    nextConfig.calibrations[i].calibrated_transform.yaw = calib.calibrations[i].calibrated_transform.yaw;
                    nextConfig.calibrations[i].calibrated_transform.pitch = calib.calibrations[i].calibrated_transform.pitch;
                    nextConfig.calibrations[i].calibrated_transform.roll = calib.calibrations[i].calibrated_transform.roll;
                }

                m_config = std::move(nextConfig);
                version = DataVersions::_1;
            }
        }

        if (version == DataVersions::_1) {
            // @TODO: fill on config version bump
        }

        return true;
    }

}