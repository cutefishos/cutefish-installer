// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "installerwindow.h"

#include "components.h"
#include "pages.h"
#include "partitionpage.h"

#include "base/i18n.h"
#include "base/logging.h"
#include "ui/animation.h"
#include "ui/application.h"
#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/image_view.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

using namespace cutefish::ui;
using cutefish::i18n::tr;
namespace dbus = cutefish::base::dbus;

static const cutefish::base::LogCategory kLog("cutefish.installer");

static constexpr double kButtonHeight = 36;
static constexpr double kButtonMinimumWidth = 128;
static constexpr double kButtonSpacing = 12;
// From the bottom of the panel to the buttons.
static constexpr double kButtonBottom = 32;
// The least space around the panel on a small screen.
static constexpr double kPanelMargin = 24;
static constexpr char kWallpaper[] = "/usr/share/backgrounds/cutefishos/default.jpg";
// How far a page slides in or out.
static constexpr double kSlide = 40;

class InstallerWindow::Root : public View
{
public:
    Root()
    {
        wallpaper = addChild<ImageView>(kWallpaper);
        wallpaper->setFillMode(ImageView::FillMode::PreserveAspectCrop);
        shadow = addChild<PanelShadow>();
        panel = addChild<Panel>();
        pages = panel->addChild<View>();
        secondary = panel->addChild<DialogButton>();
        primary = panel->addChild<DialogButton>(std::string(), true);
    }

    ImageView* wallpaper;
    PanelShadow* shadow;
    Panel* panel;
    View* pages;
    DialogButton* secondary;
    DialogButton* primary;

protected:
    void layout() override
    {
        const RectF bounds = localRect();
        wallpaper->setGeometry(bounds);
        const double panelWidth = std::min(kPanelWidth, std::max(0.0, bounds.width - 2 * kPanelMargin));
        const double panelHeight = std::min(kPanelHeight, std::max(0.0, bounds.height - 2 * kPanelMargin));
        const RectF panelRect {rounding::centeredStart(bounds.width, panelWidth),
                               rounding::centeredStart(bounds.height, panelHeight), panelWidth, panelHeight};
        panel->setGeometry(panelRect);
        const double reach = PanelShadow::kReach;
        shadow->setGeometry(panelRect.adjusted(-reach, -reach, reach, reach));

        const RectF inside = panel->localRect();
        pages->setGeometry(inside);
        for (const auto& page : pages->children())
            page->setGeometry(pages->localRect());

        std::vector<DialogButton*> shown;
        for (DialogButton* button : {secondary, primary}) {
            if (button->isVisible())
                shown.push_back(button);
        }
        double total = 0;
        std::vector<double> widths;
        for (DialogButton* button : shown) {
            widths.push_back(std::max(kButtonMinimumWidth, button->sizeHint().width));
            total += widths.back();
        }
        if (shown.size() > 1)
            total += kButtonSpacing * double(shown.size() - 1);
        double x = rounding::centeredStart(inside.width, total);
        const double y = inside.height - kButtonBottom - kButtonHeight;
        for (size_t i = 0; i < shown.size(); ++i) {
            shown[i]->setGeometry({x, y, widths[i], kButtonHeight});
            x += widths[i] + kButtonSpacing;
        }
    }
};

InstallerWindow::InstallerWindow(Backend& backend)
    : m_backend(backend)
{
    m_window.setTitle(tr("Install Cutefish"));
    m_window.setMinimumSize({800, 600});
    m_window.resize({1024, 768});
    m_window.setFullScreen(true);

    auto root = std::make_unique<Root>();
    m_root = root.get();
    m_welcome = m_root->pages->addChild<WelcomePage>();
    m_disk = m_root->pages->addChild<DiskPage>();
    m_partition = m_root->pages->addChild<PartitionPage>(std::filesystem::exists("/sys/firmware/efi"));
    m_partition->setWindow(&m_window);
    m_account = m_root->pages->addChild<AccountPage>();
    m_timeZone = m_root->pages->addChild<TimeZonePage>();
    m_summary = m_root->pages->addChild<SummaryPage>();
    m_progress = m_root->pages->addChild<ProgressPage>();
    m_result = m_root->pages->addChild<ResultPage>();
    m_window.setContentView(std::move(root));

    m_root->primary->onClicked = [this] { primaryClicked(); };
    m_root->secondary->onClicked = [this] { secondaryClicked(); };
    m_disk->onSelectionChanged = [this] { updateButtons(); };
    m_partition->onChanged = [this] { updateButtons(); };
    m_account->onChanged = [this] { updateButtons(); };
    m_account->onAccepted = [this] { primaryClicked(); };
    m_timeZone->onSelectionChanged = [this] { updateButtons(); };

    m_backend.onProgress = [this](const std::string& stage, double fraction) {
        m_progress->setProgress(stage, fraction);
    };
    m_backend.onFinished = [this](bool success, const std::string& error) { finished(success, error); };

    // Stopping half way would leave the disk unusable.
    m_window.onCloseRequested = [this] {
        if (!m_backend.isRunning())
            Application::instance()->quit();
    };

    m_transition = std::make_unique<Animation>(&m_window);
    m_transition->setDuration(std::chrono::milliseconds(300));
    m_transition->setEasing(Animation::Easing::OutCubic);

    showStep(Step::Welcome);
}

