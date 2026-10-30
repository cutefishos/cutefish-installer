// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The files and inputs an installation writes, apart from the commands that
// write them.
namespace plan {

// One file system the installed system mounts, or swaps to.
struct Mount
{
    std::string uuid;
    // A directory, or installer::kMountSwap.
    std::string mountPoint;
    std::string fsType;
};

// The mounts by UUID, the root first.
std::string fstab(const std::vector<Mount>& mounts);

// sfdisk --append's input for one partition, in sectors; GPT partitions get
// the name.
std::string newPartition(uint64_t startSector, uint64_t sizeSectors, const std::string& type, const std::string& name);

// The indexes of the mount points in the order to mount them: each after the
// one it is inside, swap last.
std::vector<size_t> mountOrder(const std::vector<std::string>& mountPoints);

std::string hosts(const std::string& hostName);

// The groups of `wanted` that the group file lists, in order.
std::vector<std::string> existingGroups(std::string_view groupFile, const std::vector<std::string>& wanted);

// AccountsService's file for an account whose picture is the icon.
std::string accountsServiceUser(const std::string& icon);

// The groups a new user joins when the system has them.
const std::vector<std::string>& userGroups();

// The packages only a live system needs.
const std::vector<std::string>& livePackages();

// The installed packages of dpkg-query -W -f '${db:Status-Abbrev} ${Package}\n'.
std::vector<std::string> installedPackages(std::string_view dpkgQueryOutput);

// The packages that install and update the boot loader, beside the
// programs the live image has.
std::vector<std::string> bootloaderPackages(bool uefi);

// grub-install's --target for this processor's UEFI.
std::string efiTarget();

// A percentage line of unsquashfs -percentage, as a fraction.
std::optional<double> unsquashfsProgress(std::string_view line);

// The VERSION_CODENAME of an os-release file.
std::string codename(std::string_view osRelease);

} // namespace plan
