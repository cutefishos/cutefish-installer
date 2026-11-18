// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "keepawake.h"

#include "base/dbus.h"
#include "base/event_loop.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace cutefish;

// logind's Inhibit on a session bus connection of its own: it hands out the
// write end of a pipe, and the read end tells whether anyone still holds it.
class FakeLogind
{
public:
    explicit FakeLogind(base::EventLoop* loop)
        : m_bus(base::dbus::Connection::open(base::dbus::Connection::Bus::Session, loop))
    {
        if (!m_bus)
            return;
        m_object = std::make_unique<base::dbus::Object>(m_bus.get(), "/org/freedesktop/login1",
                                                        "org.freedesktop.login1.Manager");
        m_object->addMethod("Inhibit", [this](const std::string& what, const std::string& who,
                                              const std::string& why, const std::string& mode) {
            lastInhibit = {what, who, why, mode};
            int ends[2];
            if (pipe2(ends, O_CLOEXEC) != 0)
                return base::dbus::UnixFd();
            readEnds.push_back(ends[0]);
            return base::dbus::UnixFd::adopt(ends[1]);
        });
        m_valid = m_object->publish() && m_bus->requestName("org.freedesktop.login1");
    }

    ~FakeLogind()
    {
        if (m_bus)
            m_bus->releaseName("org.freedesktop.login1");
        for (int fd : readEnds)
            close(fd);
    }

    bool hasBus() const { return m_bus != nullptr; }
    bool isValid() const { return m_valid; }

    // Whether the descriptor handed out last is still open anywhere.
    bool held() const
    {
        if (readEnds.empty())
            return false;
        pollfd descriptor {readEnds.back(), POLLIN, 0};
        return poll(&descriptor, 1, 0) == 0;
    }

    std::vector<std::string> lastInhibit;
    std::vector<int> readEnds;

private:
    std::unique_ptr<base::dbus::Connection> m_bus;
    std::unique_ptr<base::dbus::Object> m_object;
    bool m_valid = false;
};

static bool runUntil(base::EventLoop& loop, const std::function<bool()>& done)
{
    if (done())
        return true;
    const int check = loop.startTimer(std::chrono::milliseconds(5), true, [&] {
        if (done())
            loop.quit();
    });
    const int timeout = loop.startTimer(std::chrono::seconds(5), false, [&] { loop.quit(); });
    loop.run();
    loop.stopTimer(check);
    loop.stopTimer(timeout);
    return done();
}

// One test: the fake takes logind's name, which tests run side by side on the
// same bus could not share.
TEST(KeepAwake, holdsSleepIdleAndTheLidUntilReleased)
{
    base::EventLoop loop;
    FakeLogind logind(&loop);
    if (!logind.hasBus())
        GTEST_SKIP() << "No session bus";
    ASSERT_TRUE(logind.isValid());
    std::unique_ptr<base::dbus::Connection> bus =
            base::dbus::Connection::open(base::dbus::Connection::Bus::Session, &loop);
    ASSERT_TRUE(bus);
    KeepAwake keepAwake(bus.get());

    keepAwake.hold();
    ASSERT_TRUE(runUntil(loop, [&] { return keepAwake.isHeld(); }));
    EXPECT_EQ(logind.lastInhibit, (std::vector<std::string> {"sleep:idle:handle-lid-switch", "Cutefish Installer",
                                                             "Installing the system", "block"}));
    EXPECT_TRUE(logind.held());
    keepAwake.hold();
    EXPECT_EQ(logind.readEnds.size(), 1u);

    keepAwake.release();
    EXPECT_FALSE(keepAwake.isHeld());
    EXPECT_TRUE(runUntil(loop, [&] { return !logind.held(); }));

    // Released before logind answered: the inhibitor it hands out is dropped.
    keepAwake.hold();
    keepAwake.release();
    ASSERT_TRUE(runUntil(loop, [&] { return logind.readEnds.size() == 2 && !logind.held(); }));
    EXPECT_FALSE(keepAwake.isHeld());
}
