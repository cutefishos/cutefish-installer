// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "backend.h"

#include "base/i18n.h"

#include <chrono>

namespace dbus = cutefish::base::dbus;
using cutefish::i18n::tr;

Backend::Backend(dbus::Connection* systemBus)
    : m_bus(systemBus)
{
    if (!m_bus)
        return;
    m_proxy = std::make_unique<dbus::Proxy>(m_bus, installer::kService, installer::kPath, installer::kInterface);
    // Authorization may ask for a password first.
    m_proxy->setInteractiveAuthorizationAllowed(true);
    m_proxy->setTimeout(std::chrono::minutes(5));
    m_progress = m_proxy->onSignal("Progress", [this](const dbus::Message& message) {
        std::string stage;
        double fraction = 0;
        if (m_running && message.read(stage, fraction) && onProgress)
            onProgress(stage, fraction);
    });
    m_finished = m_proxy->onSignal("Finished", [this](const dbus::Message& message) {
        bool success = false;
        std::string error;
        if (m_running && message.read(success, error))
            finish(success, error);
    });
    m_owner = m_bus->watchName(installer::kService, [this](const std::string& owner) {
        if (m_running && owner.empty())
            finish(false, tr("The installer service stopped unexpectedly."));
        m_ownerName = owner;
    });
}

Backend::~Backend() = default;

void Backend::loadDisks(std::function<void(std::vector<DiskInfo> disks, std::string error)> done)
{
    if (!m_proxy) {
        done({}, tr("The system bus cannot be reached."));
        return;
    }
    m_disksCall = m_proxy->callAsync("GetDisks", [done](const dbus::Reply& reply) {
        std::vector<dbus::VariantMap> list;
        if (reply.isError() || !reply.read(list)) {
            done({}, reply.isError() ? reply.error().message : std::string(tr("The disks cannot be listed.")));
            return;
        }
        std::vector<DiskInfo> disks;
        for (const dbus::VariantMap& entry : list) {
            DiskInfo disk;
            const auto value = [&entry](const char* key) {
                const auto found = entry.find(key);
                return found == entry.end() ? dbus::Variant() : found->second;
            };
            disk.path = value("path").value<std::string>();
            disk.model = value("model").value<std::string>();
            disk.size = value("size").value<uint64_t>();
            disk.removable = value("removable").value<bool>();
            disks.push_back(std::move(disk));
        }
        done(std::move(disks), {});
    });
}

void Backend::install(const installer::Options& options, std::function<void(std::string error)> done)
{
    if (!m_proxy) {
        done(tr("The system bus cannot be reached."));
        return;
    }
    m_running = true;
    m_installCall = m_proxy->callAsync(
            "Install",
            [this, done](const dbus::Reply& reply) {
                if (!reply.isError()) {
                    done({});
                    return;
                }
                m_running = false;
                done(reply.error().message);
            },
            installer::toMap(options));
}

void Backend::finish(bool success, const std::string& error)
{
    m_running = false;
    if (onFinished)
        onFinished(success, error);
}
