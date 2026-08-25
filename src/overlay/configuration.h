#pragma once

#include <string>
#include <vector>

#include "config/configuration_data_versions.h"

namespace glz {
struct generic;
}

namespace spacecal {

enum class ConfigurationError {
    Ok,
    FileNotExist,
    JsonCorrupt,
    SerialiseFail,
    SingletonExists,
    Count,
};

typedef spacecal::config::versioned::Configuration_Latest Configuration;

class ConfigurationManager {
public:
    ConfigurationError init();

    ConfigurationError loadConfiguration();
    ConfigurationError saveConfiguration() const;

    void resetConfiguration();

    std::string inline getConfigurationPath() const { return m_configPath; }

    [[nodiscard]] static inline ConfigurationManager* getInstance() { return s_instance; }
    [[nodiscard]] inline Configuration* getConfiguration() { return &m_config; }

private:
    bool upgradeConfigToLatest(const uint32_t readVersion, glz::generic* configPath);

private:
    std::string m_configPath;
    static ConfigurationManager* s_instance;

    Configuration m_config;
};
} // namespace spacecal