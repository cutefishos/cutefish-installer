// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pages.h"

#include "base/file.h"
#include "base/i18n.h"
#include "base/strings.h"
#include "ui/cell_layout.h"
#include "ui/icon.h"
#include "ui/list_view.h"
#include "ui/painter.h"
#include "ui/progress_bar.h"
#include "ui/search_field.h"
#include "ui/text_field.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

using namespace cutefish::ui;
using cutefish::i18n::format;
using cutefish::i18n::tr;
namespace strings = cutefish::base::strings;

static constexpr double kDiskRowHeight = 56;
static constexpr double kZoneRowHeight = 44;
static constexpr int kZoneRowsShown = 4;
static constexpr double kFieldHeight = 36;
static constexpr double kFormRowHeight = 44;
// From the card's edges to the captions and the typed text.
static constexpr double kFormPadding = 16;
static constexpr double kMessageGap = 12;
static constexpr double kErrorPadding = 16;
static constexpr double kProgressHintGap = 20;

// A name the live image's system accounts take, which the installed system
// has too.
static bool isSystemAccount(const std::string& name)
{
    const std::string passwd = cutefish::base::file::read("/etc/passwd").value_or("");
    for (const std::string& line : strings::split(passwd, '\n')) {
        const std::vector<std::string> fields = strings::split(line, ':');
        if (fields.size() < 3 || fields[0] != name)
            continue;
        const std::optional<unsigned> uid = strings::toNumber<unsigned>(fields[2]);
        return !uid || *uid < 1000 || *uid == 65534;
    }
    return false;
}

// A filled circle with a white glyph, for how it ended.
class Badge : public View
{
public:
    Badge(const char* glyph, Color color)
        : m_color(color)
    {
        m_glyph = addChild<Icon>(glyph, 28);
        m_glyph->setColor(Color::rgb(0xFFFFFF));
    }

protected:
    void layout() override
    {
        const RectF bounds = localRect();
        m_glyph->setGeometry({rounding::centeredStart(bounds.width, 28), rounding::centeredStart(bounds.height, 28), 28, 28});
    }

    void paint(Painter& painter) override
    {
        painter.fillEllipse(localRect(), m_color);
    }

private:
    Color m_color;
    Icon* m_glyph;
};

WelcomePage::WelcomePage()
{
    setIcon(std::make_unique<Picture>(dataPath("logo.svg")), 112, 52);
    setTitle(tr("Welcome to Cutefish"));
    setSubtitle(tr("Try Cutefish without changing your computer, or install it now."));
}

DiskPage::DiskPage()
{
    setThemeIcon("drive-harddisk");
    setTitle(tr("Choose a Disk"));
    setSubtitle(tr("Cutefish uses the whole disk. Everything on it will be erased."));
    m_card = content()->addChild<Card>();
    m_busy = content()->addChild<BusyIndicator>();
    m_message = addLabel(content(), {}, 14);
    m_message->setTextAlignment(Label::TextAlignment::Center);
    m_message->setWrap(true);
    setLoading();
}

void DiskPage::setLoading()
{
    m_loading = true;
    m_disks.clear();
    select(-1);
    m_message->setText({});
    requestLayout();
}

