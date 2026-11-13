// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "languages.h"

#include "base/locale.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <unistd.h>

static std::string codeFor(std::string_view session)
{
    return installerLanguages()[installerLanguageFor(session)].code;
}

TEST(Languages, list)
{
    const std::vector<InstallerLanguage>& languages = installerLanguages();
    ASSERT_EQ(languages.size(), 11u);
    EXPECT_EQ(languages.front().code, "en_US");
    EXPECT_EQ(languages.front().locale, "en_US.UTF-8");
    EXPECT_EQ(languages.front().name, "English");

    const auto named = [&languages](const std::string& code) {
        for (const InstallerLanguage& language : languages) {
            if (language.code == code)
                return language;
        }
        return InstallerLanguage();
    };
    EXPECT_EQ(named("es_ES").name, "Espa\u00f1ol");
    EXPECT_EQ(named("pt_BR").name, "Portugu\u00eas (Brasil)");
    EXPECT_EQ(named("fr_FR").name, "Fran\u00e7ais");
    EXPECT_EQ(named("de_DE").name, "Deutsch");
    EXPECT_EQ(named("ru_RU").name, "\u0420\u0443\u0441\u0441\u043a\u0438\u0439");
    EXPECT_EQ(named("ja_JP").name, "\u65e5\u672c\u8a9e");
    EXPECT_EQ(named("ko_KR").name, "\ud55c\uad6d\uc5b4");
    EXPECT_EQ(named("it_IT").name, "Italiano");
    EXPECT_EQ(named("zh_Hans_CN").name, "\u7b80\u4f53\u4e2d\u6587");
    EXPECT_EQ(named("zh_Hans_CN").locale, "zh_CN.UTF-8");
    EXPECT_EQ(named("zh_Hant_TW").name, "\u7e41\u9ad4\u4e2d\u6587");
    EXPECT_EQ(named("zh_Hant_TW").locale, "zh_TW.UTF-8");
}

TEST(Languages, sessionLanguageChoosesARow)
{
    EXPECT_EQ(codeFor("en_US"), "en_US");
    EXPECT_EQ(codeFor("en_SG"), "en_US");
    EXPECT_EQ(codeFor("de_AT"), "de_DE");
    EXPECT_EQ(codeFor("fr_CA"), "fr_FR");
    EXPECT_EQ(codeFor("ja_JP"), "ja_JP");
    EXPECT_EQ(codeFor("pt"), "pt_BR");
    EXPECT_EQ(codeFor("pt_PT"), "pt_BR");
    EXPECT_EQ(codeFor("zh"), "zh_Hans_CN");
    EXPECT_EQ(codeFor("zh_CN"), "zh_Hans_CN");
    EXPECT_EQ(codeFor("zh_SG"), "zh_Hans_CN");
    EXPECT_EQ(codeFor("zh_Hans_CN"), "zh_Hans_CN");
    EXPECT_EQ(codeFor("zh_Hans_HK"), "zh_Hans_CN");
    EXPECT_EQ(codeFor("zh_TW"), "zh_Hant_TW");
    EXPECT_EQ(codeFor("zh_HK"), "zh_Hant_TW");
    EXPECT_EQ(codeFor("zh_MO"), "zh_Hant_TW");
    EXPECT_EQ(codeFor("zh_Hant_HK"), "zh_Hant_TW");
    EXPECT_EQ(codeFor("zh_Hant_TW"), "zh_Hant_TW");
    EXPECT_EQ(codeFor("th_TH"), "en_US");
    EXPECT_EQ(codeFor("sr_RS@latin"), "en_US");
    EXPECT_EQ(codeFor("C"), "en_US");
    EXPECT_EQ(codeFor(""), "en_US");
}

TEST(Languages, sessionLanguage)
{
    const std::filesystem::path directory = std::filesystem::temp_directory_path()
            / ("installer-languages-" + std::to_string(::getpid()));
    std::filesystem::create_directories(directory);
    const std::string config = (directory / "services.conf").string();

    EXPECT_EQ(sessionLanguage(config), cutefish::base::Locale::systemLanguage());
    std::ofstream(config) << "[Language]\nlanguage=zh_Hant_TW\npreferredLanguages=zh_Hant_TW, en_US\n";
    EXPECT_EQ(sessionLanguage(config), "zh_Hant_TW");
    std::filesystem::remove_all(directory);
}
