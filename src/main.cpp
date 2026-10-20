// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "backend.h"
#include "installerwindow.h"

#include "base/command_line.h"
#include "base/dbus.h"
#include "base/i18n.h"
#include "ui/application.h"

#include <chrono>
#include <filesystem>
#include <memory>

using namespace cutefish;

static const char* const kService = "org.cutefish.InstallerWindow";
static const char* const kPath = "/InstallerWindow";

int main(int argc, char** argv)
{
    base::CommandLineParser parser;
    parser.setApplicationDescription("Installs Cutefish from the live system.");
    parser.setApplicationVersion(INSTALLER_VERSION);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"autostart", "Started with the session: only on a live system."});
    parser.process(argc, argv);
    if (parser.isSet("autostart") && !std::filesystem::exists("/run/live/medium"))
        return 0;

    ui::Application app(argc, argv);
    app.setApplicationId("cutefish-installer");
    i18n::setDomain("cutefish-installer");

    base::dbus::Connection* bus = app.sessionBus();
    if (bus && !bus->requestName(kService)) {
        base::dbus::Proxy running(bus, kService, kPath, kService);
        running.setTimeout(std::chrono::milliseconds(1500));
        running.call("activateWindow");
        return 0;
    }

    Backend backend(base::dbus::Connection::system());
    InstallerWindow window(backend);

    std::unique_ptr<base::dbus::Object> service;
    if (bus) {
        service = std::make_unique<base::dbus::Object>(bus, kPath, kService);
        service->addMethod("activateWindow", [&window] { window.activate(); });
        service->publish();
    }

    window.window().show();
    return app.run();
}
