// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The files and inputs an installation writes, apart from the commands that
// write them.
namespace plan {

// sfdisk's input for a GPT disk: an EFI system partition and the root file
// system with UEFI; a BIOS boot partition for the boot loader's core and the
// root file system without.
std::string partitionTable(bool uefi);

// The root file system by its UUID and, with UEFI, the EFI system partition.
std::string fstab(const std::string& rootUuid, const std::string& espUuid);

std::string hosts(const std::string& hostName);

// The groups of `wanted` that the group file lists, in order.
std::vector<std::string> existingGroups(std::string_view groupFile, const std::vector<std::string>& wanted);

// The groups a new user joins when the system has them.
const std::vector<std::string>& userGroups();

// The packages only a live system needs.
const std::vector<std::string>& livePackages();

// The installed packages of dpkg-query -W -f '${db:Status-Abbrev} ${Package}\n'.
std::vector<std::string> installedPackages(std::string_view dpkgQueryOutput);

// The packages that install and update the boot loader, beside the
// programs the live image has.
std::vector<std::string> bootloaderPackages(bool uefi);

// A percentage line of unsquashfs -percentage, as a fraction.
std::optional<double> unsquashfsProgress(std::string_view line);

// The VERSION_CODENAME of an os-release file.
std::string codename(std::string_view osRelease);

} // namespace plan
