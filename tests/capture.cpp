// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

// Renders every step headless, with made-up disks and choices:
//   installer-capture DIR DATA_DIR [WIDTH HEIGHT SCALE [dark]]
#include "backend.h"
#include "components.h"
#include "installerwindow.h"
#include "pages.h"

#include "base/timer.h"
#include "ui/application.h"
#include "ui/theme.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace cutefish;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s DIR DATA_DIR [WIDTH HEIGHT SCALE [dark]]\n", argv[0]);
        return 1;
    }
    const std::string directory = argv[1];
    setDataDirectory(argv[2]);
    const double width = argc > 3 ? std::atof(argv[3]) : 1440;
    const double height = argc > 4 ? std::atof(argv[4]) : 960;
    const double scale = argc > 5 ? std::atof(argv[5]) : 1.5;
    const bool dark = argc > 6 && std::string(argv[6]) == "dark";
    std::filesystem::create_directories(directory);

    ui::Application app(argc, argv, ui::Application::Platform::Headless);
    ui::Theme::setCurrent(dark ? ui::Theme::dark() : ui::Theme::light());
    Backend backend(nullptr);
    InstallerWindow window(backend);
    window.window().setScale(scale);
    window.window().resize({width, height});

    // The wallpaper decodes on a thread once the window is first drawn.
    window.window().grab();
    base::Timer poll;
    poll.onTimeout = [&] {
        window.window().grab();
        if (window.isBackgroundLoaded())
            app.quit();
    };
    poll.start(std::chrono::milliseconds(20));
    app.run();
    poll.stop();

    const auto save = [&](const std::string& name) {
        window.window().grab().savePng(directory + "/" + name + ".png");
        std::ofstream(directory + "/" + name + ".txt") << window.window().layoutDump();
    };

    save("1-welcome");
    window.showStep(InstallerWindow::Step::Disk);
    window.diskPage()->setLoading();
    save("2-disk-loading");
    window.diskPage()->setDisks({
            {"/dev/nvme0n1", "YMTC PC005 512GB", 512110190592, false},
            {"/dev/sda", "SanDisk Ultra", 61505273856, true},
            {"/dev/sdb", "Generic Flash Disk", 8053063680, true},
    });
    save("2-disk");
    window.diskPage()->setDisks({});
    save("2-disk-none");
    window.diskPage()->setDisks({{"/dev/nvme0n1", "YMTC PC005 512GB", 512110190592, false}});

    window.showStep(InstallerWindow::Step::Account);
    save("3-account-empty");
    window.showStep(InstallerWindow::Step::TimeZone);
    window.timeZonePage()->scrollToSelection();
    save("4-timezone");

    installer::Options options {"/dev/nvme0n1", "Reion Wong", "reion", "secret", "reion-pc", "Asia/Singapore"};
    window.summaryPage()->setSummary(options, *window.diskPage()->selectedDisk(),
                                     {"Asia/Singapore", "Singapore", "Singapore", "UTC+08:00"});
    window.showStep(InstallerWindow::Step::Summary);
    save("5-summary");

    window.progressPage()->setProgress(installer::kStageCopy, 0.42);
    window.showStep(InstallerWindow::Step::Progress);
    save("6-progress");

    window.resultPage()->setSucceeded();
    window.showStep(InstallerWindow::Step::Result);
    save("7-done");
    window.resultPage()->setFailed("grub-install failed with exit code 1:\n"
                                   "grub-install: error: cannot find EFI directory.");
    save("7-failed");
    return 0;
}
