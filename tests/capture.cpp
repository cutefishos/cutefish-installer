// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

// Renders every step headless, with made-up disks and choices:
//   installer-capture DIR DATA_DIR [SCALE [dark]]
// and prints each text that does not fit its place, for a run in another
// language; the exit status is 2 when one does not.
#include "backend.h"
#include "components.h"
#include "installerwindow.h"
#include "pages.h"
#include "partitionpage.h"

#include "base/i18n.h"
#include "ui/application.h"
#include "ui/check_box.h"
#include "ui/combo_box.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace cutefish;

static int s_problems = 0;

static void report(const std::string& state, const std::string& problem)
{
    std::printf("%s: %s\n", state.c_str(), problem.c_str());
    ++s_problems;
}

static std::string describe(const ui::RectF& rect)
{
    char text[96];
    std::snprintf(text, sizeof text, "%.1f,%.1f %.1fx%.1f", rect.x, rect.y, rect.width, rect.height);
    return text;
}

// Texts wider or taller than their place, controls narrower than they ask
// for, and either outside their parent.
static void checkFit(const std::string& state, const ui::View* view)
{
    if (!view->isVisible())
        return;
    const ui::RectF slot = view->geometry();
    const ui::View* parent = view->parent();
    const auto outside = [&] {
        return parent && (slot.x < -0.5 || slot.y < -0.5 || slot.x + slot.width > parent->geometry().width + 0.5
                          || slot.y + slot.height > parent->geometry().height + 0.5);
    };
    if (const auto* label = dynamic_cast<const ui::Label*>(view); label && !label->text().empty()) {
        const ui::SizeF natural = label->sizeHint();
        const double height = label->heightForWidth(slot.width);
        const bool oneLine = height <= natural.height + 0.5;
        if (height > slot.height + 0.5 || (oneLine && natural.width > slot.width + 0.5) || label->isElided())
            report(state, "\"" + label->text() + "\" needs " + std::to_string(int(std::ceil(natural.width))) + "x"
                           + std::to_string(int(std::ceil(height))) + " in " + describe(slot));
        else if (outside())
            report(state, "\"" + label->text() + "\" outside its parent at " + describe(slot));
        return;
    }
    const bool control = dynamic_cast<const ui::DialogButton*>(view) || dynamic_cast<const ui::CheckBox*>(view)
            || dynamic_cast<const ui::ComboBox*>(view);
    if (control && view->sizeHint().width > slot.width + 0.5)
        report(state, std::string("control needs ") + std::to_string(int(std::ceil(view->sizeHint().width)))
                       + " in " + describe(slot));
    if (control && outside())
        report(state, "control outside its parent at " + describe(slot));
    if (const auto* page = dynamic_cast<const Page*>(view)) {
        const ui::RectF content = page->content()->geometry();
        if (content.y + content.height > slot.height - Page::kFooterHeight + 0.5)
            report(state, "content reaches into the buttons, bottom " + std::to_string(content.y + content.height));
    }
    for (const auto& child : view->children())
        checkFit(state, child.get());
}

static void checkWindow(const std::string& state, ui::Window& window)
{
    window.grab();
    checkFit(state, window.contentView());
}

