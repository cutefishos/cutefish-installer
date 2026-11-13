// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>
#include <string_view>
#include <vector>

// A language the installer speaks, which the installed system starts in.
struct InstallerLanguage
{
    // As the session's language settings name it, such as "zh_Hans_CN".
    std::string code;
    // The locale the system is set to, such as "zh_CN.UTF-8".
    std::string locale;
    // In the language itself, such as "Deutsch".
    std::string name;
};

// English first, then the languages the installer is translated to.
const std::vector<InstallerLanguage>& installerLanguages();

// The index in installerLanguages() for a language code such as "de_AT",
// "zh_HK" or "zh_Hant_TW": the same language, Brazilian Portuguese for any
// Portuguese, Traditional Chinese for Taiwan, Hong Kong and Macau; English
// for any other.
size_t installerLanguageFor(std::string_view code);

// The language the session runs in: the first of cutefish-services'
// languages in the settings file, else the system's.
std::string sessionLanguage(const std::string& servicesConfig);
std::string sessionLanguage();
