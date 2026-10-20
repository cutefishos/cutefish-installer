// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "disks.h"
#include "installjob.h"

#include "base/dbus.h"
#include "base/timer.h"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// org.cutefish.Installer: lists the disks the live system can go on and
// installs it on one, for an active local session without a password. Works
// only from a live medium.
class InstallerService
{
public:
    struct Settings
    {
        InstallJob::Paths paths;
        // Listed instead of the disks found, such as loop devices, for tests.
        std::vector<std::string> testDisks;
    };

    explicit InstallerService(Settings settings);
    ~InstallerService();

    bool exportTo(cutefish::base::dbus::Connection* bus);

    std::vector<Disk> disks() const;

    // Nothing called for a while and nothing being installed.
    std::function<void()> onIdle;

private:
    void getLayout(cutefish::base::dbus::MethodCall& call);
    void install(cutefish::base::dbus::MethodCall& call);
    // Empty when the options can be installed; the partitions to make go
    // into `plan`.
    std::string checkOptions(const installer::Options& options, installer::PartitionPlan* plan) const;
    void start(const installer::Options& options);
    void finished(const std::string& error);

    Settings m_settings;
    cutefish::base::Timer m_idleTimer;
    cutefish::base::dbus::Connection* m_bus = nullptr;
    std::unique_ptr<cutefish::base::dbus::Object> m_object;
    cutefish::base::dbus::PendingCall m_authorization;
    bool m_authorizing = false;
    bool m_running = false;
    std::thread m_thread;
};
