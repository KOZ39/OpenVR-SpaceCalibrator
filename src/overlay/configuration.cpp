#include "configuration.h"
#include "log.h"
#include "util.h"
#include "platform.h"

#include <filesystem>
#include <glaze/glaze.hpp>

namespace spacecal {

    ConfigurationManager* ConfigurationManager::s_instance = nullptr;

#if OS_WINDOWS
    std::string getLegacySettingsString() {
        // Check registry for legacy config
        // HKEY_CURRENT_USER\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator
        constexpr const char* RegistryKey = "Software\\OpenVR-SpaceCalibrator";

        // @TODO: Read registry key
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

        std::string jsonConfigRaw;
        if (!std::filesystem::is_regular_file(m_configPath)) {

#if OS_WINDOWS
            jsonConfigRaw = getLegacySettingsString();
            if (jsonConfigRaw.empty()) {
                LOG_WARNING("Failed to locate configuration file \"{0}\" on disk, and legacy registry key was absent. Using default settings...", m_configPath);
                return ConfigurationError::FileNotExist;
            }
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

        uint32_t version = (uint32_t)spacecal::config::versioned::DataVersions::Current;
        if (!json.at("dataVersion").is_null() && json.at("dataVersion").is_number()) {
            version = (uint32_t)json.at("dataVersion").get_number();
            // Legacy is the u32 max, so treat it as a special case here
            if (version == (uint32_t)spacecal::config::versioned::DataVersions::Legacy) {
                LOG_INFO("Config version :: Legacy");
            } else {
                LOG_INFO("Config version :: {}", version);
            }

            if (version < (uint32_t)spacecal::config::versioned::DataVersions::Current) {
                // perform upgrade on config
                upgradeConfigToLatest(version, m_configPath);
            }
        } else {
            // @TOOD: Verify old config
        }

        glz::error_ctx deserialiseError = glz::read_file_json<options>(m_config, m_configPath, std::string{});
        if (deserialiseError.ec != glz::error_code::none) {
            // FUCK
            LOG_WARNING("Failed to parse configuration file \"{0}\". {1}", m_configPath, deserialiseError.custom_error_message);
            return ConfigurationError::JsonCorrupt;
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

        return ConfigurationError::Ok;
    }

    bool ConfigurationManager::upgradeConfigToLatest(const uint32_t readVersion, const std::string& configPath) {
        return false;
    }

}