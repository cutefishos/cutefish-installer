// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "backend.h"
#include "components.h"
#include "installerwindow.h"
#include "pages.h"

#include "ui/application.h"
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
    EXPECT_TRUE(window.accountPage()->isComplete());
    EXPECT_EQ(window.timeZonePage()->selectedZone() ? window.timeZonePage()->selectedZone()->id : "", zone);
    app.setLanguageEnvironment({{"LANGUAGE", ""}});

    // A summary value too long for its line wraps instead of being cut.
    SummaryRow row("Disk", "Short");
    EXPECT_EQ(row.heightForWidth(kContentWidth), SummaryRow::kHeight);
    row.setValue("Partitioned manually, Samsung SSD 990 PRO with Heatsink 4TB NV (4.0 TB)");
    EXPECT_GT(row.heightForWidth(kContentWidth), SummaryRow::kHeight);
}
