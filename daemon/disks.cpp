// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "disks.h"

#include "base/file.h"
#include "base/strings.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace file = cutefish::base::file;
namespace strings = cutefish::base::strings;

static std::string readTrimmed(const std::string& path)
{
    const std::optional<std::string> contents = file::read(path);
    return contents ? strings::simplified(*contents) : std::string();
}

static bool isSkippedName(const std::string& name)
{
    for (const char* prefix : {"loop", "ram", "zram", "sr", "fd", "dm-", "md", "nbd"}) {
        if (name.starts_with(prefix))
            return true;
    }
    return false;
}

std::vector<Disk> listDisks(const std::string& sys, const std::string& excluded)
{
    std::vector<Disk> disks;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(sys + "/block", error)) {
        const std::string name = entry.path().filename();
        if (isSkippedName(name) || name == excluded)
            continue;
        const std::string directory = entry.path();
        if (readTrimmed(directory + "/ro") == "1")
            continue;
        const std::optional<uint64_t> sectors = strings::toNumber<uint64_t>(readTrimmed(directory + "/size"));
        if (!sectors || *sectors == 0)
            continue;

        Disk disk;
        disk.path = "/dev/" + name;
        // The kernel counts 512-byte sectors here whatever the device's own.
        disk.size = *sectors * 512;
        disk.removable = readTrimmed(directory + "/removable") == "1";
        const std::filesystem::path device = std::filesystem::canonical(entry.path(), error);
        if (!error && device.string().find("/usb") != std::string::npos)
            disk.removable = true;
        error.clear();
        const std::string vendor = readTrimmed(directory + "/device/vendor");
        disk.model = readTrimmed(directory + "/device/model");
        if (!vendor.empty() && vendor != "ATA" && !disk.model.starts_with(vendor))
            disk.model = vendor + ' ' + disk.model;
        disk.model = strings::simplified(disk.model);
        disks.push_back(std::move(disk));
    }
    std::sort(disks.begin(), disks.end(), [](const Disk& a, const Disk& b) {
        if (a.removable != b.removable)
            return !a.removable;
        return a.path < b.path;
    });
    return disks;
}

std::string diskOfMount(const std::string& path, const std::string& sys)
{
    struct stat info {};
    if (::stat(path.c_str(), &info) != 0 || major(info.st_dev) == 0)
        return {};
    std::error_code error;
    const std::string link = sys + "/dev/block/" + std::to_string(major(info.st_dev)) + ':'
            + std::to_string(minor(info.st_dev));
    std::filesystem::path device = std::filesystem::canonical(link, error);
    if (error)
        return {};
    if (std::filesystem::exists(device / "partition"))
        device = device.parent_path();
    return device.filename();
}

std::string partitionPath(const std::string& disk, int number)
{
    const bool endsInDigit = !disk.empty() && disk.back() >= '0' && disk.back() <= '9';
    return disk + (endsInDigit ? "p" : "") + std::to_string(number);
}

bool isOnDisk(const std::string& device, const std::string& disk)
{
    if (device == disk)
        return true;
    if (disk.empty() || !device.starts_with(disk))
        return false;
    std::string_view number = std::string_view(device).substr(disk.size());
    const bool endsInDigit = disk.back() >= '0' && disk.back() <= '9';
    if (endsInDigit) {
        if (!number.starts_with('p'))
            return false;
        number.remove_prefix(1);
    }
    return !number.empty() && std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; });
}
