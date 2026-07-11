#include "localisation.h"

#if OS_WINDOWS
#include "log.h"

#include <unordered_map>
#include <windows.h>

namespace spacecal {

    // Define some stuff so that we can ensure no collisions in our hashmap and O(1)
    // access times.
    struct FNV1aHasher {
        std::size_t operator()(const wchar_t* str) const {
            const std::size_t FNV_offset_basis = 14695981039346656037ULL;
            const std::size_t FNV_prime = 1099511628211ULL;

            std::size_t hash = FNV_offset_basis;

            // Hash each wide character until we hit a null terminator
            for (std::size_t i = 0; str[i] != 0; ++i) {
                hash ^= static_cast<std::size_t>(str[i]); // XOR the current wide char
                hash *= FNV_prime;                        // Multiply by FNV prime
            }

            return hash;
        }
    };
    struct WCharArrayEqual {
        bool operator()(const wchar_t* a, const wchar_t* b) const {
            return std::wcscmp(a, b) == 0;
        }
    };

    // This is a clusterfuck which maps EVERY SINGLE WINDOWS DIALECT to a locale
    // supported by this overlay. See
    // https://winprotocoldoc.z19.web.core.windows.net/MS-LCID/%5bMS-LCID%5d.pdf
    // for more info. We map Windows provided language codes to Locales. Actually
    // its not every dialect supported by windows, just the ones which map to a
    // language the overlay supports. This is to remove a lot of strings from the
    // string table.
    std::unordered_map<const wchar_t*, Locale, FNV1aHasher, WCharArrayEqual>
        s_windowsLangLocaleMapping = {
            {L"bg", Locale::Bulgarian},
            {L"bg-BG", Locale::Bulgarian},

            {L"zh", Locale::Chinese_Simplified},
            {L"zh-CN", Locale::Chinese_Simplified},
            {L"zh-SG", Locale::Chinese_Simplified},
            {L"zh-Hant", Locale::Chinese_Simplified},

            {L"zh-HK", Locale::Chinese_Traditional},
            {L"zh-MO", Locale::Chinese_Traditional},
            {L"zh-TW", Locale::Chinese_Traditional},

            {L"cs", Locale::Czech},
            {L"cs-CZ", Locale::Czech},

            {L"da", Locale::Danish},
            {L"da-DK", Locale::Danish},
            {L"da-GL", Locale::Danish},

            {L"nl", Locale::Dutch},
            {L"nl-AW", Locale::Dutch},
            {L"nl-BE", Locale::Dutch},
            {L"nl-BQ", Locale::Dutch},
            {L"nl-CW", Locale::Dutch},
            {L"nl-NL", Locale::Dutch},
            {L"nl-SX", Locale::Dutch},
            {L"nl-SR", Locale::Dutch},

            {L"en", Locale::English_UK},
            {L"en-AS", Locale::English_UK},
            {L"en-AI", Locale::English_UK},
            {L"en-AG", Locale::English_UK},
            {L"en-AU", Locale::English_UK},
            {L"en-AT", Locale::English_UK},
            {L"en-BS", Locale::English_UK},
            {L"en-BB", Locale::English_UK},
            {L"en-BE", Locale::English_UK},
            {L"en-BZ", Locale::English_UK},
            {L"en-BM", Locale::English_UK},
            {L"en-BW", Locale::English_UK},
            {L"en-IO", Locale::English_UK},
            {L"en-VG", Locale::English_UK},
            {L"en-BI", Locale::English_UK},
            {L"en-CM", Locale::English_UK},
            {L"en-CA", Locale::English_UK},
            {L"en-029", Locale::English_UK},
            {L"en-KY", Locale::English_UK},
            {L"en-CX", Locale::English_UK},
            {L"en-CC", Locale::English_UK},
            {L"en-CK", Locale::English_UK},
            {L"en-CY", Locale::English_UK},
            {L"en-DK", Locale::English_UK},
            {L"en-DM", Locale::English_UK},
            {L"en-ER", Locale::English_UK},
            {L"en-150", Locale::English_UK},
            {L"en-FK", Locale::English_UK},
            {L"en-FI", Locale::English_UK},
            {L"en-FJ", Locale::English_UK},
            {L"en-GM", Locale::English_UK},
            {L"en-DE", Locale::English_UK},
            {L"en-GH", Locale::English_UK},
            {L"en-GI", Locale::English_UK},
            {L"en-GD", Locale::English_UK},
            {L"en-GU", Locale::English_UK},
            {L"en-GG", Locale::English_UK},
            {L"en-GY", Locale::English_UK},
            {L"en-HK", Locale::English_UK},
            {L"en-IN", Locale::English_UK},
            {L"en-IE", Locale::English_UK},
            {L"en-IM", Locale::English_UK},
            {L"en-IL", Locale::English_UK},
            {L"en-JM", Locale::English_UK},
            {L"en-JE", Locale::English_UK},
            {L"en-KE", Locale::English_UK},
            {L"en-KI", Locale::English_UK},
            {L"en-LS", Locale::English_UK},
            {L"en-LR", Locale::English_UK},
            {L"en-MO", Locale::English_UK},
            {L"en-MG", Locale::English_UK},
            {L"en-MW", Locale::English_UK},
            {L"en-MY", Locale::English_UK},
            {L"en-MV", Locale::English_UK},
            {L"en-MT", Locale::English_UK},
            {L"en-MH", Locale::English_UK},
            {L"en-MU", Locale::English_UK},
            {L"en-FM", Locale::English_UK},
            {L"en-MS", Locale::English_UK},
            {L"en-NA", Locale::English_UK},
            {L"en-NR", Locale::English_UK},
            {L"en-NL", Locale::English_UK},
            {L"en-NZ", Locale::English_UK},
            {L"en-NG", Locale::English_UK},
            {L"en-NU", Locale::English_UK},
            {L"en-NF", Locale::English_UK},
            {L"en-MP", Locale::English_UK},
            {L"en-PK", Locale::English_UK},
            {L"en-PW", Locale::English_UK},
            {L"en-PG", Locale::English_UK},
            {L"en-PN", Locale::English_UK},
            {L"en-PR", Locale::English_UK},
            {L"en-PH", Locale::English_UK},
            {L"en-RW", Locale::English_UK},
            {L"en-KN", Locale::English_UK},
            {L"en-LC", Locale::English_UK},
            {L"en-VC", Locale::English_UK},
            {L"en-WS", Locale::English_UK},
            {L"en-SC", Locale::English_UK},
            {L"en-SL", Locale::English_UK},
            {L"en-SG", Locale::English_UK},
            {L"en-SX", Locale::English_UK},
            {L"en-SI", Locale::English_UK},
            {L"en-SB", Locale::English_UK},
            {L"en-ZA", Locale::English_UK},
            {L"en-SS", Locale::English_UK},
            {L"en-SH", Locale::English_UK},
            {L"en-SD", Locale::English_UK},
            {L"en-SZ", Locale::English_UK},
            {L"en-SE", Locale::English_UK},
            {L"en-CH", Locale::English_UK},
            {L"en-TZ", Locale::English_UK},
            {L"en-TK", Locale::English_UK},
            {L"en-TO", Locale::English_UK},
            {L"en-TT", Locale::English_UK},
            {L"en-TC", Locale::English_UK},
            {L"en-TV", Locale::English_UK},
            {L"en-UG", Locale::English_UK},
            {L"en-AE", Locale::English_UK},
            {L"en-GB", Locale::English_UK},
            {L"en-US", Locale::English_US},
            {L"en-UM", Locale::English_UK},
            {L"en-VI", Locale::English_UK},
            {L"en-VU", Locale::English_UK},
            {L"en-001", Locale::English_UK},
            {L"en-ZM", Locale::English_UK},
            {L"en-ZW", Locale::English_UK},

            {L"fi", Locale::Finnish},
            {L"fi-FI", Locale::Finnish},

            {L"fr", Locale::French},
            {L"fr-DZ", Locale::French},
            {L"fr-BE", Locale::French},
            {L"fr-BJ", Locale::French},
            {L"fr-BF", Locale::French},
            {L"fr-BI", Locale::French},
            {L"fr-CM", Locale::French},
            {L"fr-CA", Locale::French},
            {L"fr-029", Locale::French},
            {L"fr-CF", Locale::French},
            {L"fr-TD", Locale::French},
            {L"fr-KM", Locale::French},
            {L"fr-CG", Locale::French},
            {L"fr-CD", Locale::French},
            {L"fr-CI", Locale::French},
            {L"fr-DJ", Locale::French},
            {L"fr-GQ", Locale::French},
            {L"fr-FR", Locale::French},
            {L"fr-GF", Locale::French},
            {L"fr-PF", Locale::French},
            {L"fr-GA", Locale::French},
            {L"fr-GP", Locale::French},
            {L"fr-GN", Locale::French},
            {L"fr-HT", Locale::French},
            {L"fr-LU", Locale::French},
            {L"fr-MG", Locale::French},
            {L"fr-ML", Locale::French},
            {L"fr-MQ", Locale::French},
            {L"fr-MR", Locale::French},
            {L"fr-MU", Locale::French},
            {L"fr-YT", Locale::French},
            {L"fr-MA", Locale::French},
            {L"fr-NC", Locale::French},
            {L"fr-NE", Locale::French},
            {L"fr-MC", Locale::French},
            {L"fr-RE", Locale::French},
            {L"fr-RW", Locale::French},
            {L"fr-BL", Locale::French},
            {L"fr-MF", Locale::French},
            {L"fr-PM", Locale::French},
            {L"fr-SN", Locale::French},
            {L"fr-SC", Locale::French},
            {L"fr-CH", Locale::French},
            {L"fr-SY", Locale::French},
            {L"fr-TG", Locale::French},
            {L"fr-TN", Locale::French},
            {L"fr-VU", Locale::French},
            {L"fr-WF", Locale::French},

            {L"de", Locale::German},
            {L"de-AT", Locale::German},
            {L"de-BE", Locale::German},
            {L"de-DE", Locale::German},
            {L"de-IT", Locale::German},
            {L"de-LI", Locale::German},
            {L"de-LU", Locale::German},
            {L"de-CH", Locale::German},

            {L"el", Locale::Greek},
            {L"el-CY", Locale::Greek},
            {L"el-GR", Locale::Greek},

            {L"hu", Locale::Hungarian},
            {L"hu-HU", Locale::Hungarian},

            {L"it", Locale::Italian},
            {L"it-IT", Locale::Italian},
            {L"it-SM", Locale::Italian},
            {L"it-CH", Locale::Italian},
            {L"it-VA", Locale::Italian},

            {L"ja", Locale::Japanese},
            {L"ja-JP", Locale::Japanese},

            {L"ko", Locale::Korean},
            {L"ko-KR", Locale::Korean},
            {L"ko-KP", Locale::Korean},

            {L"no", Locale::Norwegian},
            {L"nb", Locale::Norwegian},
            {L"nb-NO", Locale::Norwegian},
            {L"nn", Locale::Norwegian},
            {L"nn-NO", Locale::Norwegian},
            {L"nb-SJ", Locale::Norwegian},

            {L"pl", Locale::Polish},
            {L"pl-PL", Locale::Polish},

            {L"pt", Locale::Portuguese_Portugal},
            {L"pt-AO", Locale::Portuguese_Portugal},
            {L"pt-BR", Locale::Portuguese_Brazil},
            {L"pt-CV", Locale::Portuguese_Portugal},
            {L"pt-GQ", Locale::Portuguese_Portugal},
            {L"pt-GW", Locale::Portuguese_Portugal},
            {L"pt-LU", Locale::Portuguese_Portugal},
            {L"pt-MO", Locale::Portuguese_Portugal},
            {L"pt-MZ", Locale::Portuguese_Portugal},
            {L"pt-PT", Locale::Portuguese_Portugal},
            {L"pt-ST", Locale::Portuguese_Portugal},
            {L"pt-CH", Locale::Portuguese_Portugal},
            {L"pt-TL", Locale::Portuguese_Portugal},

            {L"ro", Locale::Romanian},
            {L"ro-MD", Locale::Romanian},
            {L"ro-RO", Locale::Romanian},

            {L"ru", Locale::Russian},
            {L"ru-BY", Locale::Russian},
            {L"ru-KZ", Locale::Russian},
            {L"ru-KG", Locale::Russian},
            {L"ru-MD", Locale::Russian},
            {L"ru-RU", Locale::Russian},
            {L"ru-UA", Locale::Russian},
            {L"cu-RU", Locale::Russian},

            {L"es", Locale::Spanish_Spain},
            {L"es-AR", Locale::Spanish_LatinAmerica},
            {L"es-BZ", Locale::Spanish_LatinAmerica},
            {L"es-VE", Locale::Spanish_LatinAmerica},
            {L"es-BO", Locale::Spanish_LatinAmerica},
            {L"es-BR", Locale::Spanish_LatinAmerica},
            {L"es-CL", Locale::Spanish_LatinAmerica},
            {L"es-CO", Locale::Spanish_LatinAmerica},
            {L"es-CR", Locale::Spanish_LatinAmerica},
            {L"es-CU", Locale::Spanish_LatinAmerica},
            {L"es-DO", Locale::Spanish_LatinAmerica},
            {L"es-EC", Locale::Spanish_LatinAmerica},
            {L"es-SV", Locale::Spanish_LatinAmerica},
            {L"es-GQ", Locale::Spanish_LatinAmerica},
            {L"es-GT", Locale::Spanish_LatinAmerica},
            {L"es-HN", Locale::Spanish_LatinAmerica},
            {L"es-419", Locale::Spanish_LatinAmerica},
            {L"es-MX", Locale::Spanish_LatinAmerica},
            {L"es-NI", Locale::Spanish_LatinAmerica},
            {L"es-PA", Locale::Spanish_LatinAmerica},
            {L"es-PY", Locale::Spanish_LatinAmerica},
            {L"es-PE", Locale::Spanish_LatinAmerica},
            {L"es-PH", Locale::Spanish_LatinAmerica},
            {L"es-PR", Locale::Spanish_LatinAmerica},
            {L"es-ES_tradnl", Locale::Spanish_Spain},
            {L"es-ES", Locale::Spanish_Spain},
            {L"es-US", Locale::Spanish_LatinAmerica},
            {L"es-UY", Locale::Spanish_LatinAmerica},

            {L"sv", Locale::Swedish},
            {L"sv-AX", Locale::Swedish},
            {L"sv-FI", Locale::Swedish},
            {L"sv-SE", Locale::Swedish},

            {L"th", Locale::Thai},
            {L"th-TH", Locale::Thai},

            {L"tr", Locale::Turkish},
            {L"tr-CY", Locale::Turkish},
            {L"tr-TR", Locale::Turkish},

            {L"uk", Locale::Ukrainian},
            {L"uk-UA", Locale::Ukrainian},

            {L"vi", Locale::Vietnamese},
            {L"vi-VN", Locale::Vietnamese},
    };

