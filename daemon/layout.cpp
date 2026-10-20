// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "layout.h"

#include "base/file.h"
#include "base/json.h"
#include "base/process.h"
#include "base/strings.h"

#include <chrono>
#include <unistd.h>

namespace json = cutefish::base::json;
namespace strings = cutefish::base::strings;

// The partition number at the end of a device node: 2 for /dev/sda2 and
// /dev/nvme0n1p2.
static int numberOf(const std::string& node)
{
    size_t digits = node.size();
    while (digits > 0 && node[digits - 1] >= '0' && node[digits - 1] <= '9')
        --digits;
    return strings::toNumber<int>(std::string_view(node).substr(digits)).value_or(0);
}

static std::optional<std::string> output(const std::string& program, const std::vector<std::string>& arguments)
{
    cutefish::base::Process process;
    if (!process.start(program, arguments) || !process.waitForFinished(std::chrono::seconds(10)))
        return std::nullopt;
    return process.standardOutput();
}

std::optional<installer::DiskLayout> parseSfdisk(std::string_view text)
{
    const std::optional<json::Value> document = json::Value::parse(text);
    if (!document || !(*document)["partitiontable"].isObject())
        return std::nullopt;
    const json::Value& table = (*document)["partitiontable"];
    installer::DiskLayout layout;
    layout.disk = table["device"].toString();
    layout.table = table["label"].toString();
    layout.sectorSize = uint64_t(table["sectorsize"].toInt(512));
    for (const json::Value& entry : table["partitions"].toArray()) {
        installer::ExistingPartition partition;
        partition.path = entry["node"].toString();
        partition.number = numberOf(partition.path);
        partition.start = uint64_t(entry["start"].toInt()) * layout.sectorSize;
        partition.size = uint64_t(entry["size"].toInt()) * layout.sectorSize;
        partition.type = strings::toAsciiLower(entry["type"].toString());
        layout.partitions.push_back(std::move(partition));
    }
    return layout;
}

// lsblk nests partitions under their disk, or lists them beside it.
static void addDeviceFileSystems(installer::DiskLayout& layout, const json::Value& devices)
{
    for (const json::Value& device : devices.toArray()) {
        for (installer::ExistingPartition& partition : layout.partitions) {
            if (partition.path != device["path"].toString())
                continue;
            partition.fsType = device["fstype"].toString();
            partition.label = device["label"].toString();
        }
        addDeviceFileSystems(layout, device["children"]);
    }
}

void addFileSystems(installer::DiskLayout& layout, std::string_view lsblkJson)
{
    if (const std::optional<json::Value> document = json::Value::parse(lsblkJson))
        addDeviceFileSystems(layout, (*document)["blockdevices"]);
}

std::optional<installer::DiskLayout> readLayout(const std::string& disk, const std::string& sys)
{
    const std::string name = disk.substr(disk.rfind('/') + 1);
    const std::optional<uint64_t> sectors =
            strings::toNumber<uint64_t>(cutefish::base::file::read(sys + "/block/" + name + "/size").value_or(""));
    if (!sectors || *sectors == 0)
        return std::nullopt;

    installer::DiskLayout layout;
    if (const std::optional<std::string> text = output("sfdisk", {"--json", disk})) {
        if (std::optional<installer::DiskLayout> parsed = parseSfdisk(*text))
            layout = std::move(*parsed);
    }
    layout.disk = disk;
    // The kernel counts 512-byte sectors here whatever the device's own.
    layout.size = *sectors * 512;
    if (const std::optional<std::string> text = output("lsblk", {"--json", "--bytes", "--output", "PATH,FSTYPE,LABEL", disk}))
        addFileSystems(layout, *text);
    return layout;
}

std::vector<std::string> availableFileSystems()
{
    std::vector<std::string> types;
    for (const std::string& type : installer::fileSystems()) {
        const std::string program = mkfsCommand(type, {}).front();
        if (::access(("/usr/sbin/" + program).c_str(), X_OK) == 0 || ::access(("/sbin/" + program).c_str(), X_OK) == 0)
            types.push_back(type);
    }
    return types;
}

std::vector<std::string> mkfsCommand(const std::string& fsType, const std::string& label)
{
    std::vector<std::string> command;
    if (fsType == "ext4")
        command = {"mkfs.ext4", "-F", "-q"};
    else if (fsType == "btrfs")
        command = {"mkfs.btrfs", "-f", "-q"};
    else if (fsType == "xfs")
        command = {"mkfs.xfs", "-f", "-q"};
    else if (fsType == "vfat")
        command = {"mkfs.vfat", "-F", "32"};
    else
        command = {"mkswap"};
    if (!label.empty()) {
        command.push_back(fsType == "vfat" ? "-n" : "-L");
        command.push_back(label);
    }
    return command;
}
