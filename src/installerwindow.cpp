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
#include "ui/color_provider.h"
#include "ui/dialog_window.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

using namespace cutefish::ui;
using cutefish::i18n::tr;
namespace dbus = cutefish::base::dbus;

static const cutefish::base::LogCategory kLog("cutefish.installer");

static constexpr double kButtonMinimumWidth = 112;
static constexpr double kButtonSpacing = 12;
// How far a page slides in or out.
static constexpr double kSlide = 40;

class InstallerWindow::Root : public View
{
public:
    Root()
    {
        pages = addChild<View>();
        other = addChild<DialogButton>();
        back = addChild<DialogButton>();
        primary = addChild<DialogButton>(std::string(), true);
    }

    View* pages;
    // The way on at the right, the way back beside it, and at the left
    // the other way a page offers.
    DialogButton* other;
    DialogButton* back;
    DialogButton* primary;

    bool acceptsPointer() const override { return true; }

protected:
    // The empty parts of the window move it.
    void pointerPress(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left && window())
            window()->startMove();
    }

    void layout() override
    {
        const RectF inside = localRect();
        pages->setGeometry(inside);
        for (const auto& page : pages->children())
            page->setGeometry(pages->localRect());

        const auto widthOf = [](const DialogButton* button) {
            return std::max(kButtonMinimumWidth, button->sizeHint().width);
        };
        const double y = inside.height - kWindowMargin - kButtonHeight;
        double right = inside.width - kWindowMargin;
        for (DialogButton* button : {primary, back}) {
            if (!button->isVisible())
                continue;
            const double width = widthOf(button);
            button->setGeometry({right - width, y, width, kButtonHeight});
            right -= width + kButtonSpacing;
        }
        other->setGeometry({kWindowMargin, y, widthOf(other), kButtonHeight});
    }
};

InstallerWindow::InstallerWindow(Backend& backend)
    : m_backend(backend)
{
    // A window of fixed size over the live desktop, above the windows of
    // the system being tried; the compositor opens it centred.
    m_window.setTitle(tr("Install Cutefish"));
    m_window.setBackgroundColor(ColorId::SecondBackground);
    m_window.setMinimumSize({kWindowWidth, kWindowHeight});
    m_window.setMaximumSize({kWindowWidth, kWindowHeight});
    m_window.resize({kWindowWidth, kWindowHeight});
    m_window.setUserResizable(false);
    m_window.setAlwaysOnTop(true);

    auto root = std::make_unique<Root>();
    m_root = root.get();
    m_language = m_root->pages->addChild<LanguagePage>();
    buildPages();
    m_window.setContentView(std::move(root));

    m_root->primary->onClicked = [this] { primaryClicked(); };
    m_root->back->onClicked = [this] { backClicked(); };
    m_root->other->onClicked = [this] { otherClicked(); };
    m_language->onSelectionChanged = [this] { languageChosen(); };
    m_languageChanged = Application::instance()->languageChanged.connect([this] { retranslate(); });

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

    showStep(Step::Language);
}

InstallerWindow::~InstallerWindow() = default;

void InstallerWindow::buildPages()
{
    View* pages = m_root->pages;
    m_welcome = pages->addChild<WelcomePage>();
    m_disk = pages->addChild<DiskPage>();
    m_partition = pages->addChild<PartitionPage>(std::filesystem::exists("/sys/firmware/efi"));
    m_partition->setWindow(&m_window);
    m_account = pages->addChild<AccountPage>();
    m_timeZone = pages->addChild<TimeZonePage>();
    m_summary = pages->addChild<SummaryPage>();
    m_progress = pages->addChild<ProgressPage>();
    m_result = pages->addChild<ResultPage>();

    m_disk->onSelectionChanged = [this] { updateButtons(); };
    m_partition->onChanged = [this] { updateButtons(); };
    m_account->onChanged = [this] { updateButtons(); };
    m_account->onAccepted = [this] { primaryClicked(); };
    m_timeZone->onSelectionChanged = [this] { updateButtons(); };
}