void DiskPage::setDisks(std::vector<DiskInfo> disks, const std::string& preferred)
{
    m_loading = false;
    for (ChoiceRow* row : m_rows)
        m_card->takeChild(row);
    m_rows.clear();
    m_disks = std::move(disks);

    int usable = -1;
    int usableCount = 0;
    int kept = -1;
    for (size_t i = 0; i < m_disks.size(); ++i) {
        const DiskInfo& disk = m_disks[i];
        ChoiceRow* row = m_card->addChild<ChoiceRow>();
        row->setIconName(disk.removable ? "drive-harddisk-usb" : "drive-harddisk");
        row->setTitle(disk.model.empty() ? disk.path : disk.model);
        std::string detail = disk.path;
        if (disk.size < installer::kMinimumDiskSize)
            detail += " · " + format(tr("Too small, needs {size}"), {{"size", formatSize(installer::kMinimumDiskSize)}});
        row->setDetail(detail);
        row->setNote(formatSize(disk.size));
        row->setEnabled(disk.size >= installer::kMinimumDiskSize);
        row->onClicked = [this, i] { select(int(i)); };
        m_rows.push_back(row);
        if (row->isEnabled() && disk.path == preferred)
            kept = int(i);
        if (row->isEnabled() && !disk.removable) {
            usable = int(i);
            ++usableCount;
        }
    }
    m_message->setText(m_disks.empty() ? tr("No disk was found to install on.") : std::string());
    // One internal disk is the obvious choice.
    select(kept >= 0 ? kept : usableCount == 1 ? usable : -1);
    requestLayout();
}

void DiskPage::setError(const std::string& error)
{
    setDisks({});
    m_message->setText(format(tr("The disks cannot be listed: {error}"), {{"error", error}}));
}

const DiskInfo* DiskPage::selectedDisk() const
{
    return m_selected >= 0 && size_t(m_selected) < m_disks.size() ? &m_disks[size_t(m_selected)] : nullptr;
}

void DiskPage::select(int index)
{
    m_selected = index;
    for (size_t i = 0; i < m_rows.size(); ++i)
        m_rows[i]->setSelected(int(i) == index);
    if (onSelectionChanged)
        onSelectionChanged();
}

double DiskPage::contentHeight() const
{
    if (m_loading)
        return 64;
    if (m_rows.empty())
        return 64;
    return double(m_rows.size()) * kDiskRowHeight + 2 * kCardInset;
}

void DiskPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_busy->setVisible(m_loading);
    m_busy->setRunning(m_loading);
    m_busy->setGeometry({rounding::centeredStart(bounds.width, 22), rounding::centeredStart(bounds.height, 22), 22, 22});
    m_card->setVisible(!m_loading && !m_rows.empty());
    m_card->setGeometry(bounds);
    for (size_t i = 0; i < m_rows.size(); ++i)
        m_rows[i]->setGeometry({0, kCardInset + double(i) * kDiskRowHeight, bounds.width, kDiskRowHeight});
    m_message->setVisible(!m_loading && !m_message->text().empty());
    m_message->setColor(Theme::current().disabledText);
    const double messageHeight = m_message->heightForWidth(bounds.width);
    m_message->setGeometry({0, rounding::centeredStart(bounds.height, messageHeight), bounds.width, messageHeight});
}

AccountPage::AccountPage()
{
    setThemeIcon("preferences-desktop-user");
    setTitle(tr("Create Your Account"));
    setSubtitle(tr("This account can change settings and install software."));

    m_card = content()->addChild<Card>();
    m_card->setRowSeparators(kFormRowHeight);
    m_fullName = addField(tr("Full name"));
    m_userName = addField(tr("User name"));
    m_hostName = addField(tr("Computer name"));
    m_password = addField(tr("Password"));
    m_confirm = addField(tr("Confirm password"));
    m_password.field->setEchoMode(TextField::EchoMode::Password);
    m_confirm.field->setEchoMode(TextField::EchoMode::Password);
    m_userName.field->setMaximumLength(32);
    m_hostName.field->setMaximumLength(63);
    m_message = addLabel(content(), {}, 12);
    m_message->setWrap(true);

    m_fullName.field->onTextEdited = [this] {
        if (!m_userNameEdited)
            m_userName.field->setText(installer::suggestedUserName(m_fullName.field->text()));
        if (!m_hostNameEdited)
            m_hostName.field->setText(installer::suggestedHostName(m_userName.field->text()));
        changed();
    };
    m_userName.field->onTextEdited = [this] {
        m_userNameEdited = !m_userName.field->text().empty();
        if (!m_hostNameEdited)
            m_hostName.field->setText(installer::suggestedHostName(m_userName.field->text()));
        changed();
    };
    m_hostName.field->onTextEdited = [this] {
        m_hostNameEdited = !m_hostName.field->text().empty();
        changed();
    };
    m_password.field->onTextEdited = [this] { changed(); };
    m_confirm.field->onTextEdited = [this] { changed(); };

    const std::vector<TextField*> order {m_fullName.field, m_userName.field, m_hostName.field, m_password.field,
                                         m_confirm.field};
    for (size_t i = 0; i + 1 < order.size(); ++i)
        order[i]->onAccepted = [next = order[i + 1]] { next->setFocus(); };
    m_confirm.field->onAccepted = [this] {
        if (isComplete() && onAccepted)
            onAccepted();
    };
}

