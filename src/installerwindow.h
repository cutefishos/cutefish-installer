// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "backend.h"

#include "base/signal.h"
#include "common/installer.h"
#include "ui/window.h"

#include <memory>

namespace cutefish::ui {
class Animation;
}

class Page;
class LanguagePage;
class WelcomePage;
class DiskPage;
class PartitionPage;
class AccountPage;
class TimeZonePage;
class SummaryPage;
class ProgressPage;
class ResultPage;

// The installer: the language, a welcome that lets the system be tried
// instead, the choices one page each, then the installation.
class InstallerWindow
{
public:
    enum class Step {
        Language,
        Welcome,
        Disk,
        Partition,
        Account,
        TimeZone,
        Summary,
        Progress,
        Result,
    };

    explicit InstallerWindow(Backend& backend);
    ~InstallerWindow();

    cutefish::ui::Window& window() { return m_window; }
    void activate();

    // Straight to the step, without the transition.
    void showStep(Step step);
    Step step() const { return m_step; }

    LanguagePage* languagePage() const { return m_language; }
    DiskPage* diskPage() const { return m_disk; }
    PartitionPage* partitionPage() const { return m_partition; }
    AccountPage* accountPage() const { return m_account; }
    TimeZonePage* timeZonePage() const { return m_timeZone; }
    SummaryPage* summaryPage() const { return m_summary; }
    ProgressPage* progressPage() const { return m_progress; }
    ResultPage* resultPage() const { return m_result; }

private:
    class Root;

    Page* page(Step step) const;
    // Every page but the language's, whose choice lasts.
    void buildPages();
    // The pages made again in the language now spoken, with what was chosen
    // and typed on them.
    void retranslate();
    void go(Step step);
    void updateButtons();
    void primaryClicked();
    void backClicked();
    void otherClicked();
    void languageChosen();
    void fillSummary();
    void loadDisks();
    void loadLayout();
    void install();
    void finished(bool success, const std::string& error);
    void restart();

    Backend& m_backend;
    cutefish::ui::Window m_window;
    Root* m_root = nullptr;
    LanguagePage* m_language = nullptr;
    WelcomePage* m_welcome = nullptr;
    DiskPage* m_disk = nullptr;
    PartitionPage* m_partition = nullptr;
    AccountPage* m_account = nullptr;
    TimeZonePage* m_timeZone = nullptr;
    SummaryPage* m_summary = nullptr;
    ProgressPage* m_progress = nullptr;
    ResultPage* m_result = nullptr;
    Step m_step = Step::Language;
    // The partitions are chosen by hand instead of erasing the disk.
    bool m_manual = false;
    std::unique_ptr<cutefish::ui::Animation> m_transition;
    Page* m_leaving = nullptr;
    bool m_succeeded = false;
    std::string m_error;
    cutefish::base::Subscription m_languageChanged;
};