// A message shown in a place of fixed width and lines, set one at a time.
static void checkMessages(const std::string& state, const std::vector<std::string>& messages, double size,
                          double width, int lines)
{
    ui::Label label;
    label.setFontSize(size);
    label.setWrap(true);
    for (const std::string& message : messages) {
        label.setText(message);
        // Lines of another script are of another height.
        const double line = label.sizeHint().height;
        if (label.heightForWidth(width) > lines * line + 0.5)
            report(state, "\"" + message + "\" takes more than " + std::to_string(lines) + " lines");
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s DIR DATA_DIR [SCALE [dark]]\n", argv[0]);
        return 1;
    }
    const std::string directory = argv[1];
    setDataDirectory(argv[2]);
    const double scale = argc > 3 ? std::atof(argv[3]) : 1.5;
    const bool dark = argc > 4 && std::string(argv[4]) == "dark";
    std::filesystem::create_directories(directory);

    ui::Application app(argc, argv, ui::Application::Platform::Headless);
    i18n::setDomain("cutefish-installer");
    ui::Theme::setCurrent(dark ? ui::Theme::dark() : ui::Theme::light());
    Backend backend(nullptr);
    InstallerWindow window(backend);
    window.window().setScale(scale);
    const ui::SizeF size = window.window().size();
    if (size.width != kWindowWidth || size.height != kWindowHeight)
        report("window", "size " + std::to_string(size.width) + "x" + std::to_string(size.height));

    const auto save = [&](const std::string& name) {
        window.window().grab().savePng(directory + "/" + name + ".png");
        std::ofstream(directory + "/" + name + ".txt") << window.window().layoutDump();
        checkFit(name, window.window().contentView());
    };

    save("0-language");
    window.showStep(InstallerWindow::Step::Welcome);
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
    // Two of one model, told apart by their paths.
    window.diskPage()->setDisks({
            {"/dev/nvme0n1", "Samsung SSD 990 PRO 2TB", 2000398934016, false},
            {"/dev/nvme1n1", "Samsung SSD 990 PRO 2TB", 2000398934016, false},
    });
    save("2-disk-same");
    // More than stand side by side.
    std::vector<DiskInfo> many;
    for (int i = 0; i < 7; ++i)
        many.push_back({"/dev/sd" + std::string(1, char('a' + i)), "WDC WD40EFAX-68JH4N" + std::to_string(i),
                        4000787030016, false});
    window.diskPage()->setDisks(many, "/dev/sdb");
    save("2-disk-many");
    window.diskPage()->setDisks({});
    save("2-disk-none");
    window.diskPage()->setDisks({{"/dev/nvme0n1", "YMTC PC005 512GB", 512110190592, false}});

    window.showStep(InstallerWindow::Step::Account);
    save("3-account-empty");
    window.showStep(InstallerWindow::Step::TimeZone);
    window.timeZonePage()->scrollToSelection();
    save("4-timezone");
    window.timeZonePage()->setQuery("san");
    save("4-timezone-search");
    window.timeZonePage()->setQuery("nowhere at all");
    save("4-timezone-none");
    window.timeZonePage()->setQuery({});

    installer::Options options {"/dev/nvme0n1", "Reion Wong", "reion", "secret", "reion-pc", "Asia/Singapore"};
    installer::Options typed;
    window.accountPage()->fill(typed);
    options.picture = typed.picture;
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
    for (size_t i = 0; i < partitions->entries().size(); ++i) {
        partitions->select(int(i));
        const bool free = partitions->entries()[i].partition < 0;
        ui::Window* dialog = free ? partitions->openAddDialogForCapture() : partitions->openEditDialogForCapture();
        checkWindow(std::string(free ? "3-dialog-add-" : "3-dialog-edit-") + std::to_string(i), *dialog);
    }
    checkWindow("3-dialog-new-table", *partitions->openNewTableDialogForCapture());
    partitions->select(3);
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
    // An existing partition formatted, which the table marks.
    installer::PartitionPlan formatted = *replay;
    formatted.assign(2, "/home", true, "ext4");
    partitions->setPlanForCapture(formatted);
    save("3-partition-format");
    partitions->setPlanForCapture(*replay);
    for (size_t i = 0; i < partitions->entries().size(); ++i) {
        partitions->select(int(i));
        if (partitions->entries()[i].partition >= 0)
            checkWindow("3-dialog-planned-" + std::to_string(i), *partitions->openEditDialogForCapture());
    }
    window.summaryPage()->setSummary(options, *window.diskPage()->selectedDisk(), singapore, &partitions->plan());
    window.showStep(InstallerWindow::Step::Summary);
    save("5-summary-manual");

    // The longest values a summary row may hold: a disk model of 40
    // characters, a long full name, a computer name of 63 and a long zone.
    const DiskInfo longDisk {"/dev/nvme0n1", "Samsung SSD 990 PRO with Heatsink 4TB NV", 4000787030016, false};
    const TimeZone longZone {"America/Port_of_Spain", "Port of Spain", "Trinidad and Tobago", "UTC-04:00"};
    installer::Options longOptions {"/dev/nvme0n1", "Maximilian Alexander Ferdinand von Habsburg", "maximilian",
                                    "secret", std::string(60, 'm') + "-pc", "America/Port_of_Spain"};
    longOptions.picture = options.picture;
    window.summaryPage()->setSummary(longOptions, longDisk, longZone, nullptr);
    save("5-summary-long");
    window.summaryPage()->setSummary(longOptions, longDisk, longZone, &partitions->plan());
    save("5-summary-long-manual");

    window.progressPage()->setProgress(installer::kStageCopy, 0.42);
    window.showStep(InstallerWindow::Step::Progress);
    save("6-progress");

    window.resultPage()->setSucceeded();
    window.showStep(InstallerWindow::Step::Result);
    save("7-done");
    window.resultPage()->setFailed("grub-install failed with exit code 1:\n"
                                   "grub-install: error: cannot find EFI directory.");
    save("7-failed");

    using i18n::format;
    using i18n::tr;
    // The buttons the steps show along the bottom, one at the left and the
    // others at the right: each at least 112 wide, 12 apart, with the
    // window's 32 at both ends.
    const std::vector<std::vector<std::string>> rows {
            {tr("Continue")},
            {tr("Try Cutefish"), tr("Back"), tr("Continue")},
            {tr("Partition Manually"), tr("Back"), tr("Continue")},
            {tr("Back"), tr("Install")},
            {tr("Back"), tr("Erase Disk and Install")},
            {tr("Keep Trying"), tr("Restart Now")},
            {tr("Close"), tr("Try Again")},
    };
    for (const std::vector<std::string>& row : rows) {
        double total = 12 * double(row.size() - 1);
        for (const std::string& text : row)
            total += std::max(112.0, ui::DialogButton(text).sizeHint().width);
        if (total > kWindowWidth - 2 * kWindowMargin)
            report("buttons", "\"" + row.back() + "\" row needs " + std::to_string(int(total)));
    }
    // Under the account form, two lines kept for them.
    checkMessages("3-account-message",
                  {tr("A user name starts with a lower-case letter and uses only a–z, 0–9, “-” and “_”."),
                   format(tr("“{name}” is taken by the system. Choose another user name."), {{"name", "messagebus"}}),
                   tr("A computer name uses only letters, digits and hyphens, and does not start or end with a hyphen."),
                   tr("The passwords do not match.")},
                  12, kContentWidth, 2);
    // Under the partition table, one line kept for them.
    checkMessages("3-partition-message",
                  {tr("The disk has no partition table. Create one first."),
                   tr("A disk with an MBR partition table holds at most four partitions."),
                   format(tr("“{mount}” is not a mount point the system can use."), {{"mount", "/srv/data"}}),
                   format(tr("Two partitions are given “{mount}”."), {{"mount", "/boot/efi"}}),
                   tr("Only a disk with a GPT partition table needs a BIOS boot partition."),
                   tr("Only a new partition, or one made for it, can be the BIOS boot partition."),
                   format(tr("The partition for “{mount}” must be formatted."), {{"mount", "/boot/efi"}}),
                   format(tr("“{type}” cannot be used for “{mount}”."), {{"type", "btrfs"}, {"mount", "/boot/efi"}}),
                   format(tr("The partition for “{mount}” holds no file system; format it to use it there."),
                          {{"mount", "/boot/efi"}}),
                   format(tr("The partition for “{mount}” holds “{type}”; format it to use it there."),
                          {{"mount", "/boot/efi"}, {"type", "ntfs"}}),
                   format(tr("Choose a partition for the system: give one the mount point “{mount}”."), {{"mount", "/"}}),
                   format(tr("The partition for “{mount}” needs at least {size} GB."), {{"mount", "/"}, {"size", "12"}}),
                   format(tr("Choose an EFI system partition: give one the mount point “{mount}”."),
                          {{"mount", "/boot/efi"}}),
                   tr("The EFI system partition needs at least 32 MB."),
                   tr("Add a 1 MB partition used as BIOS boot, for the boot loader.")},
                  12, 720, 1);
    return s_problems > 0 ? 2 : 0;
}
