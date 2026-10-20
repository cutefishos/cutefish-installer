// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

// Renders every step headless, with made-up disks and choices:
//   installer-capture DIR DATA_DIR [WIDTH HEIGHT SCALE [dark]]
#include "backend.h"
#include "components.h"
#include "installerwindow.h"
#include "pages.h"
#include "partitionpage.h"

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
    const TimeZone singapore {"Asia/Singapore", "Singapore", "Singapore", "UTC+08:00"};
    window.summaryPage()->setSummary(options, *window.diskPage()->selectedDisk(), singapore, nullptr);
    window.showStep(InstallerWindow::Step::Summary);
    save("5-summary");

    // A disk shared with Windows: its EFI system partition kept, the free
    // space for the system and swap.
    constexpr uint64_t kGB = 1'000'000'000;
    installer::DiskLayout layout;
    layout.disk = "/dev/nvme0n1";
    layout.size = 512110190592;
    layout.table = "gpt";
    layout.partitions = {
            {1, "/dev/nvme0n1p1", installer::kMiB, 100 * installer::kMiB, "c12a7328-f81f-11d2-ba4b-00a0c93ec93b",
             "vfat", "SYSTEM"},
            {2, "/dev/nvme0n1p2", 101 * installer::kMiB, 16 * installer::kMiB, "e3c9e316-0b5c-4db8-817d-f92df00215ae",
             "", ""},
            {3, "/dev/nvme0n1p3", 117 * installer::kMiB, 250 * kGB, "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7", "ntfs",
             "Windows"},
    };
    PartitionPage* partitions = window.partitionPage();
    partitions->setLoading("YMTC PC005 512GB");
    window.showStep(InstallerWindow::Step::Partition);
    save("3-partition-loading");
    partitions->setLayout(layout, {"ext4", "btrfs", "xfs", "vfat", "swap"});
    partitions->select(3);
    save("3-partition");
    window.showStep(InstallerWindow::Step::Partition);
    installer::PartitionPlan plan(layout);
    plan.assign(0, "/boot/efi", false, {});
    const installer::PartitionPlan::Region free = plan.freeRegions().back();
    plan.add(free.start, free.size - 16 * kGB, "/", "ext4");
    plan.add(plan.freeRegions().back().start, 16 * kGB, "swap", "swap");
    partitions->setLayout(layout, {"ext4", "btrfs", "xfs", "vfat", "swap"});
    // Replays the plan through the page, as the dialogs would.
    const auto replay = installer::PartitionPlan::fromMap(plan.toMap(), layout);
    if (replay)
        partitions->setPlanForCapture(*replay);
    partitions->select(4);
    save("3-partition-planned");
    window.summaryPage()->setSummary(options, *window.diskPage()->selectedDisk(), singapore, &partitions->plan());
    window.showStep(InstallerWindow::Step::Summary);
    save("5-summary-manual");

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
