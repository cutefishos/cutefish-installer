// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "keepawake.h"

#include "base/logging.h"

#include <string>
#include <utility>

namespace dbus = cutefish::base::dbus;

static const cutefish::base::LogCategory kLog("cutefish.installer");

// idle holds off the power service's idle suspend, sleep refuses its Suspend
// calls, and handle-lid-switch keeps logind's own lid action from sleeping.
static constexpr char kWhat[] = "sleep:idle:handle-lid-switch";

KeepAwake::KeepAwake(dbus::Connection* bus)
    : m_logind(bus, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager")
{
}

void KeepAwake::hold()
{
    m_wanted = true;
    if (m_inhibitor.isValid() || !m_inhibiting.isNull() || !m_logind.isValid())
        return;
    m_inhibiting = m_logind.callAsync(
            "Inhibit",
            [this](const dbus::Reply& reply) {
                dbus::PendingCall done = std::move(m_inhibiting);
                const auto inhibitor = reply.value<dbus::UnixFd>();
                if (!inhibitor) {
                    CF_WARNING(kLog) << "cutefish-installer: cannot keep the machine awake:"
                                     << reply.error().message;
                    return;
                }
                if (m_wanted)
                    m_inhibitor = *inhibitor;
            },
            std::string(kWhat), std::string("Cutefish Installer"), std::string("Installing the system"),
            std::string("block"));
}

void KeepAwake::release()
{
    m_wanted = false;
    m_inhibitor = dbus::UnixFd();
}