AccountPage::Field AccountPage::addField(const std::string& caption)
{
    Field field;
    field.caption = addLabel(m_card, caption, 14);
    field.field = m_card->addChild<TextField>();
    field.field->setFrameVisible(false);
    field.field->setTextMargins(0, 0);
    field.field->setFontSize(14);
    field.field->setPlaceholder(tr("Required"));
    return field;
}

std::string AccountPage::problem() const
{
    const std::string& userName = m_userName.field->text();
    if (!userName.empty() && !installer::isValidUserName(userName))
        return tr("A user name starts with a lower-case letter and uses only a–z, 0–9, “-” and “_”.");
    if (!userName.empty() && isSystemAccount(userName))
        return format(tr("“{name}” is taken by the system. Choose another user name."), {{"name", userName}});
    const std::string& hostName = m_hostName.field->text();
    if (!hostName.empty() && !installer::isValidHostName(hostName))
        return tr("A computer name uses only letters, digits and hyphens, and does not start or end with a hyphen.");
    const std::string& confirm = m_confirm.field->text();
    if (!confirm.empty() && confirm != m_password.field->text()
        && !m_password.field->text().starts_with(confirm))
        return tr("The passwords do not match.");
    return {};
}

bool AccountPage::isComplete() const
{
    for (const Field* field : {&m_fullName, &m_userName, &m_hostName, &m_password, &m_confirm}) {
        if (field->field->text().empty())
            return false;
    }
    return problem().empty() && m_password.field->text() == m_confirm.field->text();
}

void AccountPage::fill(installer::Options& options) const
{
    options.fullName = std::string(strings::trimmed(m_fullName.field->text()));
    options.userName = m_userName.field->text();
    options.hostName = m_hostName.field->text();
    options.password = m_password.field->text();
}

void AccountPage::focusFirstField()
{
    m_fullName.field->setFocus();
}

void AccountPage::changed()
{
    m_message->setText(problem());
    if (onChanged)
        onChanged();
}

double AccountPage::contentHeight() const
{
    return 5 * kFormRowHeight + kMessageGap + 2 * m_message->sizeHint().height;
}

void AccountPage::layoutContent()
{
    const double width = content()->geometry().width;
    const Field* fields[] = {&m_fullName, &m_userName, &m_hostName, &m_password, &m_confirm};
    double captionWidth = 0;
    for (const Field* field : fields)
        captionWidth = std::max(captionWidth, field->caption->sizeHint().width);
    const double fieldLeft = kFormPadding + captionWidth + 24;
    m_card->setGeometry({0, 0, width, 5 * kFormRowHeight});
    double y = 0;
    for (const Field* field : fields) {
        const double captionHeight = field->caption->sizeHint().height;
        field->caption->setGeometry({kFormPadding, y + rounding::centeredStart(kFormRowHeight, captionHeight),
                                     captionWidth, captionHeight});
        field->field->setGeometry({fieldLeft, y + rounding::centeredStart(kFormRowHeight, kFieldHeight),
                                   std::max(0.0, width - kFormPadding - fieldLeft), kFieldHeight});
        y += kFormRowHeight;
    }
    m_message->setColor(accent::red);
    m_message->setGeometry({0, y + kMessageGap, width, m_message->heightForWidth(width)});
}