void InstallerWindow::retranslate()
{
    showStep(m_step);
    m_window.setTitle(tr("Install Cutefish"));
    m_language->retranslate();

    std::vector<std::unique_ptr<View>> previous;
    const DiskPage* disk = m_disk;
    const PartitionPage* partition = m_partition;
    const AccountPage* account = m_account;
    const TimeZonePage* timeZone = m_timeZone;
    const ProgressPage* progress = m_progress;
    for (View* page : std::initializer_list<View*> {m_welcome, m_disk, m_partition, m_account, m_timeZone, m_summary,
                                                    m_progress, m_result})
        previous.push_back(m_root->pages->takeChild(page));
    buildPages();

    m_disk->takeStateFrom(*disk);
    if (const DiskInfo* chosen = m_disk->selectedDisk(); chosen && m_manual)
        m_partition->setLoading(chosen->model.empty() ? chosen->path : chosen->model);
    m_partition->takeStateFrom(*partition);
    m_account->takeStateFrom(*account);
    m_timeZone->takeStateFrom(*timeZone);
    if (int(m_step) >= int(Step::Summary) && m_disk->selectedDisk() && m_timeZone->selectedZone())
        fillSummary();
    m_progress->takeStateFrom(*progress);
    if (m_step == Step::Result) {
        if (m_succeeded)
            m_result->setSucceeded();
        else
            m_result->setFailed(m_error);
    }
    showStep(m_step);
}

void InstallerWindow::languageChosen()
{
    // cutefish-services switches the whole session, this program included,
    // whose languageChanged then calls retranslate(); quietly, as nothing
    // here needs a restart.
    dbus::Connection* bus = Application::instance()->sessionBus();
    const dbus::Proxy service(bus, "org.cutefish.Services", "/org/cutefish/Services/Language",
                              "org.cutefish.Services.Language");
    const std::vector<std::string> languages {m_language->selectedLanguage().code};
    if (!bus || !service.send("setPreferredLanguagesQuietly", languages))
        CF_WARNING(kLog) << "cutefish-installer: cannot ask the session to change the language";
}

void InstallerWindow::activate()
{
    m_window.show();
    m_window.activate();
}

Page* InstallerWindow::page(Step step) const
{
    switch (step) {
    case Step::Language:
        return m_language;
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
    return m_language;
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
    DialogButton* back = m_root->back;
    DialogButton* other = m_root->other;
    primary->setVisible(true);
    primary->setText(tr("Continue"));
    primary->setDestructive(false);
    primary->setEnabled(true);
    back->setVisible(true);
    back->setText(tr("Back"));
    other->setVisible(false);
    other->setEnabled(true);

    switch (m_step) {
    case Step::Language:
        back->setVisible(false);
        break;
    case Step::Welcome:
        other->setVisible(true);
        other->setText(tr("Try Cutefish"));
        break;
    case Step::Disk:
        other->setVisible(true);
        // TRANSLATORS: A button: choose the partitions by hand instead of erasing the disk.
        other->setText(tr("Partition Manually"));
        other->setEnabled(m_disk->selectedDisk());
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
        primary->setText(m_manual ? tr("Install") : tr("Erase Disk and Install"));
        primary->setDestructive(m_summary->erases());
        break;
    case Step::Progress:
        primary->setVisible(false);
        back->setVisible(false);
        break;
    case Step::Result:
        back->setVisible(false);
        other->setVisible(true);
        // TRANSLATORS: After a successful installation: go on using the live system instead of restarting.
        other->setText(m_succeeded ? tr("Keep Trying") : tr("Close"));
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
    case Step::Language:
        go(Step::Welcome);
        break;
    case Step::Welcome:
        go(Step::Disk);
        break;
    case Step::Disk:
        m_manual = false;
        go(Step::Account);
        break;
    case Step::Partition:
        go(Step::Account);
        break;
    case Step::Account:
        go(Step::TimeZone);
        break;
    case Step::TimeZone:
        fillSummary();
        go(Step::Summary);
        break;
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

void InstallerWindow::backClicked()
{
    switch (m_step) {
    case Step::Language:
    case Step::Progress:
    case Step::Result:
        break;
    case Step::Welcome:
        go(Step::Language);
        break;
    case Step::Disk:
        go(Step::Welcome);
        break;
    case Step::Partition:
        go(Step::Disk);
        break;
    case Step::Account:
        go(m_manual ? Step::Partition : Step::Disk);
        break;
    case Step::TimeZone:
        go(Step::Account);
        break;
    case Step::Summary:
        go(Step::TimeZone);
        break;
    }
}

void InstallerWindow::otherClicked()
{
    switch (m_step) {
    case Step::Welcome:
    case Step::Result:
        Application::instance()->quit();
        break;
    case Step::Disk:
        if (m_disk->selectedDisk()) {
            m_manual = true;
            go(Step::Partition);
        }
        break;
    default:
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

void InstallerWindow::fillSummary()
{
    installer::Options options;
    m_account->fill(options);
    m_summary->setSummary(options, *m_disk->selectedDisk(), *m_timeZone->selectedZone(),
                          m_manual ? &m_partition->plan() : nullptr);
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
    if (m_manual)
        options.partitioning = m_partition->plan().toMap();
    m_account->fill(options);
    options.timeZone = m_timeZone->selectedZone()->id;
    options.language = m_language->selectedLanguage().locale;

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
    m_error = error;
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

