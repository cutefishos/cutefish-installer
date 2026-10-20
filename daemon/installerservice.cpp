// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "installerservice.h"

#include "base/event_loop.h"
#include "base/file.h"
#include "base/logging.h"
#include "base/strings.h"

#include <algorithm>
#include <map>
#include <tuple>

namespace dbus = cutefish::base::dbus;
namespace strings = cutefish::base::strings;

static const cutefish::base::LogCategory kLog("cutefish.installer");

static constexpr char kAction[] = "org.cutefish.installer.install";

// polkit's Subject: a kind and its details.
using PolkitSubject = std::tuple<std::string, dbus::VariantMap>;

// A name the live image's system accounts already take.
static bool isSystemAccount(const std::string& name)
{
    const std::optional<std::string> passwd = cutefish::base::file::read("/etc/passwd");
    if (!passwd)
        return false;
    for (const std::string& line : strings::split(*passwd, '\n')) {
        const std::vector<std::string> fields = strings::split(line, ':');
        if (fields.size() < 3 || fields[0] != name)
            continue;
        const std::optional<unsigned> uid = strings::toNumber<unsigned>(fields[2]);
        return !uid || *uid < 1000 || *uid == 65534;
    }
    return false;
}

InstallerService::InstallerService(Settings settings)
    : m_settings(std::move(settings))
{
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(std::chrono::seconds(60));
    m_idleTimer.onTimeout = [this] {
        if (!m_running && !m_authorizing && onIdle)
            onIdle();
    };
    m_idleTimer.start();
}

InstallerService::~InstallerService()
{
    if (m_thread.joinable())
        m_thread.join();
}

bool InstallerService::exportTo(dbus::Connection* bus)
{
    m_bus = bus;
    m_object = std::make_unique<dbus::Object>(bus, installer::kPath, installer::kInterface);
    m_object->addMethod(
            "GetDisks",
            [this] {
                m_idleTimer.start();
                std::vector<dbus::VariantMap> list;
                for (const Disk& disk : disks()) {
                    list.push_back({
                            {"path", dbus::Variant(disk.path)},
                            {"model", dbus::Variant(disk.model)},
                            {"size", dbus::Variant(disk.size)},
                            {"removable", dbus::Variant(disk.removable)},
                    });
                }
                return list;
            },
            {{}, {"disks"}});
    m_object->addMethod(
            "Install", "a{sv}", "", [this](dbus::MethodCall& call) { install(call); }, {{"options"}, {}});
    m_object->addProperty("Running", [this] { return m_running; });
    m_object->addSignal("Progress", "sd", {"stage", "fraction"});
    m_object->addSignal("Finished", "bs", {"success", "error"});
    return m_object->publish();
}

std::vector<Disk> InstallerService::disks() const
{
    if (m_settings.testDisks.empty())
        return listDisks("/sys", diskOfMount(m_settings.paths.medium));
    std::vector<Disk> disks;
    for (const std::string& path : m_settings.testDisks) {
        Disk disk;
        disk.path = path;
        disk.model = "Test disk";
        const std::string name = path.substr(path.rfind('/') + 1);
        const std::optional<uint64_t> sectors = strings::toNumber<uint64_t>(
                cutefish::base::file::read("/sys/block/" + name + "/size").value_or(""));
        disk.size = sectors.value_or(0) * 512;
        disks.push_back(disk);
    }
    return disks;
}

void InstallerService::install(dbus::MethodCall& call)
{
    m_idleTimer.start();

    dbus::VariantMap map;
    call.read(map);
    const std::optional<installer::Options> options = installer::optionsFromMap(map);
    if (!options) {
        call.replyError("org.freedesktop.DBus.Error.InvalidArgs", "Incomplete options");
        return;
    }
    if (m_running || m_authorizing) {
        call.replyError("org.cutefish.Installer.Error.Busy", "An installation is running");
        return;
    }
    if (const std::string problem = checkOptions(*options); !problem.empty()) {
        call.replyError("org.freedesktop.DBus.Error.InvalidArgs", problem);
        return;
    }

    dbus::Message check = m_bus->newMethodCall("org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
                                               "org.freedesktop.PolicyKit1.Authority", "CheckAuthorization");
    const PolkitSubject subject {"system-bus-name", {{"name", dbus::Variant(call.message().sender())}}};
    // 1: the user may be asked, where the policy wants a password.
    check.append(subject, std::string(kAction), std::map<std::string, std::string>(), uint32_t(1), std::string());

    m_authorizing = true;
    dbus::MethodCall request = call;
    m_authorization = m_bus->callAsync(
            check,
            [this, request, options = *options](const dbus::Reply& reply) mutable {
                m_authorizing = false;
                m_idleTimer.start();

                bool authorized = false;
                std::tuple<bool, bool, std::map<std::string, std::string>> result;
                if (reply.read(result))
                    authorized = std::get<0>(result);
                else
                    CF_WARNING(kLog) << "cutefish-installer: polkit check failed:" << reply.error().message;

                if (!authorized) {
                    request.replyError("org.freedesktop.DBus.Error.AccessDenied", "Not authorized to install");
                    return;
                }
                request.reply();
                start(options);
            },
            std::chrono::minutes(5));
}

std::string InstallerService::checkOptions(const installer::Options& options) const
{
    const std::vector<Disk> disks = this->disks();
    const auto disk = std::find_if(disks.begin(), disks.end(),
                                   [&options](const Disk& disk) { return disk.path == options.disk; });
    if (disk == disks.end())
        return "Not a disk the system can be installed on: " + options.disk;
    if (disk->size < installer::kMinimumDiskSize && m_settings.testDisks.empty())
        return "The disk is too small";
    if (!installer::isValidUserName(options.userName) || isSystemAccount(options.userName))
        return "Not a user name that can be used: " + options.userName;
    if (options.password.empty() || options.password.find_first_of(":\n") != std::string::npos)
        return "Not a password that can be set";
    if (!strings::isValidUtf8(options.fullName))
        return "Not a valid full name";
    if (!installer::isValidHostName(options.hostName))
        return "Not a valid computer name: " + options.hostName;
    if (!installer::isValidTimeZone(options.timeZone))
        return "Not a time zone: " + options.timeZone;
    return {};
}

void InstallerService::start(const installer::Options& options)
{
    m_running = true;
    m_object->emitPropertiesChanged({"Running"});
    if (m_thread.joinable())
        m_thread.join();

    const cutefish::base::LoopHandle loop = cutefish::base::EventLoop::current()->handle();
    m_thread = std::thread([this, loop, paths = m_settings.paths, options] {
        InstallJob job(paths, options);
        job.onProgress = [this, loop](const std::string& stage, double fraction) {
            loop.post([this, stage, fraction] { m_object->emitSignal("Progress", stage, fraction); });
        };
        const std::string error = job.run();
        loop.post([this, error] { finished(error); });
    });
}

void InstallerService::finished(const std::string& error)
{
    if (m_thread.joinable())
        m_thread.join();
    m_running = false;
    if (!error.empty())
        CF_WARNING(kLog) << "cutefish-installer: installation failed:" << error;
    m_object->emitPropertiesChanged({"Running"});
    m_object->emitSignal("Finished", error.empty(), error);
    m_idleTimer.start();
}
