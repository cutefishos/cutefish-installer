// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "backend.h"
#include "components.h"
#include "installerwindow.h"
#include "pages.h"

#include "ui/application.h"
#include "ui/test_input.h"
#include "ui/text_field.h"

#include <gtest/gtest.h>

using namespace cutefish;

static void findFields(ui::View* view, std::vector<ui::TextField*>& fields)
{
    if (auto* field = dynamic_cast<ui::TextField*>(view))
        fields.push_back(field);
    for (const auto& child : view->children())
        findFields(child.get(), fields);
}

TEST(InstallerWindow, languages)
{
    int argc = 1;
    char name[] = "installer-tests";
    char* argv[] = {name, nullptr};
    ui::Application app(argc, argv, ui::Application::Platform::Headless);
    Backend backend(nullptr);
    InstallerWindow window(backend);
    // A window of the pages' own size, not the screen's.
    EXPECT_EQ(window.window().size().width, kWindowWidth);
    EXPECT_EQ(window.window().size().height, kWindowHeight);
    EXPECT_FALSE(window.window().states().fullScreen);
    // The window has no header: its empty parts move it, its rows do not.
    window.window().show();
    window.window().grab();
    ui::TestInput::press(window.window(), {20, 20});
    ui::TestInput::release(window.window(), {20, 20});
    EXPECT_EQ(ui::TestInput::moveRequests(window.window()), 1);
    ui::TestInput::click(window.window(), {kWindowWidth / 2, 236});
    EXPECT_EQ(ui::TestInput::moveRequests(window.window()), 1);
    EXPECT_EQ(window.step(), InstallerWindow::Step::Language);
    ASSERT_TRUE(window.languagePage());
    EXPECT_EQ(window.languagePage()->selectedLanguage().code,
              installerLanguages()[installerLanguageFor(sessionLanguage())].code);

    // What was chosen and typed stays when the pages are made again.
    window.showStep(InstallerWindow::Step::Account);
    window.diskPage()->setDisks({{"/dev/sda", "Disk A", 64'000'000'000, false},
                                 {"/dev/sdb", "Disk B", 128'000'000'000, false}},
                                "/dev/sdb");
    std::vector<ui::TextField*> fields;
    findFields(window.accountPage(), fields);
    ASSERT_EQ(fields.size(), 5u);
    for (ui::TextField* field : fields)
        field->setText("ada");
    window.progressPage()->setProgress(installer::kStageCopy, 0.5);
    installer::Options before;
    window.accountPage()->fill(before);
    EXPECT_TRUE(before.picture.empty() || installer::isValidPictureName(before.picture));

    const LanguagePage* language = window.languagePage();
    const DiskPage* disk = window.diskPage();
    const std::string zone = window.timeZonePage()->selectedZone() ? window.timeZonePage()->selectedZone()->id : "";
    app.setLanguageEnvironment({{"LANGUAGE", "de"}});

    EXPECT_EQ(window.step(), InstallerWindow::Step::Account);
    EXPECT_EQ(window.languagePage(), language);
    EXPECT_NE(window.diskPage(), disk);
    ASSERT_TRUE(window.diskPage()->selectedDisk());
    EXPECT_EQ(window.diskPage()->selectedDisk()->path, "/dev/sdb");
    installer::Options options;
    window.accountPage()->fill(options);
    EXPECT_EQ(options.fullName, "ada");
    EXPECT_EQ(options.userName, "ada");
    EXPECT_EQ(options.password, "ada");
    EXPECT_EQ(options.picture, before.picture);
    EXPECT_TRUE(window.accountPage()->isComplete());
    EXPECT_EQ(window.timeZonePage()->selectedZone() ? window.timeZonePage()->selectedZone()->id : "", zone);
    app.setLanguageEnvironment({{"LANGUAGE", ""}});

    // Summary text too long for its line wraps instead of being cut.
    SummaryItem item;
    item.setText("Ada", "ada · ada-pc");
    EXPECT_EQ(item.heightForWidth(kContentWidth), SummaryItem::kHeight);
    item.setText("Ada", "ada · " + std::string(60, 'm') + "-pc");
    EXPECT_GT(item.heightForWidth(kContentWidth), SummaryItem::kHeight);

    // The button at the left of the disk page leads to the partitions, and
    // the way back from the account returns there.
    window.showStep(InstallerWindow::Step::Disk);
    window.window().grab();
    const ui::PointF other {kWindowMargin + 20, kWindowHeight - kWindowMargin - kButtonHeight / 2};
    const ui::PointF back {kWindowWidth - kWindowMargin - 112 - 12 - 20, other.y};
    const ui::PointF primary {kWindowWidth - kWindowMargin - 20, other.y};
    ui::TestInput::click(window.window(), other);
    EXPECT_EQ(window.step(), InstallerWindow::Step::Partition);
    window.showStep(InstallerWindow::Step::Account);
    window.window().grab();
    ui::TestInput::click(window.window(), back);
    EXPECT_EQ(window.step(), InstallerWindow::Step::Partition);
    // Continue on the disk page erases the disk instead.
    window.showStep(InstallerWindow::Step::Disk);
    window.window().grab();
    ui::TestInput::click(window.window(), primary);
    EXPECT_EQ(window.step(), InstallerWindow::Step::Account);
    window.showStep(InstallerWindow::Step::Account);
    window.window().grab();
    ui::TestInput::click(window.window(), back);
    EXPECT_EQ(window.step(), InstallerWindow::Step::Disk);
}
