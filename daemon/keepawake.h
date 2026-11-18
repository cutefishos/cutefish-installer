// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus.h"

// Keeps the machine from sleeping, for idleness or with the lid closed, while
// held: a logind block inhibitor, which goes with the process too.
class KeepAwake
{
public:
    explicit KeepAwake(cutefish::base::dbus::Connection* bus);

    KeepAwake(const KeepAwake&) = delete;
    KeepAwake& operator=(const KeepAwake&) = delete;

    void hold();
    void release();

    bool isHeld() const { return m_inhibitor.isValid(); }

private:
    cutefish::base::dbus::Proxy m_logind;
    cutefish::base::dbus::PendingCall m_inhibiting;
    cutefish::base::dbus::UnixFd m_inhibitor;
    bool m_wanted = false;
};
