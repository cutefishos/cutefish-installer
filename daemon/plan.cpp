// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "plan.h"

#include "common/partitioning.h"

#include "base/strings.h"

#include <algorithm>
#include <set>

namespace strings = cutefish::base::strings;

namespace plan {

std::string fstab(const std::vector<Mount>& mounts)
{
    std::string text = "# <file system> <mount point> <type> <options> <dump> <pass>\n";
    std::vector<std::string> mountPoints;
    for (const Mount& mount : mounts)
        mountPoints.push_back(mount.mountPoint);
    for (const size_t index : mountOrder(mountPoints)) {
        const Mount& mount = mounts[index];
        const std::string device = "UUID=" + mount.uuid;
        if (mount.mountPoint == installer::kMountSwap) {
            text += device + " none swap sw 0 0\n";
        } else if (mount.fsType == "vfat") {
            text += device + ' ' + mount.mountPoint + " vfat umask=0077 0 1\n";
        } else {
            // fsck checks ext4 only: the root first, the rest after it.
            const char* pass = mount.fsType != "ext4" ? "0" : mount.mountPoint == "/" ? "1" : "2";
            text += device + ' ' + mount.mountPoint + ' ' + mount.fsType + " defaults,noatime 0 " + pass + '\n';
        }
    }
    return text;
}

std::string newPartition(uint64_t startSector, uint64_t sizeSectors, const std::string& type, const std::string& name)
{
    std::string line = "start=" + std::to_string(startSector) + ", size=" + std::to_string(sizeSectors) + ", type=" + type;
    if (!name.empty())
        line += ", name=\"" + name + '"';
    return line + '\n';
}

std::vector<size_t> mountOrder(const std::vector<std::string>& mountPoints)
{
    const auto depth = [&mountPoints](size_t index) {
        const std::string& mountPoint = mountPoints[index];
        if (mountPoint == installer::kMountSwap)
            return size_t(1000);
        return mountPoint == "/" ? size_t(0) : size_t(std::count(mountPoint.begin(), mountPoint.end(), '/'));
    };
    std::vector<size_t> order(mountPoints.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&depth](size_t a, size_t b) { return depth(a) < depth(b); });
    return order;
}

std::string hosts(const std::string& hostName)
{
    return "127.0.0.1\tlocalhost\n"
           "127.0.1.1\t" + hostName + "\n"
           "\n"
           "::1\t\tlocalhost ip6-localhost ip6-loopback\n"
           "ff02::1\t\tip6-allnodes\n"
           "ff02::2\t\tip6-allrouters\n";
}

std::string accountsServiceUser(const std::string& icon)
{
    return "[User]\n"
           "Icon=" + icon + "\n"
           "SystemAccount=false\n";
}

std::vector<std::string> existingGroups(std::string_view groupFile, const std::vector<std::string>& wanted)
{
    std::set<std::string> listed;
    for (const std::string& line : strings::split(groupFile, '\n')) {
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
            listed.insert(line.substr(0, colon));
    }
    std::vector<std::string> groups;
    for (const std::string& group : wanted) {
        if (listed.contains(group))
            groups.push_back(group);
    }
    return groups;
}

const std::vector<std::string>& userGroups()
{
    static const std::vector<std::string> groups {"sudo", "adm", "audio", "video", "plugdev", "netdev",
                                                  "bluetooth", "lpadmin", "scanner", "cdrom", "dip"};
    return groups;
}

const std::vector<std::string>& livePackages()
{
    static const std::vector<std::string> packages {
            "live-boot",   "live-boot-doc", "live-boot-initramfs-tools", "live-config", "live-config-doc",
            "live-config-systemd", "live-tools", "live-task-localisation", "live-task-recommended",
            "cutefish-installer", "calamares", "calamares-settings-debian"};
    return packages;
}

std::vector<std::string> installedPackages(std::string_view dpkgQueryOutput)
{
    std::vector<std::string> packages;
    for (const std::string& line : strings::split(dpkgQueryOutput, '\n')) {
        const std::vector<std::string> fields = strings::splitWhitespace(line);
        if (fields.size() == 2 && fields[0] == "ii")
            packages.push_back(fields[1].substr(0, fields[1].find(':')));
    }
    return packages;
}

std::vector<std::string> bootloaderPackages(bool uefi)
{
#if defined(__aarch64__)
    (void)uefi;
    return {"grub-efi-arm64"};
#else
    if (uefi)
        return {"grub-efi-amd64"};
    return {"grub-pc"};
#endif
}

std::string efiTarget()
{
#if defined(__aarch64__)
    return "arm64-efi";
#else
    return "x86_64-efi";
#endif
}

std::optional<double> unsquashfsProgress(std::string_view line)
{
    const std::optional<int> percent = strings::toNumber<int>(strings::trimmed(line));
    if (!percent || *percent < 0 || *percent > 100)
        return std::nullopt;
    return *percent / 100.0;
}

std::string codename(std::string_view osRelease)
{
    for (const std::string& line : strings::split(osRelease, '\n')) {
        if (!line.starts_with("VERSION_CODENAME="))
            continue;
        std::string value = line.substr(17);
        value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
        return value;
    }
    return {};
}

} // namespace plan
