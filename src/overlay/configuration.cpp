#include "configuration.h"
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

    void ConfigurationManager::init() {
        if (s_instance != nullptr) {
            LOG_FATAL("Tried creating ConfigurationManager more than once! Breaking singleton. Aborting...");
            return;
        }
        s_instance = this;
        m_configPath = util::getSpaceCalibratorConfigPath().string();

        // Load config from disk. If it does not exist, create a new config
        if (loadConfiguration() != ConfigurationError::Ok) {
            saveConfiguration();
        }
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
        if (version < (uint32_t)spacecal::config::versioned::DataVersions::Current) {
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
            // @TODO: fill on config version bump
        }

        return true;
    }

}