#include "localisation.h"
#include "log.h"
#include "platform.h"
#include "util.h"

#include <filesystem>
#include <glaze/glaze.hpp>

namespace spacecal {
    LocalisationManager* LocalisationManager::m_instance = nullptr;

    void LocalisationManager::init()
    {

        if (m_instance != nullptr) {
            LOG_FATAL("Tried creating LocalisationManager more than once! Breaking singleton. Aborting...");
            return;
        }
        m_instance = this;

        m_selectedLocale = estimateSystemLocale();

        // @TODO: Check if it's defined in settings, if so, estimate, else, lmao

        loadLocalisationStrings(m_selectedLocale);
    }

    std::string LocalisationManager::getString(const std::string& input) const
    {

        if (m_localisedStrings.contains(input)) {
            return m_localisedStrings.at(input);
        }

        LOG_WARN("String \"{0}\" was not defined in a localisation file. Falling back to input key.", input.c_str());

        return input;
    }

    bool LocalisationManager::loadLocalisationStrings(const Locale locale)
    {

        // To load locale strings we first:
        //   - Unload all existing locales
        //   - Load the en_GB locale
        //   - If the locale is not en_GB, load the desired locale, replacing all missing entries with the new data

        m_localisedStrings.clear();

        bool result = loadLocaleFromFile(Locale::English_UK);
        if (locale != Locale::English_UK) {
            result |= loadLocaleFromFile(locale);
        }

        return result;
    }

    bool LocalisationManager::loadLocaleFromFile(const Locale locale)
    {
        auto rootDir = util::getSpaceCalibratorInstallDir();
        std::string langPath = (rootDir / (getLocaleAsRegionString(locale) + ".locale")).string();

        FILE* localeFile = nullptr;
        errno_t fileErr = fopen_s(&localeFile, langPath.c_str(), "rb");
        switch (fileErr) {
        case 0: // OK
            break;
        case EACCES:
            LOG_WARNING("The locale file \"{0}\" could not be loaded due to misconfigured permissions.", langPath);
            return false;
        case EMFILE:
        case ENFILE:
            LOG_WARNING("The locale file \"{0}\" could not be loaded as too many files are open on the system.", langPath);
            return false;
        case EFBIG:
            LOG_WARNING("The locale file \"{0}\" could not be loaded as it was too large.", langPath);
            return false;
        default:
            LOG_WARNING("The locale file \"{0}\" could not be loaded due to an unknown error ({1}).", langPath, fileErr);
            break;
        }
        if (localeFile) {

            // Read to std::string
            fseek(localeFile, 0, SEEK_END);
            size_t localeFileSize = ftell(localeFile);
            std::string buffer;
            buffer.resize(localeFileSize);
            rewind(localeFile);
            fread(&buffer[0], 1, localeFileSize, localeFile);
            fclose(localeFile);
            localeFile = nullptr;

            glz::json_t json{};
            glz::error_ctx jsonError = glz::read_json(json, buffer);
            if (jsonError.ec != glz::error_code::none) {
                LOG_WARNING("Failed to parse JSON for locale file \"{0}\". {1}.", langPath, jsonError.custom_error_message);
                return false;
            }
            if (json.empty()) {
                LOG_WARNING("Failed to parse JSON for locale file \"{0}\". Expected JSON object, got empty file.", langPath);
                return false;
            }
            if (!json.is_object()) {
                LOG_WARNING("Failed to parse JSON for locale file \"{0}\". Expected JSON object, got another type.", langPath);
                return false;
            }

            // JSON is in expected format, awesome!
            // Therefore, we should iterate through it and begin loading the key value pairs into memory
            glz::json_t::object_t jsonObject = json.as<glz::json_t::object_t>();
            for (std::pair<const std::string, glz::json_t> elem : jsonObject) {
                if (elem.first.empty()) {
                    LOG_WARNING("Failed to parse JSON for locale file \"{0}\". Expected JSON key-value pair of type string, got invalid key.", langPath);
                    continue;
                }
                if (!elem.second.is_string()) {
                    LOG_WARNING("Failed to parse JSON for locale file \"{0}\". Expected JSON key-value pair of type string, got another value type.", langPath);
                    continue;
                }
                m_localisedStrings[elem.first] = elem.second.get_string();
            }

            return true;
        } else {
            LOG_WARNING("The locale file \"{0}\" could not be found on the disk.", langPath);
            return false;
        }

        return false;
    }

    std::string LocalisationManager::getLocaleAsRegionString(const Locale locale) const
    {

        switch (locale) {
        case Locale::English_UK:
            return "en_GB";
        case Locale::English_US:
            return "en_US";
        case Locale::French:
            return "fr";
        case Locale::Italian:
            return "it";
        case Locale::German:
            return "de";
        case Locale::Dutch:
            return "nl";
        case Locale::Spanish_Spain:
            return "es_ES";
        case Locale::Spanish_LatinAmerica:
            return "es_US";
        case Locale::Danish:
            return "da";
        case Locale::Swedish:
            return "se";
        case Locale::Finnish:
            return "fi";
        case Locale::Norwegian:
            return "no";
        case Locale::Bulgarian:
            return "bg";
        case Locale::Polish:
            return "pl";
        case Locale::Czech:
            return "cs";
        case Locale::Greek:
            return "el";
        case Locale::Hungarian:
            return "hu";
        case Locale::Portuguese_Portugal:
            return "pt_PT";
        case Locale::Portuguese_Brazil:
            return "pt_BR";
        case Locale::Romanian:
            return "ro";
        case Locale::Russian:
            return "ru";
        case Locale::Turkish:
            return "tr";
        case Locale::Ukrainian:
            return "uk";
        case Locale::Chinese_Simplified:
            return "zh_HANS";
        case Locale::Chinese_Traditional:
            return "zh_HANT";
        case Locale::Japanese:
            return "ja";
        case Locale::Korean:
            return "ko";
        case Locale::Thai:
            return "th";
        case Locale::Vietnamese:
            return "vi";

#if _DEBUG
        case Locale::System:
            return "ss_ss";
        case Locale::Count:
            return "cc_CC";
#else
        case Locale::System:
        case Locale::Count:
            return "en_GB";
#endif
        }

#if _DEBUG
        // Return an invalid code in debug mode to signify that I fucked up
        return "xx_XX";
#else
        return "en_GB";
#endif
    }
}