InstallerWindow::~InstallerWindow() = default;

void InstallerWindow::activate()
{
    m_window.show();
    m_window.activate();
}

Page* InstallerWindow::page(Step step) const
{
    switch (step) {
    case Step::Welcome:
        return m_welcome;
    case Step::Disk:
        return m_disk;
    case Step::Partition:
        return m_partition;
    case Step::Account:
        return m_account;
    case Step::TimeZone:
        return m_timeZone;
    case Step::Summary:
        return m_summary;
    case Step::Progress:
        return m_progress;
    case Step::Result:
        return m_result;
    }
    return m_welcome;
}

void InstallerWindow::showStep(Step step)
{
    m_transition->stop();
    m_leaving = nullptr;
    m_step = step;
    for (const auto& child : m_root->pages->children()) {
        child->setVisible(child.get() == page(step));
        child->setOpacity(1);
        child->setTransform({});
    }
    updateButtons();
}

void InstallerWindow::go(Step step)
{
    if (step == m_step)
        return;
    if (m_transition->isRunning())
        showStep(m_step);

    const double direction = int(step) > int(m_step) ? 1 : -1;
    Page* leaving = page(m_step);
    Page* entering = page(step);
    m_step = step;
    m_leaving = leaving;
    entering->setVisible(true);
    entering->setOpacity(0);
    entering->setTransform({{direction * kSlide, 0}, 1});
    updateButtons();

    m_transition->onUpdate = [leaving, entering, direction](double value) {
        leaving->setOpacity(1 - value);
        leaving->setTransform({{-direction * kSlide * value, 0}, 1});
        entering->setOpacity(value);
        entering->setTransform({{direction * kSlide * (1 - value), 0}, 1});
    };
    m_transition->onFinished = [this, leaving, entering] {
        leaving->setVisible(false);
        leaving->setOpacity(1);
        leaving->setTransform({});
        entering->setOpacity(1);
        entering->setTransform({});
        m_leaving = nullptr;
        if (entering == m_account)
            m_account->focusFirstField();
    };
    m_transition->start();

    if (step == Step::Disk)
        loadDisks();
    if (step == Step::Partition)
        loadLayout();
    if (step == Step::TimeZone)
        m_timeZone->scrollToSelection();
}

void InstallerWindow::updateButtons()
{
    DialogButton* primary = m_root->primary;
    DialogButton* secondary = m_root->secondary;
    primary->setVisible(true);
    secondary->setVisible(true);
    primary->setDestructive(false);
    primary->setEnabled(true);
    secondary->setText(tr("Back"));
    primary->setText(tr("Continue"));

    switch (m_step) {
    case Step::Welcome:
        secondary->setText(tr("Try Cutefish"));
        primary->setText(tr("Next"));
        break;
    case Step::Disk:
        primary->setEnabled(m_disk->selectedDisk());
        break;
    case Step::Partition:
        primary->setEnabled(m_partition->isComplete());
        break;
    case Step::Account:
        primary->setEnabled(m_account->isComplete());
        break;
    case Step::TimeZone:
        primary->setEnabled(m_timeZone->selectedZone());
        break;
    case Step::Summary:
        primary->setText(isManual() ? tr("Install") : tr("Erase Disk and Install"));
        primary->setDestructive(m_summary->erases());
        break;
    case Step::Progress:
        primary->setVisible(false);
        secondary->setVisible(false);
        break;
    case Step::Result:
        secondary->setText(m_succeeded ? tr("Keep Trying") : tr("Close"));
        primary->setText(m_succeeded ? tr("Restart Now") : tr("Try Again"));
        break;
    }
    m_root->requestLayout();
}

