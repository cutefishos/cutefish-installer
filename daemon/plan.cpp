// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "plan.h"

#include "base/strings.h"

#include <algorithm>
#include <set>

namespace strings = cutefish::base::strings;

namespace plan {

std::string partitionTable(bool uefi)
{
    std::string table = "label: gpt\n";
    if (uefi)
        table += "size=512MiB, type=uefi, name=\"EFI\"\n";
    else
        table += "size=1MiB, type=21686148-6449-6E6F-744E-656564454649, name=\"BIOS\"\n";
    table += "type=linux, name=\"Cutefish\"\n";
    return table;
}

std::string fstab(const std::string& rootUuid, const std::string& espUuid)
{
    std::string text = "# <file system> <mount point> <type> <options> <dump> <pass>\n";
    text += "UUID=" + rootUuid + " / ext4 defaults,noatime 0 1\n";
    if (!espUuid.empty())
        text += "UUID=" + espUuid + " /boot/efi vfat umask=0077 0 1\n";
    return text;
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
    if (uefi)
        return {"grub-efi-amd64"};
    return {"grub-pc"};
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
