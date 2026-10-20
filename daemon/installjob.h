// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "common/installer.h"

#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Puts the live system on a disk: partitions and formats it, copies the live
// image, sets up the user and the machine, swaps the live packages for a boot
// loader and unmounts everything again. Runs commands one after the other and
// blocks, so it belongs on a thread of its own.
class InstallJob
{
public:
    struct Paths
    {
        std::string squashfs = "/run/live/medium/live/filesystem.squashfs";
        // Where the medium's package pool is.
        std::string medium = "/run/live/medium";
        std::string target = "/run/cutefish-installer/target";
        std::string log = "/var/log/cutefish-installer.log";
        // Off, the firmware's boot entries stay as they are; only the
        // removable media path boots the disk.
        bool nvram = true;
    };

    InstallJob(Paths paths, installer::Options options);
    ~InstallJob();

    InstallJob(const InstallJob&) = delete;
    InstallJob& operator=(const InstallJob&) = delete;

    // A stage of installer.h and the fraction done of the whole.
    std::function<void(const std::string& stage, double fraction)> onProgress;

    // Empty when the system was installed, what went wrong otherwise.
    std::string run();

private:
    bool install();
    bool releaseDisk();
    bool partition();
    bool mountChrootFileSystems();
    bool configure();
    bool createUser();
    bool removeLivePackages();
    bool installBootloader();
    bool installPackagesFromMedium(const std::vector<std::string>& packages);
    void copyLog();
    void unmountAll();

    // The program with its arguments, its input on stdin; every line it
    // writes to stdout or stderr is logged and passed to `line`.
    bool exec(const std::vector<std::string>& command, const std::string& input = {},
              const std::function<void(std::string_view line)>& line = {}, std::string* output = nullptr);
    bool chroot(std::vector<std::string> command, const std::string& input = {}, std::string* output = nullptr);
    bool mount(std::vector<std::string> arguments, const std::string& mountPoint);
    std::vector<std::string> missingPackages(const std::vector<std::string>& packages);
    std::string uuidOf(const std::string& device);
    bool writeFile(const std::string& path, const std::string& contents, unsigned mode = 0644);
    std::string inTarget(const std::string& path) const { return m_paths.target + path; }

    void progress(const char* stage, double fraction);
    bool fail(std::string message);
    void log(std::string_view text);

    Paths m_paths;
    installer::Options m_options;
    bool m_uefi = false;
    std::string m_espDevice;
    std::string m_rootDevice;
    std::vector<std::string> m_mounts;
    std::string m_error;
    std::FILE* m_log = nullptr;
};
