// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Disk
{
    // As /dev/<name>.
    std::string path;
    std::string model;
    uint64_t size = 0;
    bool removable = false;
};

// The whole disks the system can go on, from sysfs under `sys`: no loop, RAM,
// optical, mapped or read-only devices, and not `excluded` (a name such as
// "sda"), the disk the live system runs from.
std::vector<Disk> listDisks(const std::string& sys, const std::string& excluded);

// The name of the whole disk holding the file system mounted at `path`, such
// as "sda" for a medium on sda1; empty when it is on no disk.
std::string diskOfMount(const std::string& path, const std::string& sys = "/sys");

// The path of partition `number` on `disk`: /dev/sda2, /dev/nvme0n1p2.
std::string partitionPath(const std::string& disk, int number);

// The device is the disk or one of its partitions; /dev/sda1 is on /dev/sda,
// /dev/loop10 is not on /dev/loop1.
bool isOnDisk(const std::string& device, const std::string& disk);
