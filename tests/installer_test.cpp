// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "common/installer.h"

#include <gtest/gtest.h>

using namespace installer;

TEST(Installer, userNames)
{
    EXPECT_TRUE(isValidUserName("reion"));
    EXPECT_TRUE(isValidUserName("_build-2"));
    EXPECT_FALSE(isValidUserName(""));
    EXPECT_FALSE(isValidUserName("Reion"));
    EXPECT_FALSE(isValidUserName("2reion"));
    EXPECT_FALSE(isValidUserName("re ion"));
    EXPECT_FALSE(isValidUserName(std::string(33, 'a')));
}

TEST(Installer, hostNames)
{
    EXPECT_TRUE(isValidHostName("reion-pc"));
    EXPECT_TRUE(isValidHostName("Laptop2"));
    EXPECT_FALSE(isValidHostName("-pc"));
    EXPECT_FALSE(isValidHostName("pc-"));
    EXPECT_FALSE(isValidHostName("my.pc"));
    EXPECT_FALSE(isValidHostName(std::string(64, 'a')));
}

TEST(Installer, suggestions)
{
    EXPECT_EQ(suggestedUserName("Reion Wong"), "reion");
    EXPECT_EQ(suggestedUserName("  Ada  "), "ada");
    EXPECT_EQ(suggestedUserName("Zoë Smith"), "");
    EXPECT_EQ(suggestedUserName("3D Artist"), "");
    EXPECT_EQ(suggestedHostName("reion"), "reion-pc");
    EXPECT_EQ(suggestedHostName("_x"), "x-pc");
    EXPECT_EQ(suggestedHostName(""), "");
}

TEST(Installer, timeZones)
{
    EXPECT_TRUE(isValidTimeZone("Europe/Berlin"));
    EXPECT_TRUE(isValidTimeZone("America/Argentina/Cordoba"));
    EXPECT_TRUE(isValidTimeZone("Etc/UTC"));
    EXPECT_FALSE(isValidTimeZone("UTC"));
    EXPECT_FALSE(isValidTimeZone("../../etc/passwd"));
    EXPECT_FALSE(isValidTimeZone("Europe/../Europe/Berlin"));
    EXPECT_FALSE(isValidTimeZone("Europe/Nowhere"));
}

TEST(Installer, optionsRoundTrip)
{
    const Options options {"/dev/sda", "Reion Wong", "reion", "secret", "reion-pc", "Asia/Singapore"};
    const std::optional<Options> back = optionsFromMap(toMap(options));
    ASSERT_TRUE(back);
    EXPECT_EQ(back->disk, options.disk);
    EXPECT_EQ(back->fullName, options.fullName);
    EXPECT_EQ(back->password, options.password);
    EXPECT_EQ(back->timeZone, options.timeZone);

    cutefish::base::dbus::VariantMap partial = toMap(options);
    partial.erase("hostName");
    EXPECT_FALSE(optionsFromMap(partial));
    partial = toMap(options);
    partial["disk"] = cutefish::base::dbus::Variant(uint32_t(1));
    EXPECT_FALSE(optionsFromMap(partial));
}
