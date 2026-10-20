// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timezones.h"

#include <algorithm>
#include <gtest/gtest.h>

TEST(TimeZones, load)
{
    const std::vector<TimeZone> zones = loadTimeZones();
    ASSERT_GT(zones.size(), 300u);
    EXPECT_EQ(zones.front().id, "Etc/UTC");
    const auto berlin = std::find_if(zones.begin(), zones.end(), [](const TimeZone& zone) {
        return zone.id == "Europe/Berlin";
    });
    ASSERT_NE(berlin, zones.end());
    EXPECT_EQ(berlin->city, "Berlin");
    EXPECT_EQ(berlin->country, "Germany");
    EXPECT_TRUE(berlin->offset == "UTC+01:00" || berlin->offset == "UTC+02:00");
    const auto kolkata = std::find_if(zones.begin(), zones.end(), [](const TimeZone& zone) {
        return zone.id == "Asia/Kolkata";
    });
    ASSERT_NE(kolkata, zones.end());
    EXPECT_EQ(kolkata->offset, "UTC+05:30");
}

TEST(TimeZones, match)
{
    const TimeZone zone {"America/New_York", "New York", "United States", "UTC-05:00"};
    EXPECT_TRUE(matchesTimeZone(zone, ""));
    EXPECT_TRUE(matchesTimeZone(zone, "new york"));
    EXPECT_TRUE(matchesTimeZone(zone, "new_york"));
    EXPECT_TRUE(matchesTimeZone(zone, "united york"));
    EXPECT_TRUE(matchesTimeZone(zone, "America"));
    EXPECT_FALSE(matchesTimeZone(zone, "berlin"));
}
