// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "installer.h"

#include "base/strings.h"

#include <sys/stat.h>

namespace dbus = cutefish::base::dbus;
namespace strings = cutefish::base::strings;

namespace installer {

static bool isLowerAlnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

dbus::VariantMap toMap(const Options& options)
{
    dbus::VariantMap map {
            {"disk", dbus::Variant(options.disk)},
            {"fullName", dbus::Variant(options.fullName)},
            {"userName", dbus::Variant(options.userName)},
            {"password", dbus::Variant(options.password)},
            {"hostName", dbus::Variant(options.hostName)},
            {"timeZone", dbus::Variant(options.timeZone)},
    };
    if (!options.partitioning.empty())
        map["partitioning"] = dbus::Variant(options.partitioning);
    return map;
}

std::optional<Options> optionsFromMap(const dbus::VariantMap& map)
{
    Options options;
    for (auto [key, value] : std::initializer_list<std::pair<const char*, std::string*>> {
                 {"disk", &options.disk},
                 {"fullName", &options.fullName},
                 {"userName", &options.userName},
                 {"password", &options.password},
                 {"hostName", &options.hostName},
                 {"timeZone", &options.timeZone},
         }) {
        const auto found = map.find(key);
        if (found == map.end())
            return std::nullopt;
        const std::optional<std::string> text = found->second.get<std::string>();
        if (!text)
            return std::nullopt;
        *value = *text;
    }
    if (const auto found = map.find("partitioning"); found != map.end()) {
        const std::optional<dbus::VariantMap> partitioning = found->second.get<dbus::VariantMap>();
        if (!partitioning)
            return std::nullopt;
        options.partitioning = *partitioning;
    }
    return options;
}

bool isValidUserName(std::string_view name)
{
    if (name.empty() || name.size() > 32)
        return false;
    if (!(name.front() >= 'a' && name.front() <= 'z') && name.front() != '_')
        return false;
    for (const char c : name) {
        if (!isLowerAlnum(c) && c != '_' && c != '-')
            return false;
    }
    return true;
}

bool isValidHostName(std::string_view name)
{
    if (name.empty() || name.size() > 63 || name.front() == '-' || name.back() == '-')
        return false;
    for (const char c : name) {
        if (!isLowerAlnum(strings::toAsciiLower(c)) && c != '-')
            return false;
    }
    return true;
}

bool isValidTimeZone(const std::string& zone, const std::string& zoneinfo)
{
    if (zone.empty() || zone.front() == '/' || zone.find('/') == std::string::npos)
        return false;
    for (const std::string& part : strings::split(zone, '/')) {
        if (part.empty() || part == "." || part == "..")
            return false;
        for (const char c : part) {
            if (!isLowerAlnum(strings::toAsciiLower(c)) && c != '_' && c != '-' && c != '+')
                return false;
        }
    }
    struct stat info {};
    return ::stat((zoneinfo + '/' + zone).c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

std::string suggestedUserName(std::string_view fullName)
{
    const std::vector<std::string> words = strings::splitWhitespace(fullName);
    if (words.empty())
        return {};
    std::string name;
    for (const char c : words.front()) {
        const char lower = strings::toAsciiLower(c);
        if (!isLowerAlnum(lower))
            return {};
        name += lower;
    }
    return isValidUserName(name) ? name : std::string();
}

std::string suggestedHostName(std::string_view userName)
{
    if (userName.empty())
        return {};
    std::string name;
    for (const char c : userName) {
        if (isLowerAlnum(c) || c == '-')
            name += c;
    }
    while (!name.empty() && name.front() == '-')
        name.erase(0, 1);
    if (name.empty())
        return {};
    name = name.substr(0, 60);
    return name + "-pc";
}

} // namespace installer
