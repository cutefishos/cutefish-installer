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

// What Install takes: the whole disk is erased and the system put on it.
struct Options
{
    std::string disk;
    std::string fullName;
    std::string userName;
    std::string password;
    std::string hostName;
    std::string timeZone;
};

cutefish::base::dbus::VariantMap toMap(const Options& options);
std::optional<Options> optionsFromMap(const cutefish::base::dbus::VariantMap& map);

// A login name useradd takes with its default rules, lower case only.
bool isValidUserName(std::string_view name);
// One label of a host name: letters, digits and inner hyphens.
bool isValidHostName(std::string_view name);
// An Area/Location name below `zoneinfo` that names a zone file.
bool isValidTimeZone(const std::string& zone, const std::string& zoneinfo = "/usr/share/zoneinfo");
// The first word of the full name in lower case, when it is plain ASCII;
// empty otherwise.
std::string suggestedUserName(std::string_view fullName);
std::string suggestedHostName(std::string_view userName);

// The disk size below which there is no room for the system and its updates.
inline constexpr uint64_t kMinimumDiskSize = 16'000'000'000;

} // namespace installer
