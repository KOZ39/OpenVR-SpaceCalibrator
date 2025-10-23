#pragma once

#include <fmt/args.h>
#include <string>
#include <unordered_map>

namespace spacecal {

    enum class Locale {
        English_UK,
        English_US,
        French,
        Italian,
        German,
        Dutch,
        Spanish_Spain,
        Spanish_LatinAmerica,
        Danish,
        Swedish,
        Finnish,
        Norwegian,
        Bulgarian,
        Polish,
        Czech,
        Greek,
        Hungarian,
        Portuguese_Portugal,
        Portuguese_Brazil,
        Romanian,
        Russian,
        Turkish,
        Ukrainian,

        // Asian languages
        Chinese_Simplified,
        Chinese_Traditional,
        Japanese,
        Korean,
        Thai,
        Vietnamese,

        // Use the OS defined locale
        System,

        Count,
    };

    class LocalisationManager {

    public:
        static inline LocalisationManager* GetInstance() { return m_instance; }

        void Init();
        std::string GetString(const std::string& input) const;
        bool LoadLocalisationStrings(const Locale locale);
        Locale EstimateSystemLocale() const;

        /// <summary>
        /// Converts a locale into a locale region in the form of en_US, en_GB, pl, etc. Used to identify which locale files to load.
        /// </summary>
        /// <param name="locale">The locale to convert</param>
        /// <returns>A string representing the unique locale code for the given Locale</returns>
        std::string GetLocaleAsRegionString(const Locale locale) const;

    private:
        bool LoadLocaleFromFile(const Locale locale);

    private:
        static LocalisationManager* m_instance;
        Locale m_selectedLocale = Locale::English_UK;

        std::unordered_map<std::string, std::string> m_localisedStrings;
    };
}

#define LOCALE_GET(key) spacecal::LocalisationManager::GetInstance()->GetString(key)
#define LOCALE_FORMAT(key, ...) fmt::vformat(spacecal::LocalisationManager::GetInstance()->GetString(key), fmt::make_format_args(__VA_ARGS__))