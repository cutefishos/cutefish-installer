// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "common/partitioning.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The table and partitions of sfdisk --json, sizes in bytes; empty when the
// text is not sfdisk's.
std::optional<installer::DiskLayout> parseSfdisk(std::string_view json);

// Adds the file systems and labels of lsblk --json --bytes --output
// PATH,FSTYPE,LABEL to the partitions.
void addFileSystems(installer::DiskLayout& layout, std::string_view lsblkJson);

// The disk as it is now; a disk without a partition table has none and no
// partitions. Empty when the disk cannot be read.
std::optional<installer::DiskLayout> readLayout(const std::string& disk, const std::string& sys = "/sys");

// The file systems of installer::fileSystems() whose tools are installed.
std::vector<std::string> availableFileSystems();

// The program that makes the file system, with its arguments before the device.
std::vector<std::string> mkfsCommand(const std::string& fsType, const std::string& label);
