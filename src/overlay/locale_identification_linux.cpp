#include "localisation.h"

#if OS_LINUX

#include "log.h"

#include <unordered_map>
#include <locale.h>

namespace spacecal {
    // This is a clusterfuck which maps EVERY SINGLE LINUX DIALECT to a locale
    // supported by this overlay. See /usr/share/i18n/SUPPORTED and /usr/share/locale.
    // Actually its not every dialect supported by Linux, just the ones which map to a
    // language the overlay supports. This is to remove a lot of strings from the
    // string table.
    // @NOTE: most of these are just the windows table with - swapped to _, as the windows one is more dialect heavy lol
    std::unordered_map<const char*, Locale>
        s_linuxLangLocaleMapping = {
            {"bg", Locale::Bulgarian},
            {"bg_BG", Locale::Bulgarian},

            {"zh", Locale::Chinese_Simplified},
            {"zh_CN", Locale::Chinese_Simplified},
            {"zh_SG", Locale::Chinese_Simplified},
            {"zh_Hant", Locale::Chinese_Simplified},

            {"zh_HK", Locale::Chinese_Traditional},
            {"zh_MO", Locale::Chinese_Traditional},
            {"zh_TW", Locale::Chinese_Traditional},
            {"cu_RU", Locale::Chinese_Traditional},

            {"cs", Locale::Czech},
            {"cs_CZ", Locale::Czech},

            {"da", Locale::Danish},
            {"da_DK", Locale::Danish},
            {"da_GL", Locale::Danish},

            {"nl", Locale::Dutch},
            {"nl_AW", Locale::Dutch},
            {"nl_BE", Locale::Dutch},
            {"nl_BQ", Locale::Dutch},
            {"nl_CW", Locale::Dutch},
            {"nl_NL", Locale::Dutch},
            {"nl_SX", Locale::Dutch},
            {"nl_SR", Locale::Dutch},

            {"en", Locale::English_UK},
            {"en_AS", Locale::English_UK},
            {"en_AI", Locale::English_UK},
            {"en_AG", Locale::English_UK},
            {"en_AU", Locale::English_UK},
            {"en_AT", Locale::English_UK},
            {"en_BS", Locale::English_UK},
            {"en_BB", Locale::English_UK},
            {"en_BE", Locale::English_UK},
            {"en_BZ", Locale::English_UK},
            {"en_BM", Locale::English_UK},
            {"en_BW", Locale::English_UK},
            {"en_IO", Locale::English_UK},
            {"en_VG", Locale::English_UK},
            {"en_BI", Locale::English_UK},
            {"en_CM", Locale::English_UK},
            {"en_CA", Locale::English_UK},
            {"en_029", Locale::English_UK},
            {"en_KY", Locale::English_UK},
            {"en_CX", Locale::English_UK},
            {"en_CC", Locale::English_UK},
            {"en_CK", Locale::English_UK},
            {"en_CY", Locale::English_UK},
            {"en_DK", Locale::English_UK},
            {"en_DM", Locale::English_UK},
            {"en_ER", Locale::English_UK},
            {"en_150", Locale::English_UK},
            {"en_FK", Locale::English_UK},
            {"en_FI", Locale::English_UK},
            {"en_FJ", Locale::English_UK},
            {"en_GM", Locale::English_UK},
            {"en_DE", Locale::English_UK},
            {"en_GH", Locale::English_UK},
            {"en_GI", Locale::English_UK},
            {"en_GD", Locale::English_UK},
            {"en_GU", Locale::English_UK},
            {"en_GG", Locale::English_UK},
            {"en_GY", Locale::English_UK},
            {"en_HK", Locale::English_UK},
            {"en_IN", Locale::English_UK},
            {"en_IE", Locale::English_UK},
            {"en_IM", Locale::English_UK},
            {"en_IL", Locale::English_UK},
            {"en_JM", Locale::English_UK},
            {"en_JE", Locale::English_UK},
            {"en_KE", Locale::English_UK},
            {"en_KI", Locale::English_UK},
            {"en_LS", Locale::English_UK},
            {"en_LR", Locale::English_UK},
            {"en_MO", Locale::English_UK},
            {"en_MG", Locale::English_UK},
            {"en_MW", Locale::English_UK},
            {"en_MY", Locale::English_UK},
            {"en_MV", Locale::English_UK},
            {"en_MT", Locale::English_UK},
            {"en_MH", Locale::English_UK},
            {"en_MU", Locale::English_UK},
            {"en_FM", Locale::English_UK},
            {"en_MS", Locale::English_UK},
            {"en_NA", Locale::English_UK},
            {"en_NR", Locale::English_UK},
            {"en_NL", Locale::English_UK},
            {"en_NZ", Locale::English_UK},
            {"en_NG", Locale::English_UK},
            {"en_NU", Locale::English_UK},
            {"en_NF", Locale::English_UK},
            {"en_MP", Locale::English_UK},
            {"en_PK", Locale::English_UK},
            {"en_PW", Locale::English_UK},
            {"en_PG", Locale::English_UK},
            {"en_PN", Locale::English_UK},
            {"en_PR", Locale::English_UK},
            {"en_PH", Locale::English_UK},
            {"en_RW", Locale::English_UK},
            {"en_KN", Locale::English_UK},
            {"en_LC", Locale::English_UK},
            {"en_VC", Locale::English_UK},
            {"en_WS", Locale::English_UK},
            {"en_SC", Locale::English_UK},
            {"en_SL", Locale::English_UK},
            {"en_SG", Locale::English_UK},
            {"en_SX", Locale::English_UK},
            {"en_SI", Locale::English_UK},
            {"en_SB", Locale::English_UK},
            {"en_ZA", Locale::English_UK},
            {"en_SS", Locale::English_UK},
            {"en_SH", Locale::English_UK},
            {"en_SD", Locale::English_UK},
            {"en_SZ", Locale::English_UK},
            {"en_SE", Locale::English_UK},
            {"en_CH", Locale::English_UK},
            {"en_TZ", Locale::English_UK},
            {"en_TK", Locale::English_UK},
            {"en_TO", Locale::English_UK},
            {"en_TT", Locale::English_UK},
            {"en_TC", Locale::English_UK},
            {"en_TV", Locale::English_UK},
            {"en_UG", Locale::English_UK},
            {"en_AE", Locale::English_UK},
            {"en_GB", Locale::English_UK},
            {"en_US", Locale::English_US},
            {"en_UM", Locale::English_UK},
            {"en_VI", Locale::English_UK},
            {"en_VU", Locale::English_UK},
            {"en_001", Locale::English_UK},
            {"en_ZM", Locale::English_UK},
            {"en_ZW", Locale::English_UK},

            {"fi", Locale::Finnish},
            {"fi_FI", Locale::Finnish},

            {"fr", Locale::French},
            {"fr_DZ", Locale::French},
            {"fr_BE", Locale::French},
            {"fr_BJ", Locale::French},
            {"fr_BF", Locale::French},
            {"fr_BI", Locale::French},
            {"fr_CM", Locale::French},
            {"fr_CA", Locale::French},
            {"fr_029", Locale::French},
            {"fr_CF", Locale::French},
            {"fr_TD", Locale::French},
            {"fr_KM", Locale::French},
            {"fr_CG", Locale::French},
            {"fr_CD", Locale::French},
            {"fr_CI", Locale::French},
            {"fr_DJ", Locale::French},
            {"fr_GQ", Locale::French},
            {"fr_FR", Locale::French},
            {"fr_GF", Locale::French},
            {"fr_PF", Locale::French},
            {"fr_GA", Locale::French},
            {"fr_GP", Locale::French},
            {"fr_GN", Locale::French},
            {"fr_HT", Locale::French},
            {"fr_LU", Locale::French},
            {"fr_MG", Locale::French},
            {"fr_ML", Locale::French},
            {"fr_MQ", Locale::French},
            {"fr_MR", Locale::French},
            {"fr_MU", Locale::French},
            {"fr_YT", Locale::French},
            {"fr_MA", Locale::French},
            {"fr_NC", Locale::French},
            {"fr_NE", Locale::French},
            {"fr_MC", Locale::French},
            {"fr_RE", Locale::French},
            {"fr_RW", Locale::French},
            {"fr_BL", Locale::French},
            {"fr_MF", Locale::French},
            {"fr_PM", Locale::French},
            {"fr_SN", Locale::French},
            {"fr_SC", Locale::French},
            {"fr_CH", Locale::French},
            {"fr_SY", Locale::French},
            {"fr_TG", Locale::French},
            {"fr_TN", Locale::French},
            {"fr_VU", Locale::French},
            {"fr_WF", Locale::French},

            {"de", Locale::German},
            {"de_AT", Locale::German},
            {"de_BE", Locale::German},
            {"de_DE", Locale::German},
            {"de_IT", Locale::German},
            {"de_LI", Locale::German},
            {"de_LU", Locale::German},
            {"de_CH", Locale::German},

            {"el", Locale::Greek},
            {"el_CY", Locale::Greek},
            {"el_GR", Locale::Greek},

            {"hu", Locale::Hungarian},
            {"hu_HU", Locale::Hungarian},

            {"it", Locale::Italian},
            {"it_IT", Locale::Italian},
            {"it_SM", Locale::Italian},
            {"it_CH", Locale::Italian},
            {"it_VA", Locale::Italian},

            {"ja", Locale::Japanese},
            {"ja_JP", Locale::Japanese},

            {"ko", Locale::Korean},
            {"ko_KR", Locale::Korean},
            {"ko_KP", Locale::Korean},

            {"no", Locale::Norwegian},
            {"no_NO", Locale::Norwegian},
            {"nb", Locale::Norwegian},
            {"nb_NO", Locale::Norwegian},
            {"nn", Locale::Norwegian},
            {"nn_NO", Locale::Norwegian},
            {"nb_SJ", Locale::Norwegian},

            {"pl", Locale::Polish},
            {"pl_PL", Locale::Polish},

            {"pt", Locale::Portuguese_Portugal},
            {"pt_AO", Locale::Portuguese_Portugal},
            {"pt_BR", Locale::Portuguese_Brazil},
            {"pt_CV", Locale::Portuguese_Portugal},
            {"pt_GQ", Locale::Portuguese_Portugal},
            {"pt_GW", Locale::Portuguese_Portugal},
            {"pt_LU", Locale::Portuguese_Portugal},
            {"pt_MO", Locale::Portuguese_Portugal},
            {"pt_MZ", Locale::Portuguese_Portugal},
            {"pt_PT", Locale::Portuguese_Portugal},
            {"pt_ST", Locale::Portuguese_Portugal},
            {"pt_CH", Locale::Portuguese_Portugal},
            {"pt_TL", Locale::Portuguese_Portugal},

            {"ro", Locale::Romanian},
            {"ro_MD", Locale::Romanian},
            {"ro_RO", Locale::Romanian},

            {"ru", Locale::Russian},
            {"ru_BY", Locale::Russian},
            {"ru_KZ", Locale::Russian},
            {"ru_KG", Locale::Russian},
            {"ru_MD", Locale::Russian},
            {"ru_RU", Locale::Russian},
            {"ru_UA", Locale::Russian},

            {"es", Locale::Spanish_Spain},
            {"es_AR", Locale::Spanish_LatinAmerica},
            {"es_BZ", Locale::Spanish_LatinAmerica},
            {"es_VE", Locale::Spanish_LatinAmerica},
            {"es_BO", Locale::Spanish_LatinAmerica},
            {"es_BR", Locale::Spanish_LatinAmerica},
            {"es_CL", Locale::Spanish_LatinAmerica},
            {"es_CO", Locale::Spanish_LatinAmerica},
            {"es_CR", Locale::Spanish_LatinAmerica},
            {"es_CU", Locale::Spanish_LatinAmerica},
            {"es_DO", Locale::Spanish_LatinAmerica},
            {"es_EC", Locale::Spanish_LatinAmerica},
            {"es_SV", Locale::Spanish_LatinAmerica},
            {"es_GQ", Locale::Spanish_LatinAmerica},
            {"es_GT", Locale::Spanish_LatinAmerica},
            {"es_HN", Locale::Spanish_LatinAmerica},
            {"es_419", Locale::Spanish_LatinAmerica},
            {"es_MX", Locale::Spanish_LatinAmerica},
            {"es_NI", Locale::Spanish_LatinAmerica},
            {"es_PA", Locale::Spanish_LatinAmerica},
            {"es_PY", Locale::Spanish_LatinAmerica},
            {"es_PE", Locale::Spanish_LatinAmerica},
            {"es_PH", Locale::Spanish_LatinAmerica},
            {"es_PR", Locale::Spanish_LatinAmerica},
            {"es_ES_tradnl", Locale::Spanish_Spain},
            {"es_ES", Locale::Spanish_Spain},
            {"es_US", Locale::Spanish_LatinAmerica},
            {"es_UY", Locale::Spanish_LatinAmerica},

            // catalan is weird so alias it
            {"ca", Locale::Spanish_Spain },
            {"ca-AD", Locale::Spanish_Spain },
            {"ca-ES", Locale::Spanish_Spain },
            {"ca-FR", Locale::French },
            {"ca-IT", Locale::Italian },

            {"sv", Locale::Swedish},
            {"sv_AX", Locale::Swedish},
            {"sv_FI", Locale::Swedish},
            {"sv_SE", Locale::Swedish},

            {"th", Locale::Thai},
            {"th_TH", Locale::Thai},

            {"tr", Locale::Turkish},
            {"tr_CY", Locale::Turkish},
            {"tr_TR", Locale::Turkish},

            {"uk", Locale::Ukrainian},
            {"uk_UA", Locale::Ukrainian},

            {"vi", Locale::Vietnamese},
            {"vi_VN", Locale::Vietnamese},
    };

    Locale LocalisationManager::estimateSystemLocale() const {

        Locale estimatedLocale = Locale::English_UK;

        char* szSystemLocale = setlocale(LC_MESSAGES, NULL);
        if (szSystemLocale == nullptr || szSystemLocale[0] == '\0') {
            szSystemLocale = setlocale(LC_ALL, NULL);
        }
        if (szSystemLocale == nullptr || szSystemLocale[0] == '\0') {
            return estimatedLocale; // idk what the OS locale is fuck it heres english
        }

        // trim the first . -> we dont care about encoding cuz this app is entirely UTF8, so we just care about what strings to show to the user :D
        std::string szTrimmedLocale = std::string(szSystemLocale);
        size_t dotPos = szTrimmedLocale.find('.');
        if (dotPos != std::string::npos) {
            szTrimmedLocale = szTrimmedLocale.substr(0, dotPos);
        }

        auto it = s_linuxLangLocaleMapping.find(szTrimmedLocale.c_str());
        if (it != s_linuxLangLocaleMapping.end()) {
            // Found locale! Use it
            return it->second;
        }

        return estimatedLocale;
    }
} // namespace localisation
#endif // OS_LINUX