TimeZonePage::TimeZonePage()
{
    setThemeIcon("preferences-system-time");
    setTitle(tr("Choose Your Time Zone"));
    setSubtitle(tr("The clock follows the time zone. You can change it later in Settings."));

    m_zones = loadTimeZones();
    const std::string current = currentTimeZone();
    for (size_t i = 0; i < m_zones.size(); ++i) {
        if (m_zones[i].id == current)
            m_selected = int(i);
    }

    m_search = content()->addChild<SearchField>(tr("Search for a city or country"));
    m_search->setFill(cardColor());
    m_card = content()->addChild<Card>();
    m_card->setClipsChildren(true);
    m_card->setChildClipRadius(kCardRadius);
    m_list = m_card->addChild<ListView>();
    m_list->setRowHeight(kZoneRowHeight);
    m_list->setMargins({0, kCardInset, 0, kCardInset});
    m_list->createItem = [] { return std::make_unique<ChoiceRow>(); };
    m_list->bindItem = [this](View* item, size_t row) {
        auto* choice = static_cast<ChoiceRow*>(item);
        const size_t index = m_shown[row];
        const TimeZone& zone = m_zones[index];
        choice->setTitle(zone.city);
        choice->setDetail(zone.country);
        choice->setNote(zone.offset);
        choice->setSelected(int(index) == m_selected);
        choice->onClicked = [this, index] {
            m_selected = int(index);
            m_list->reload();
            if (onSelectionChanged)
                onSelectionChanged();
        };
    };
    m_search->onTextChanged = [this] { filter(); };
    filter();
}

const TimeZone* TimeZonePage::selectedZone() const
{
    return m_selected >= 0 ? &m_zones[size_t(m_selected)] : nullptr;
}

void TimeZonePage::scrollToSelection()
{
    const auto found = std::find(m_shown.begin(), m_shown.end(), size_t(std::max(m_selected, 0)));
    if (m_selected >= 0 && found != m_shown.end())
        m_list->scrollToRow(size_t(found - m_shown.begin()));
}

void TimeZonePage::filter()
{
    m_shown.clear();
    const std::string& query = m_search->text();
    for (size_t i = 0; i < m_zones.size(); ++i) {
        if (matchesTimeZone(m_zones[i], query))
            m_shown.push_back(i);
    }
    m_list->setRowCount(m_shown.size());
    m_list->reload();
    m_list->setScrollPosition({0, 0});
}

double TimeZonePage::contentHeight() const
{
    return kFieldHeight + 12 + kZoneRowsShown * kZoneRowHeight + 2 * kCardInset;
}

void TimeZonePage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_search->setGeometry({0, 0, bounds.width, kFieldHeight});
    const double top = kFieldHeight + 12;
    m_card->setGeometry({0, top, bounds.width, bounds.height - top});
    m_list->setGeometry(m_card->localRect());
}

SummaryPage::SummaryPage()
{
    setThemeIcon("system-software-install");
    setTitle(tr("Ready to Install"));
    setSubtitle(tr("Check your choices. Go back to change any of them."));
    m_card = content()->addChild<Card>();
    for (const char* text : {tr("Disk"), tr("Full name"), tr("User name"), tr("Computer name"), tr("Time zone")})
        m_rows.push_back(m_card->addChild<SummaryRow>(text, std::string()));
    m_rows.back()->setSeparatorVisible(false);
    m_warning = addLabel(content(), {}, 12);
    m_warning->setWrap(true);
    m_warning->setTextAlignment(Label::TextAlignment::Center);
}

void SummaryPage::setSummary(const installer::Options& options, const DiskInfo& disk, const TimeZone& zone)
{
    const std::string diskName = disk.model.empty() ? disk.path : disk.model;
    m_rows[0]->setValue(diskName + " (" + formatSize(disk.size) + ")");
    m_rows[1]->setValue(options.fullName);
    m_rows[2]->setValue(options.userName);
    m_rows[3]->setValue(options.hostName);
    m_rows[4]->setValue(zone.country.empty() || zone.country == zone.city ? zone.city : zone.city + ", " + zone.country);
    m_warning->setText(format(tr("Everything on {disk} will be erased. This cannot be undone."), {{"disk", diskName}}));
    requestLayout();
}

