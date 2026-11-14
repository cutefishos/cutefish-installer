// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace installer {

inline constexpr char kService[] = "org.cutefish.Installer";
inline constexpr char kPath[] = "/org/cutefish/Installer";
inline constexpr char kInterface[] = "org.cutefish.Installer";

// The stages Progress names, in order.
inline constexpr char kStagePrepare[] = "prepare";
inline constexpr char kStageCopy[] = "copy";
inline constexpr char kStageConfigure[] = "configure";
inline constexpr char kStageBootloader[] = "bootloader";
inline constexpr char kStageFinish[] = "finish";

// What Install takes. Without a partition plan the whole disk is erased.
struct Options
{
    std::string disk;
    std::string fullName;
    std::string userName;
    std::string password;
    std::string hostName;
    std::string timeZone;
    // The locale the installed system starts in, such as "de_DE.UTF-8";
    // empty keeps the live system's.
    std::string language;
    // The file name of the preset the account's picture is; empty leaves the
    // choice to the service.
    std::string picture;
    // A PartitionPlan's toMap(), empty to erase the disk.
    cutefish::base::dbus::VariantMap partitioning;
};

cutefish::base::dbus::VariantMap toMap(const Options& options);
std::optional<Options> optionsFromMap(const cutefish::base::dbus::VariantMap& map);

// A login name useradd takes with its default rules, lower case only.
bool isValidUserName(std::string_view name);
// One label of a host name: letters, digits and inner hyphens.
bool isValidHostName(std::string_view name);
// A locale the system can generate in UTF-8, as the SUPPORTED file lists it,
// such as "de_DE.UTF-8".
bool isValidLocale(const std::string& locale, const std::string& supported = "/usr/share/i18n/SUPPORTED");
// A plain file name, as Options::picture is one.
bool isValidPictureName(std::string_view name);
// An Area/Location name below `zoneinfo` that names a zone file.
bool isValidTimeZone(const std::string& zone, const std::string& zoneinfo = "/usr/share/zoneinfo");
// The first word of the full name in lower case, when it is plain ASCII;
// empty otherwise.
std::string suggestedUserName(std::string_view fullName);
std::string suggestedHostName(std::string_view userName);

// The disk size below which there is no room for the system and its updates.
inline constexpr uint64_t kMinimumDiskSize = 16'000'000'000;

} // namespace installer
