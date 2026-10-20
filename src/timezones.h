// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>
#include <string_view>
#include <vector>

struct TimeZone
{
    // Such as "America/Argentina/Cordoba".
    std::string id;
    // "Cordoba"; "UTC" for Etc/UTC.
    std::string city;
    std::string country;
    // From UTC now, such as "UTC+08:00".
    std::string offset;
};

// The zones of zone.tab with their countries' names, by city, and UTC first.
std::vector<TimeZone> loadTimeZones(const std::string& zoneinfo = "/usr/share/zoneinfo");

// The zone /etc/localtime names; empty when it names none.
std::string currentTimeZone(const std::string& localtime = "/etc/localtime");

// The zone's city, country or id holds every word of the query, ignoring case.
bool matchesTimeZone(const TimeZone& zone, std::string_view query);
