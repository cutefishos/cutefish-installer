// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timezones.h"

#include "base/file.h"
#include "base/locale.h"
#include "base/strings.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <map>

namespace strings = cutefish::base::strings;

static std::string offsetText(const std::string& id)
{
    int seconds = 0;
    try {
        const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        seconds = int(std::chrono::locate_zone(id)->get_info(now).offset.count());
    } catch (const std::runtime_error&) {
        return {};
    }
    const int minutes = std::abs(seconds) / 60;
    char text[16];
    std::snprintf(text, sizeof text, "UTC%c%02d:%02d", seconds < 0 ? '-' : '+', minutes / 60, minutes % 60);
    return text;
}

static std::string cityOf(const std::string& id)
{
    return strings::replaceAll(id.substr(id.rfind('/') + 1), "_", " ");
}

std::vector<TimeZone> loadTimeZones(const std::string& zoneinfo)
{
    std::map<std::string, std::string> countries;
    for (const std::string& line : strings::split(cutefish::base::file::read(zoneinfo + "/iso3166.tab").value_or(""), '\n')) {
        if (line.empty() || line.front() == '#')
            continue;
        const std::vector<std::string> fields = strings::split(line, '\t');
        if (fields.size() >= 2)
            countries[fields[0]] = fields[1];
    }

    std::vector<TimeZone> zones;
    for (const std::string& line : strings::split(cutefish::base::file::read(zoneinfo + "/zone.tab").value_or(""), '\n')) {
        if (line.empty() || line.front() == '#')
            continue;
        const std::vector<std::string> fields = strings::split(line, '\t');
        if (fields.size() < 3)
            continue;
        std::string country = cutefish::base::Locale::territoryName(fields[0]);
        if (country.empty())
            country = countries[fields[0]];
        zones.push_back({fields[2], cityOf(fields[2]), country, offsetText(fields[2])});
    }
    std::sort(zones.begin(), zones.end(), [](const TimeZone& a, const TimeZone& b) {
        return a.city != b.city ? a.city < b.city : a.id < b.id;
    });
    zones.insert(zones.begin(), {"Etc/UTC", "UTC", {}, "UTC+00:00"});
    return zones;
}

std::string currentTimeZone(const std::string& localtime)
{
    std::error_code error;
    const std::string target = std::filesystem::read_symlink(localtime, error).string();
    const size_t found = target.find("zoneinfo/");
    if (error || found == std::string::npos)
        return {};
    const std::string id = target.substr(found + 9);
    return id == "UTC" ? "Etc/UTC" : id;
}

bool matchesTimeZone(const TimeZone& zone, std::string_view query)
{
    const std::string haystack = strings::toAsciiLower(zone.city + ' ' + zone.country + ' ' + zone.id);
    for (const std::string& word : strings::splitWhitespace(strings::toAsciiLower(query))) {
        if (haystack.find(word) == std::string::npos
            && strings::replaceAll(haystack, "_", " ").find(word) == std::string::npos)
            return false;
    }
    return true;
}
