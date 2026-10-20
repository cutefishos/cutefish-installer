// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "common/installer.h"
#include "common/partitioning.h"

#include "base/dbus.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DiskInfo
{
    std::string path;
    std::string model;
    uint64_t size = 0;
    bool removable = false;
};

// The installer service on the system bus, which does the work as root.
class Backend
{
public:
    explicit Backend(cutefish::base::dbus::Connection* systemBus);
    ~Backend();

    // The disks, or the error that kept them back.
    void loadDisks(std::function<void(std::vector<DiskInfo> disks, std::string error)> done);
    // The disk's partitions and the file systems that can be made, or the
    // error that kept them back.
    void loadLayout(const std::string& disk,
                    std::function<void(installer::DiskLayout layout, std::vector<std::string> fileSystems,
                                       std::string error)> done);
    // Starts the installation; an error when it could not start.
    void install(const installer::Options& options, std::function<void(std::string error)> done);
    bool isRunning() const { return m_running; }

    // While installing.
    std::function<void(const std::string& stage, double fraction)> onProgress;
    std::function<void(bool success, const std::string& error)> onFinished;

private:
    void finish(bool success, const std::string& error);

    cutefish::base::dbus::Connection* m_bus;
    std::unique_ptr<cutefish::base::dbus::Proxy> m_proxy;
    cutefish::base::dbus::PendingCall m_disksCall;
    cutefish::base::dbus::PendingCall m_layoutCall;
    cutefish::base::dbus::PendingCall m_fileSystemsCall;
    cutefish::base::dbus::PendingCall m_installCall;
    cutefish::base::dbus::Match m_progress;
    cutefish::base::dbus::Match m_finished;
    cutefish::base::dbus::NameWatch m_owner;
    std::string m_ownerName;
    bool m_running = false;
};