void InstallerWindow::primaryClicked()
{
    if (!m_root->primary->isEnabled() || !m_root->primary->isVisible())
        return;
    switch (m_step) {
    case Step::Welcome:
        go(Step::Disk);
        break;
    case Step::Disk:
        go(isManual() ? Step::Partition : Step::Account);
        break;
    case Step::Partition:
        go(Step::Account);
        break;
    case Step::Account:
        go(Step::TimeZone);
        break;
    case Step::TimeZone: {
        installer::Options options;
        m_account->fill(options);
        m_summary->setSummary(options, *m_disk->selectedDisk(), *m_timeZone->selectedZone(),
                              isManual() ? &m_partition->plan() : nullptr);
        go(Step::Summary);
        break;
    }
    case Step::Summary:
        install();
        break;
    case Step::Progress:
        break;
    case Step::Result:
        if (m_succeeded)
            restart();
        else
            go(Step::Summary);
        break;
    }
}

void InstallerWindow::secondaryClicked()
{
    switch (m_step) {
    case Step::Welcome:
    case Step::Result:
        Application::instance()->quit();
        break;
    case Step::Disk:
        go(Step::Welcome);
        break;
    case Step::Partition:
        go(Step::Disk);
        break;
    case Step::Account:
        go(isManual() ? Step::Partition : Step::Disk);
        break;
    case Step::TimeZone:
        go(Step::Account);
        break;
    case Step::Summary:
        go(Step::TimeZone);
        break;
    case Step::Progress:
        break;
    }
}

void InstallerWindow::loadDisks()
{
    // Listed again each time, for a disk plugged in since; the choice stays.
    const std::string chosen = m_disk->selectedDisk() ? m_disk->selectedDisk()->path : std::string();
    m_disk->setLoading();
    updateButtons();
    m_backend.loadDisks([this, chosen](std::vector<DiskInfo> disks, std::string error) {
        if (!error.empty())
            m_disk->setError(error);
        else
            m_disk->setDisks(std::move(disks), chosen);
        updateButtons();
    });
}

bool InstallerWindow::isManual() const
{
    return m_disk->isManual();
}

void InstallerWindow::loadLayout()
{
    const DiskInfo* disk = m_disk->selectedDisk();
    // Read again each time: the disk may have changed since.
    m_partition->setLoading(disk->model.empty() ? disk->path : disk->model);
    updateButtons();
    m_backend.loadLayout(disk->path, [this](installer::DiskLayout layout, std::vector<std::string> fileSystems,
                                            std::string error) {
        if (!error.empty())
            m_partition->setError(error);
        else
            m_partition->setLayout(std::move(layout), std::move(fileSystems));
        updateButtons();
    });
}

void InstallerWindow::install()
{
    installer::Options options;
    options.disk = m_disk->selectedDisk()->path;
    if (isManual())
        options.partitioning = m_partition->plan().toMap();
    m_account->fill(options);
    options.timeZone = m_timeZone->selectedZone()->id;

    m_progress->setProgress(installer::kStagePrepare, 0);
    go(Step::Progress);
    m_backend.install(options, [this](std::string error) {
        if (!error.empty())
            finished(false, error);
    });
}

void InstallerWindow::finished(bool success, const std::string& error)
{
    m_succeeded = success;
    if (success)
        m_result->setSucceeded();
    else
        m_result->setFailed(error);
    go(Step::Result);
}

void InstallerWindow::restart()
{
    dbus::Connection* bus = Application::instance()->sessionBus();
    dbus::Proxy session(bus, "org.cutefish.Session", "/Session", "org.cutefish.Session");
    if (!bus || !session.send("reboot"))
        CF_WARNING(kLog) << "cutefish-installer: cannot ask the session to restart";
}

bool InstallerWindow::isBackgroundLoaded() const
{
    const ImageView::Status status = m_root->wallpaper->status();
    return status == ImageView::Status::Ready || status == ImageView::Status::Error;
}
