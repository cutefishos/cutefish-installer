// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "languages.h"

#include "base/locale.h"
#include "base/paths.h"
#include "base/settings.h"

#include <unicode/locdspnm.h>
#include <unicode/locid.h>

#include <memory>

// The name a language gives itself, as a list shows it: "Español",
// "Português (Brasil)", Chinese by its script.
static std::string ownName(const char* id)
{
    const icu::Locale locale(id);
    UDisplayContext contexts[] = {UDISPCTX_DIALECT_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU};
    const std::unique_ptr<icu::LocaleDisplayNames> names(
            icu::LocaleDisplayNames::createInstance(locale, contexts, std::size(contexts)));
    icu::UnicodeString name;
    if (names)
        names->localeDisplayName(locale, name);
    std::string utf8;
    name.toUTF8String(utf8);
    return utf8.empty() ? std::string(id) : utf8;
}

const std::vector<InstallerLanguage>& installerLanguages()
{
    static const std::vector<InstallerLanguage> languages = [] {
        static const struct
        {
            const char* code;
            const char* locale;
            // What ownName() reads the name for.
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
                {"zh_Hans_CN", "zh_CN.UTF-8", "zh_Hans"},
                {"zh_Hant_TW", "zh_TW.UTF-8", "zh_Hant"},
        };
        std::vector<InstallerLanguage> list;
        for (const auto& language : kLanguages)
            list.push_back({language.code, language.locale, ownName(language.nameId)});
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