    Locale LocalisationManager::estimateSystemLocale() const {

        Locale estimatedLocale = Locale::English_UK;

        DWORD numLangs = 0;
        wchar_t langBuffer[512] = {};
        DWORD sizeLangBuffer = sizeof(langBuffer) / sizeof(langBuffer[0]);

        // Ask windows for the user's display languages. These will be sorted by
        // preference
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLangs, langBuffer,
            &sizeLangBuffer)) {
            wchar_t* langViewPtr = langBuffer;

            // We get a null separated string array from winapi. Iterate through it
            for (DWORD i = 0; i < numLangs; i++) {
                size_t langSize = wcslen(langViewPtr);

                // Getting this number took an hour because microsoft thought it would be
                // wise to only distribute the LCID table as a pdf.
                constexpr size_t MAX_WINDOWS_LANG_CODE = 16;
                wchar_t windowsLanguageCode[MAX_WINDOWS_LANG_CODE] = {};

                // Copy current string to windowsLanguageCode, so that we can then look it
                // up in the clusterfuck above
                wmemcpy_s(windowsLanguageCode, MAX_WINDOWS_LANG_CODE, langViewPtr,
                    langSize);

                auto it = s_windowsLangLocaleMapping.find(windowsLanguageCode);
                if (it != s_windowsLangLocaleMapping.end()) {

                    // Found locale! Use it
                    return it->second;
                }

                // Advance by one element
                langViewPtr += langSize + 1;
            }
        }

        return estimatedLocale;
    }
} // namespace localisation
#endif // OS_WINDOWS