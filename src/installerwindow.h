// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "backend.h"

#include "common/installer.h"
#include "ui/window.h"

#include <memory>

namespace cutefish::ui {
class Animation;
}

class Page;
class WelcomePage;
class DiskPage;
class AccountPage;
class TimeZonePage;
class SummaryPage;
class ProgressPage;
class ResultPage;

// The installer, full screen: a welcome that lets the system be tried
// instead, the choices one page each, then the installation.
class InstallerWindow
{
public:
    enum class Step {
        Welcome,
        Disk,
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

    DiskPage* diskPage() const { return m_disk; }
    AccountPage* accountPage() const { return m_account; }
    TimeZonePage* timeZonePage() const { return m_timeZone; }
    SummaryPage* summaryPage() const { return m_summary; }
    ProgressPage* progressPage() const { return m_progress; }
    ResultPage* resultPage() const { return m_result; }
    // The wallpaper behind the panel is decoded, or failed to be.
    bool isBackgroundLoaded() const;

private:
    class Root;

    Page* page(Step step) const;
    void go(Step step);
    void updateButtons();
    void primaryClicked();
    void secondaryClicked();
    void loadDisks();
    void install();
    void finished(bool success, const std::string& error);
    void restart();

    Backend& m_backend;
    cutefish::ui::Window m_window;
    Root* m_root = nullptr;
    WelcomePage* m_welcome = nullptr;
    DiskPage* m_disk = nullptr;
    AccountPage* m_account = nullptr;
    TimeZonePage* m_timeZone = nullptr;
    SummaryPage* m_summary = nullptr;
    ProgressPage* m_progress = nullptr;
    ResultPage* m_result = nullptr;
    Step m_step = Step::Welcome;
    std::unique_ptr<cutefish::ui::Animation> m_transition;
    Page* m_leaving = nullptr;
    bool m_succeeded = false;
};
