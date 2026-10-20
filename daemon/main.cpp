// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "installerservice.h"

#include "base/command_line.h"
#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/logging.h"

int main(int argc, char *argv[])
{
    cutefish::base::CommandLineParser parser;
    parser.setApplicationDescription("Installs the live system on a disk, for cutefish-installer.");
    parser.addHelpOption();
    parser.addOption({"squashfs", "The live system image.", "file"});
    parser.addOption({"medium", "The live medium, with the boot loader packages.", "directory"});
    parser.addOption({"test-disk", "Offer only this disk, such as a loop device; for tests.", "device"});
    parser.addOption({"no-nvram", "Leave the firmware's boot entries alone."});
    parser.process(argc, argv);

    cutefish::base::EventLoop loop;
    static const cutefish::base::LogCategory log("cutefish.installer");

    InstallerService::Settings settings;
    if (parser.isSet("squashfs"))
        settings.paths.squashfs = parser.value("squashfs");
    if (parser.isSet("medium"))
        settings.paths.medium = parser.value("medium");
    settings.testDisks = parser.values("test-disk");
    settings.paths.nvram = !parser.isSet("no-nvram");

    InstallerService service(std::move(settings));

    cutefish::base::dbus::Connection *bus = cutefish::base::dbus::Connection::system(&loop);
    if (!bus || !service.exportTo(bus) || !bus->requestName(installer::kService)) {
        CF_WARNING(log) << "cutefish-installer-service: cannot register on the system bus";
        return 1;
    }

    service.onIdle = [&loop] {
        loop.quit();
    };

    return loop.run();
}