double SummaryPage::contentHeight() const
{
    return double(m_rows.size()) * SummaryRow::kHeight + kMessageGap + m_warning->heightForWidth(kContentWidth);
}

void SummaryPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    const double cardHeight = double(m_rows.size()) * SummaryRow::kHeight;
    m_card->setGeometry({0, 0, bounds.width, cardHeight});
    for (size_t i = 0; i < m_rows.size(); ++i)
        m_rows[i]->setGeometry({0, double(i) * SummaryRow::kHeight, bounds.width, SummaryRow::kHeight});
    m_warning->setColor(accent::red);
    m_warning->setGeometry({0, cardHeight + kMessageGap, bounds.width, m_warning->heightForWidth(bounds.width)});
}

ProgressPage::ProgressPage()
{
    setIcon(std::make_unique<Picture>(dataPath("logo.svg")), 112, 52);
    setTitle(tr("Installing Cutefish"));
    m_bar = content()->addChild<ProgressBar>();
    m_hint = addLabel(content(), tr("Keep the computer on until the installation is complete."), 12);
    m_hint->setTextAlignment(Label::TextAlignment::Center);
    setProgress(installer::kStagePrepare, 0);
}

void ProgressPage::setProgress(const std::string& stage, double fraction)
{
    std::string text = tr("Preparing the disk…");
    if (stage == installer::kStageCopy)
        text = tr("Copying the system…");
    else if (stage == installer::kStageConfigure)
        text = tr("Setting up your account and settings…");
    else if (stage == installer::kStageBootloader)
        text = tr("Installing the boot loader…");
    else if (stage == installer::kStageFinish)
        text = tr("Finishing…");
    setSubtitle(text);
    m_bar->setValue(std::clamp(fraction, 0.0, 1.0));
}

double ProgressPage::contentHeight() const
{
    return 6 + kProgressHintGap + m_hint->sizeHint().height;
}

void ProgressPage::layoutContent()
{
    const double width = content()->geometry().width;
    m_bar->setGeometry({0, 0, width, 6});
    m_hint->setColor(Theme::current().disabledText);
    m_hint->setGeometry({0, 6 + kProgressHintGap, width, m_hint->sizeHint().height});
}

ResultPage::ResultPage()
{
    m_card = content()->addChild<Card>();
    m_error = addLabel(m_card, {}, 12);
    m_error->setWrap(true);
    m_error->setWrapAnywhere(true);
    m_error->setMaximumLineCount(8);
    m_error->setElide(Label::Elide::Right);
}

void ResultPage::setBadge(const char* glyph, Color color)
{
    setIcon(std::make_unique<Badge>(glyph, color), kIconSize, kIconSize);
}

void ResultPage::setSucceeded()
{
    setBadge("check", accent::green);
    setTitle(tr("Cutefish Is Installed"));
    setSubtitle(tr("Restart the computer and remove the installation medium to start using Cutefish."));
    m_error->setText({});
    requestLayout();
}

void ResultPage::setFailed(const std::string& error)
{
    setBadge("close", accent::red);
    setTitle(tr("The Installation Failed"));
    setSubtitle(tr("Cutefish could not be installed. The details are in /var/log/cutefish-installer.log."));
    m_error->setText(error);
    requestLayout();
}

double ResultPage::contentHeight() const
{
    if (m_error->text().empty())
        return 0;
    return m_error->heightForWidth(kContentWidth - 2 * kErrorPadding) + 2 * kErrorPadding;
}

void ResultPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_card->setVisible(!m_error->text().empty());
    m_card->setGeometry(bounds);
    m_error->setColor(Theme::current().disabledText);
    m_error->setGeometry(bounds.adjusted(kErrorPadding, kErrorPadding, -kErrorPadding, -kErrorPadding));
}
