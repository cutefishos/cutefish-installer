// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "languages.h"

#include "base/locale.h"
#include "base/paths.h"
#include "base/settings.h"

#include <unicode/locid.h>

const std::vector<InstallerLanguage>& installerLanguages()
{
    static const std::vector<InstallerLanguage> languages = [] {
        static const struct
        {
            const char* code;
            const char* locale;
            // The id whose dialect name the list shows: "Español", "简体中文".
            const char* nameId;
        } kLanguages[] = {
                {"en_US", "en_US.UTF-8", "en"},
                {"es_ES", "es_ES.UTF-8", "es"},
                {"pt_BR", "pt_BR.UTF-8", "pt_BR"},
                {"fr_FR", "fr_FR.UTF-8", "fr"},
                {"de_DE", "de_DE.UTF-8", "de"},
                {"ru_RU", "ru_RU.UTF-8", "ru"},
                {"ja_JP", "ja_JP.UTF-8", "ja"},
                {"ko_KR", "ko_KR.UTF-8", "ko"},
                {"it_IT", "it_IT.UTF-8", "it"},
                {"pl_PL", "pl_PL.UTF-8", "pl"},
                {"cs_CZ", "cs_CZ.UTF-8", "cs"},
                {"id_ID", "id_ID.UTF-8", "id"},
                {"ms_MY", "ms_MY.UTF-8", "ms"},
                {"vi_VN", "vi_VN.UTF-8", "vi"},
                {"zh_Hans_CN", "zh_CN.UTF-8", "zh_Hans"},
                {"zh_Hant_TW", "zh_TW.UTF-8", "zh_Hant"},
        };
        using cutefish::base::Locale;
        std::vector<InstallerLanguage> list;
        for (const auto& language : kLanguages) {
            list.push_back({language.code, language.locale,
                            Locale::languageName(language.nameId, language.nameId, Locale::LanguageNames::Dialect)});
        }
        return list;
    }();
    return languages;
}

size_t installerLanguageFor(std::string_view code)
{
    const icu::Locale locale(std::string(code).c_str());
    const std::string language = locale.getLanguage();
    const std::string script = locale.getScript();
    const std::string country = locale.getCountry();

    std::string wanted = language;
    if (language == "pt") {
        wanted = "pt_BR";
    } else if (language == "zh") {
        const bool traditional = script == "Hant"
                || (script.empty() && (country == "TW" || country == "HK" || country == "MO"));
        wanted = traditional ? "zh_Hant_TW" : "zh_Hans_CN";
    }

    const std::vector<InstallerLanguage>& languages = installerLanguages();
    for (size_t i = 0; i < languages.size(); ++i) {
        const std::string& candidate = languages[i].code;
        if (candidate == wanted || candidate.substr(0, candidate.find('_')) == wanted)
            return i;
    }
    return 0;
}

std::string sessionLanguage(const std::string& servicesConfig)
{
    const cutefish::base::Settings settings(servicesConfig);
    const std::string code = settings.stringValue("Language/language");
    return code.empty() ? cutefish::base::Locale::systemLanguage() : code;
}

std::string sessionLanguage()
{
    return sessionLanguage(cutefish::base::paths::cutefishConfig("services"));
}
