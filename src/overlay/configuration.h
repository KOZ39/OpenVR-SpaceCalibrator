#pragma once

#include <string>
#include <vector>

#include "config/configuration_data_versions.h"

namespace spacecal {

    enum class ConfigurationError {
        Ok,
        FileNotExist,
        JsonCorrupt,
        SerialiseFail,
        Count,
    };

    typedef spacecal::config::versioned::Configuration_Latest Configuration;

    class ConfigurationManager {
    public:
        void init();

        ConfigurationError loadConfiguration();
        ConfigurationError saveConfiguration() const;

        std::string inline getConfigurationPath() const { return m_configPath; }

        [[nodiscard]] static inline ConfigurationManager* getInstance() { return s_instance; }
        [[nodiscard]] inline Configuration* getConfiguration() { return &m_config; }

    private:
        bool upgradeConfigToLatest(const uint32_t readVersion, const std::string& configPath);

    private:
        std::string m_configPath;
        static ConfigurationManager* s_instance;

        Configuration m_config;
    };
} // namespace spacecal