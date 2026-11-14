// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pages.h"


#include "base/file.h"
#include "base/i18n.h"
#include "base/strings.h"
#include "ui/cell_layout.h"
#include "ui/color_provider.h"
#include "ui/drop_down_popup.h"
#include "ui/icon.h"
#include "ui/list_view.h"
#include "ui/painter.h"
#include "ui/progress_bar.h"
#include "ui/search_field.h"
#include "ui/text_field.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>

using namespace cutefish::ui;
using cutefish::i18n::format;
using cutefish::i18n::tr;
namespace strings = cutefish::base::strings;

static constexpr double kLogoWidth = 168;
static constexpr double kLogoHeight = 78;
// Half a row at a list's end says that it scrolls.
static constexpr double kLanguageRowsShown = 5.5;
static constexpr double kTallRowsShown = 3.5;
// As many disks stand side by side; more are a list.
static constexpr size_t kMostTiles = 4;
static constexpr double kTileWidth = 160;
static constexpr double kTileSpacing = 12;
static constexpr double kTileIcon = 72;
// Above a tile's icon and under its text.
static constexpr double kTilePadding = 20;
static constexpr double kTileTextInset = 16;
static constexpr double kTileIconGap = 10;
static constexpr double kTileTextGap = 3;
static constexpr double kFieldHeight = 36;
static constexpr double kFormRowHeight = 44;
static constexpr double kFormCaptionGap = 24;
// The line between the two password fields: to the fields beside it, and
// to the row's edges.
static constexpr double kFormDividerGap = 12;
static constexpr double kFormDividerInset = 12;
static constexpr double kMessageGap = 10;
static constexpr double kPickerInset = 14;
static constexpr double kPickerPicture = 44;
static constexpr double kPickerSpacing = 12;
static constexpr size_t kPickerColumns = 5;
static constexpr double kPickerGap = 8;
static constexpr double kSearchGap = 12;
static constexpr double kSummaryGap = 12;
static constexpr double kProgressWidth = 320;
static constexpr double kProgressHeight = 6;
static constexpr double kProgressGap = 12;
static constexpr double kBadgeSize = 72;
static constexpr double kBadgeGlyph = 32;
// Over and under the error's text; kCardPadding beside it.
static constexpr double kErrorPadding = 16;

// A card's list of rows to choose from, as tall as each row.
static ListView* addChoiceList(Card* card, double rowHeight)
{
    card->setClipsChildren(true);
    card->setChildClipRadius(kCardRadius);
    ListView* list = card->addChild<ListView>();
    list->setCornerRadius(kCardRadius);
    list->setRowHeight(rowHeight);
    list->createItem = [card] {
        auto row = std::make_unique<ChoiceRow>();
        row->setCard(card);
        return row;
    };
    return list;
}

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

static std::string diskName(const DiskInfo& disk)
{
    return disk.model.empty() ? disk.path : disk.model;
}

static const char* diskIcon(const DiskInfo& disk)
{
    return disk.removable ? "drive-harddisk-usb" : "drive-harddisk";
}

// A filled circle with a white glyph, for how it ended.
class Badge : public View
{
public:
    Badge(const char* glyph, Color color)
        : m_color(color)
    {
        m_glyph = addChild<Icon>(glyph, kBadgeGlyph);
        m_glyph->setColor(Color::rgb(0xFFFFFF));
    }

protected:
    void layout() override
    {
        const RectF bounds = localRect();
        m_glyph->setGeometry({rounding::centeredStart(bounds.width, kBadgeGlyph),
                              rounding::centeredStart(bounds.height, kBadgeGlyph), kBadgeGlyph, kBadgeGlyph});
    }

    void paint(Painter& painter) override
    {
        painter.fillEllipse(localRect(), m_color);
    }

private:
    Color m_color;
    Icon* m_glyph;
};

LanguagePage::LanguagePage()
{
    setThemeIcon("preferences-desktop-locale");
    retranslate();

    m_selected = installerLanguageFor(sessionLanguage());
    m_card = content()->addChild<Card>();
    m_list = addChoiceList(m_card, ChoiceRow::kHeight);
    m_list->bindItem = [this](View* item, size_t row) {
        auto* choice = static_cast<ChoiceRow*>(item);
        choice->setTitle(installerLanguages()[row].name);
        choice->setSelected(row == m_selected);
        choice->setSeparatorVisible(row + 1 < installerLanguages().size());
        choice->onClicked = [this, row] {
            if (row == m_selected)
                return;
            m_selected = row;
            m_list->reload();
            if (onSelectionChanged)
                onSelectionChanged();
        };
    };
    m_list->setRowCount(installerLanguages().size());
}

const InstallerLanguage& LanguagePage::selectedLanguage() const
{
    return installerLanguages()[m_selected];
}

void LanguagePage::retranslate()
{
    setTitle(tr("Choose Your Language"));
    setSubtitle(tr("The installer and the installed system use this language."));
}

double LanguagePage::contentHeight() const
{
    return kLanguageRowsShown * ChoiceRow::kHeight;
}

void LanguagePage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_card->setGeometry(bounds);
    m_list->setGeometry(m_card->localRect());
    // The language spoken now may be further down than the rows shown.
    if (!m_scrolled && bounds.height > 0) {
        m_list->scrollToRow(m_selected);
        m_scrolled = true;
    }
}

WelcomePage::WelcomePage()
    : Page(Kind::Hero)
{
    setIcon(std::make_unique<Picture>(dataPath("logo.svg")), kLogoWidth, kLogoHeight);
    setTitle(tr("Welcome to Cutefish"));
    setSubtitle(tr("Try Cutefish without changing your computer, or install it now."));
}

// A disk as a plate to choose: its icon over its name and its size.
class DiskPage::Tile : public View
{
public:
    Tile()
    {
        m_icon = addChild<ThemeIcon>(std::string(), kTileIcon);
        m_name = addLabel(this, {}, 13, 500);
        m_detail = addLabel(this, {}, 12);
        for (Label* label : {m_name, m_detail}) {
            label->setTextAlignment(Label::TextAlignment::Center);
            label->setWrap(true);
            label->setMaximumLineCount(2);
            label->setElide(Label::Elide::Right);
        }
        setTracksHover(true);
    }

    void setDisk(const std::string& icon, std::string name, std::string detail)
    {
        m_icon->setIconName(icon);
        m_name->setText(std::move(name));
        m_detail->setText(std::move(detail));
        requestLayout();
    }

    void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        requestPaint();
    }

    // As tall as its own text needs.
    double preferredHeight() const
    {
        return kTilePadding + kTileIcon + kTileIconGap + m_name->heightForWidth(textWidth()) + kTileTextGap
                + m_detail->heightForWidth(textWidth()) + kTilePadding;
    }

    bool acceptsPointer() const override { return isEnabled(); }

    std::function<void()> onClicked;

protected:
    void layout() override
    {
        const double width = geometry().width;
        m_icon->setGeometry({rounding::centeredStart(width, kTileIcon), kTilePadding, kTileIcon, kTileIcon});
        const double nameHeight = m_name->heightForWidth(textWidth());
        double y = kTilePadding + kTileIcon + kTileIconGap;
        m_name->setGeometry({kTileTextInset, y, textWidth(), nameHeight});
        y += nameHeight + kTileTextGap;
        m_detail->setColor(Theme::current().disabledText);
        m_detail->setGeometry({kTileTextInset, y, textWidth(), m_detail->heightForWidth(textWidth())});
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const RectF bounds = localRect();
        const Corners corners = themeCorners();
        painter.fillRoundedRect(bounds, kCardRadius, cardColor(), corners);
        if (m_selected) {
            painter.fillRoundedRect(bounds, kCardRadius, theme.highlight.withAlpha(theme.darkMode ? 0.24 : 0.12),
                                    corners);
            painter.strokeRoundedRect(bounds, kCardRadius, theme.highlight, painter.borderWidth(2), corners);
        } else if (isEnabled() && (m_pressed || isHovered())) {
            painter.fillRoundedRect(bounds, kCardRadius, theme.text.withAlpha(m_pressed ? 0.08 : 0.04), corners);
        }
    }

    void hoverChanged() override
    {
        requestPaint();
    }

    void enabledChanged() override
    {
        setOpacity(isEnabled() ? 1 : 0.5);
    }

    void pointerPress(const PointerEvent&) override
    {
        m_pressed = true;
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        m_pressed = false;
        requestPaint();
        if (localRect().contains(event.position) && onClicked)
            onClicked();
    }

private:
    static double textWidth() { return kTileWidth - 2 * kTileTextInset; }

    ThemeIcon* m_icon;
    Label* m_name;
    Label* m_detail;
    bool m_selected = false;
    bool m_pressed = false;
};

DiskPage::DiskPage()
{
    setThemeIcon("drive-harddisk");
    setTitle(tr("Choose a Disk"));
    setSubtitle(tr("Cutefish uses the whole disk. Everything on it will be erased."));
    m_row = content()->addChild<View>();
    m_card = content()->addChild<Card>();
    m_list = addChoiceList(m_card, ChoiceRow::kTallHeight);
    m_list->bindItem = [this](View* item, size_t row) {
        auto* choice = static_cast<ChoiceRow*>(item);
        const DiskInfo& disk = m_disks[row];
        choice->setIconName(diskIcon(disk));
        choice->setTitle(diskName(disk));
        choice->setDetail(isUsable(disk) ? detailOf(disk, true) : detailOf(disk, false) + " · " + disk.path);
        choice->setEnabled(isUsable(disk));
        choice->setSelected(int(row) == m_selected);
        choice->setSeparatorVisible(row + 1 < m_disks.size());
        choice->onClicked = [this, row] { select(int(row)); };
    };
    m_busy = content()->addChild<BusyIndicator>();
    m_message = addLabel(content(), {}, 14);
    m_message->setTextAlignment(Label::TextAlignment::Center);
    m_message->setWrap(true);
    setLoading();
}

void DiskPage::takeStateFrom(const DiskPage& other)
{
    if (!other.m_loading)
        setDisks(other.m_disks, other.selectedDisk() ? other.selectedDisk()->path : std::string());
}

void DiskPage::setLoading()
{
    m_loading = true;
    m_disks.clear();
    m_list->setRowCount(0);
    select(-1);
    m_message->setText({});
    requestLayout();
}

bool DiskPage::isUsable(const DiskInfo& disk) const
{
    return disk.size >= installer::kMinimumRootSize;
}

std::string DiskPage::detailOf(const DiskInfo& disk, bool withPath) const
{
    if (!isUsable(disk))
        // TRANSLATORS: Beside a disk; {size} is a size such as "12.0 GB".
        return format(tr("Too small, needs {size}"), {{"size", formatSize(installer::kMinimumRootSize)}});
    return withPath ? formatSize(disk.size) + " · " + disk.path : formatSize(disk.size);
}

bool DiskPage::showsTiles() const
{
    return !m_disks.empty() && m_disks.size() <= kMostTiles;
}

void DiskPage::setDisks(std::vector<DiskInfo> disks, const std::string& preferred)
{
    m_loading = false;
    for (Tile* tile : m_tiles)
        m_row->takeChild(tile);
    m_tiles.clear();
    m_disks = std::move(disks);

    int usable = -1;
    int usableCount = 0;
    int kept = -1;
    for (size_t i = 0; i < m_disks.size(); ++i) {
        const DiskInfo& disk = m_disks[i];
        if (!isUsable(disk))
            continue;
        if (disk.path == preferred)
            kept = int(i);
        if (!disk.removable) {
            usable = int(i);
            ++usableCount;
        }
    }
    for (size_t i = 0; showsTiles() && i < m_disks.size(); ++i) {
        const DiskInfo& disk = m_disks[i];
        // Two disks of one name are told apart by their paths.
        const bool shared = std::count_if(m_disks.begin(), m_disks.end(), [&disk](const DiskInfo& other) {
            return diskName(other) == diskName(disk);
        }) > 1;
        Tile* tile = m_row->addChild<Tile>();
        tile->setDisk(diskIcon(disk), diskName(disk), detailOf(disk, shared));
        tile->setEnabled(isUsable(disk));
        tile->onClicked = [this, i] { select(int(i)); };
        m_tiles.push_back(tile);
    }
    m_list->setRowCount(showsTiles() ? 0 : m_disks.size());
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
    for (size_t i = 0; i < m_tiles.size(); ++i)
        m_tiles[i]->setSelected(int(i) == index);
    m_list->reload();
    if (onSelectionChanged)
        onSelectionChanged();
}

double DiskPage::tileHeight() const
{
    double height = 0;
    for (const Tile* tile : m_tiles)
        height = std::max(height, tile->preferredHeight());
    return height;
}

double DiskPage::listHeight() const
{
    return std::min(double(m_disks.size()), kTallRowsShown) * ChoiceRow::kTallHeight;
}

double DiskPage::contentWidth() const
{
    if (!showsTiles())
        return kContentWidth;
    const double count = double(m_tiles.size());
    return std::max(kContentWidth, count * kTileWidth + (count - 1) * kTileSpacing);
}

double DiskPage::contentHeight() const
{
    if (m_loading || m_disks.empty())
        return 64;
    return showsTiles() ? tileHeight() : listHeight();
}

void DiskPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_busy->setVisible(m_loading);
    m_busy->setRunning(m_loading);
    m_busy->setGeometry({rounding::centeredStart(bounds.width, 22), rounding::centeredStart(bounds.height, 22), 22, 22});

    m_row->setVisible(!m_loading && showsTiles());
    m_row->setGeometry(bounds);
    const double count = double(m_tiles.size());
    double x = rounding::centeredStart(bounds.width, count * kTileWidth + (count - 1) * kTileSpacing);
    for (Tile* tile : m_tiles) {
        tile->setGeometry({x, 0, kTileWidth, bounds.height});
        x += kTileWidth + kTileSpacing;
    }

    m_card->setVisible(!m_loading && !m_disks.empty() && !showsTiles());
    m_card->setGeometry(bounds);
    m_list->setGeometry(m_card->localRect());

    m_message->setVisible(!m_loading && !m_message->text().empty());
    m_message->setColor(Theme::current().disabledText);
    const double messageHeight = m_message->heightForWidth(bounds.width);
    m_message->setGeometry({0, rounding::centeredStart(bounds.height, messageHeight), bounds.width, messageHeight});
}

// The card of the account's fields, with a line between the two that share
// its last row.
class AccountPage::Form : public Card
{
public:
    void setDivider(double x, double y, double height)
    {
        m_x = x;
        m_y = y;
        m_height = height;
        requestPaint();
    }

protected:
    void paint(Painter& painter) override
    {
        Card::paint(painter);
        if (m_height > 0)
            painter.drawVerticalLine(m_x, m_y, m_height, color(ColorId::Separator));
    }

private:
    double m_x = 0;
    double m_y = 0;
    double m_height = 0;
};

// The pictures to choose the account's from, in rows.
class AccountPage::PictureGrid : public View
{
public:
    explicit PictureGrid(const std::vector<std::string>& files)
    {
        for (const std::string& file : files) {
            AccountPicture* picture = addChild<AccountPicture>(file);
            picture->onClicked = [this, file] {
                if (onChosen)
                    onChosen(file);
            };
            m_pictures.push_back(picture);
        }
    }

    void setChosen(const std::string& file)
    {
        for (AccountPicture* picture : m_pictures)
            picture->setSelected(picture->file() == file);
    }

    SizeF sizeHint() const override
    {
        const double columns = double(std::min(m_pictures.size(), kPickerColumns));
        const double rows = std::ceil(double(m_pictures.size()) / double(kPickerColumns));
        return {2 * kPickerInset + columns * kPickerPicture + (columns - 1) * kPickerSpacing,
                2 * kPickerInset + rows * kPickerPicture + (rows - 1) * kPickerSpacing};
    }

    double heightForWidth(double) const override
    {
        return sizeHint().height;
    }

    std::function<void(const std::string& file)> onChosen;

protected:
    void layout() override
    {
        for (size_t i = 0; i < m_pictures.size(); ++i) {
            const double step = kPickerPicture + kPickerSpacing;
            m_pictures[i]->setGeometry({kPickerInset + double(i % kPickerColumns) * step,
                                        kPickerInset + double(i / kPickerColumns) * step, kPickerPicture,
                                        kPickerPicture});
        }
    }

private:
    std::vector<AccountPicture*> m_pictures;
};

AccountPage::AccountPage()
{
    const std::vector<std::string> pictures = accountPictures();
    if (pictures.empty()) {
        setThemeIcon("preferences-desktop-user");
    } else {
        // Any of them to begin with, as the service picks without a choice.
        std::random_device device;
        std::uniform_int_distribution<size_t> pick(0, pictures.size() - 1);
        auto picture = std::make_unique<AccountPicture>(pictures[pick(device)]);
        m_picture = picture.get();
        m_picture->onClicked = [this] { choosePicture(); };
        setIcon(std::move(picture), kIconSize, kIconSize);

        m_picker = std::make_unique<DropDownPopup>();
        m_grid = m_picker->setContent<PictureGrid>(pictures);
        m_grid->onChosen = [this](const std::string& file) {
            m_picture->setFile(file);
            m_picker->close();
        };
    }
    setTitle(tr("Create Your Account"));
    setSubtitle(tr("This account can change settings and install software."));

    m_card = content()->addChild<Form>();
    m_card->setRowSeparators(kFormRowHeight);
    m_fullName = addField(tr("Full name"));
    m_userName = addField(tr("User name"));
    m_hostName = addField(tr("Computer name"));
    m_password = addField(tr("Password"));
    m_confirm = addTextField();
    // TRANSLATORS: In the empty field where the password is typed a second time.
    m_confirm->setPlaceholder(tr("Verify"));
    m_password.field->setEchoMode(TextField::EchoMode::Password);
    m_confirm->setEchoMode(TextField::EchoMode::Password);
    m_userName.field->setMaximumLength(32);
    m_hostName.field->setMaximumLength(63);
    m_message = addLabel(content(), {}, 12);
    m_message->setWrap(true);
    m_message->setTextAlignment(Label::TextAlignment::Center);

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
    m_confirm->onTextEdited = [this] { changed(); };

    const std::vector<TextField*> order {m_fullName.field, m_userName.field, m_hostName.field, m_password.field,
                                         m_confirm};
    for (size_t i = 0; i + 1 < order.size(); ++i)
        order[i]->onAccepted = [next = order[i + 1]] { next->setFocus(); };
    m_confirm->onAccepted = [this] {
        if (isComplete() && onAccepted)
            onAccepted();
    };
}

AccountPage::~AccountPage() = default;

AccountPage::Field AccountPage::addField(const std::string& caption)
{
    Field field;
    field.caption = addLabel(m_card, caption, 14);
    field.field = addTextField();
    return field;
}

TextField* AccountPage::addTextField()
{
    TextField* field = m_card->addChild<TextField>();
    field->setFrameVisible(false);
    field->setTextMargins(0, 0);
    field->setFontSize(14);
    // TRANSLATORS: In an empty field that must be filled in.
    field->setPlaceholder(tr("Required"));
    return field;
}

void AccountPage::choosePicture()
{
    m_grid->setChosen(m_picture->file());
    // Centred under the picture.
    const double width = m_grid->sizeHint().width;
    m_picker->popupFor(m_picture, kPickerGap, RectF {std::round((kIconSize - width) / 2), 0, width, kIconSize});
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
    const std::string& confirm = m_confirm->text();
    if (!confirm.empty() && confirm != m_password.field->text()
        && !m_password.field->text().starts_with(confirm))
        return tr("The passwords do not match.");
    return {};
}

bool AccountPage::isComplete() const
{
    for (const Field* field : {&m_fullName, &m_userName, &m_hostName, &m_password}) {
        if (field->field->text().empty())
            return false;
    }
    return problem().empty() && m_password.field->text() == m_confirm->text();
}

void AccountPage::fill(installer::Options& options) const
{
    options.fullName = std::string(strings::trimmed(m_fullName.field->text()));
    options.userName = m_userName.field->text();
    options.hostName = m_hostName.field->text();
    options.password = m_password.field->text();
    if (m_picture)
        options.picture = std::filesystem::path(m_picture->file()).filename().string();
}

void AccountPage::focusFirstField()
{
    m_fullName.field->setFocus();
}

void AccountPage::takeStateFrom(const AccountPage& other)
{
    if (m_picture && other.m_picture)
        m_picture->setFile(other.m_picture->file());
    m_fullName.field->setText(other.m_fullName.field->text());
    m_userName.field->setText(other.m_userName.field->text());
    m_hostName.field->setText(other.m_hostName.field->text());
    m_password.field->setText(other.m_password.field->text());
    m_confirm->setText(other.m_confirm->text());
    m_userNameEdited = other.m_userNameEdited;
    m_hostNameEdited = other.m_hostNameEdited;
    changed();
}

void AccountPage::changed()
{
    m_message->setText(problem());
    if (onChanged)
        onChanged();
}

// Room for a message of two lines under the card, and as much above it, so
// the card stays in the middle.
double AccountPage::contentHeight() const
{
    return 4 * kFormRowHeight + 2 * (kMessageGap + 2 * m_message->sizeHint().height);
}

void AccountPage::layoutContent()
{
    const double width = content()->geometry().width;
    const double top = kMessageGap + 2 * m_message->sizeHint().height;
    const Field* fields[] = {&m_fullName, &m_userName, &m_hostName, &m_password};
    double captionWidth = 0;
    for (const Field* field : fields)
        captionWidth = std::max(captionWidth, field->caption->sizeHint().width);
    const double fieldLeft = kCardPadding + captionWidth + kFormCaptionGap;
    const double fieldRight = std::max(fieldLeft, width - kCardPadding);
    m_card->setGeometry({0, top, width, 4 * kFormRowHeight});
    double y = 0;
    for (const Field* field : fields) {
        const double fieldTop = y + rounding::centeredStart(kFormRowHeight, kFieldHeight);
        field->field->setGeometry({fieldLeft, fieldTop, fieldRight - fieldLeft, kFieldHeight});
        // The caption on the baseline of what is typed beside it.
        field->caption->setGeometry({kCardPadding,
                                     fieldTop + field->field->baseline() - field->caption->baselineOffset(),
                                     captionWidth, field->caption->sizeHint().height});
        y += kFormRowHeight;
    }
    // The password and its second typing share the last row.
    const double row = y - kFormRowHeight;
    const double middle = std::round((fieldLeft + fieldRight) / 2);
    const double fieldTop = row + rounding::centeredStart(kFormRowHeight, kFieldHeight);
    m_password.field->setGeometry({fieldLeft, fieldTop, std::max(0.0, middle - kFormDividerGap - fieldLeft),
                                   kFieldHeight});
    m_confirm->setGeometry({middle + kFormDividerGap, fieldTop, std::max(0.0, fieldRight - middle - kFormDividerGap),
                            kFieldHeight});
    m_card->setDivider(middle, row + kFormDividerInset, kFormRowHeight - 2 * kFormDividerInset);

    m_message->setColor(accent::red);
    m_message->setGeometry({0, top + y + kMessageGap, width, m_message->heightForWidth(width)});
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
    m_list = addChoiceList(m_card, ChoiceRow::kTallHeight);
    m_list->bindItem = [this](View* item, size_t row) {
        auto* choice = static_cast<ChoiceRow*>(item);
        const size_t index = m_shown[row];
        const TimeZone& zone = m_zones[index];
        choice->setTitle(zone.city);
        choice->setDetail(zone.country.empty() || zone.country == zone.city ? zone.offset
                                                                             : zone.country + " · " + zone.offset);
        choice->setSelected(int(index) == m_selected);
        choice->setSeparatorVisible(row + 1 < m_shown.size());
        choice->onClicked = [this, index] { select(int(index)); };
    };
    // TRANSLATORS: Where the cities found would be listed, when the search finds none.
    m_empty = addLabel(m_card, tr("No Results"), 14);
    m_empty->setTextAlignment(Label::TextAlignment::Center);

    m_search->onTextChanged = [this] { filter(); };
    m_search->onAccepted = [this] {
        if (!strings::trimmed(m_search->text()).empty() && !m_shown.empty())
            select(int(m_shown.front()));
    };
    filter();
}

const TimeZone* TimeZonePage::selectedZone() const
{
    return m_selected >= 0 ? &m_zones[size_t(m_selected)] : nullptr;
}

void TimeZonePage::scrollToSelection()
{
    const auto found = std::find(m_shown.begin(), m_shown.end(), size_t(std::max(m_selected, 0)));
    if (m_selected < 0 || found == m_shown.end())
        return;
    // With the row before it in sight, when it has one.
    const double row = double(std::max<ptrdiff_t>(0, found - m_shown.begin() - 1));
    m_list->setScrollPosition({0, row * ChoiceRow::kTallHeight});
}

void TimeZonePage::setQuery(const std::string& query)
{
    m_search->setText(query);
    filter();
}

void TimeZonePage::takeStateFrom(const TimeZonePage& other)
{
    select(other.m_selected);
    setQuery(other.m_search->text());
}

void TimeZonePage::select(int index)
{
    if (index == m_selected)
        return;
    m_selected = index;
    m_list->reload();
    if (onSelectionChanged)
        onSelectionChanged();
}

void TimeZonePage::filter()
{
    const std::string& query = m_search->text();
    m_shown.clear();
    for (size_t i = 0; i < m_zones.size(); ++i) {
        if (matchesTimeZone(m_zones[i], query))
            m_shown.push_back(i);
    }
    std::stable_partition(m_shown.begin(), m_shown.end(),
                          [&](size_t index) { return cityBeginsWith(m_zones[index], query); });
    m_list->setRowCount(m_shown.size());
    m_list->reload();
    m_list->setScrollPosition({0, 0});
    m_empty->setVisible(m_shown.empty());
    requestLayout();
}

double TimeZonePage::contentHeight() const
{
    return kFieldHeight + kSearchGap + kTallRowsShown * ChoiceRow::kTallHeight;
}

void TimeZonePage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_search->setGeometry({0, 0, bounds.width, kFieldHeight});
    const double top = kFieldHeight + kSearchGap;
    // As tall as the cities found, and one row to say that none was.
    const double rows = std::clamp(double(m_shown.size()), 1.0, kTallRowsShown);
    m_card->setGeometry({0, top, bounds.width, rows * ChoiceRow::kTallHeight});
    m_list->setGeometry(m_card->localRect());
    const double emptyHeight = m_empty->sizeHint().height;
    m_empty->setColor(Theme::current().disabledText);
    m_empty->setGeometry({kCardPadding, rounding::centeredStart(ChoiceRow::kTallHeight, emptyHeight),
                          std::max(0.0, bounds.width - 2 * kCardPadding), emptyHeight});
}

SummaryPage::SummaryPage()
{
    setThemeIcon("system-software-install");
    setTitle(tr("Ready to Install"));
    setSubtitle(tr("Check your choices. Go back to change any of them."));
    m_card = content()->addChild<Card>();
    m_disk = m_card->addChild<SummaryItem>();
    m_account = m_card->addChild<SummaryItem>();
    m_zone = m_card->addChild<SummaryItem>();
    m_zone->setPicture(std::make_unique<ThemeIcon>("preferences-system-time", SummaryItem::kPictureSize));
    m_zone->setSeparatorVisible(false);
    m_warning = content()->addChild<Callout>();
}

void SummaryPage::setSummary(const installer::Options& options, const DiskInfo& disk, const TimeZone& zone,
                             const installer::PartitionPlan* plan)
{
    const std::string name = diskName(disk);
    m_disk->setPicture(std::make_unique<ThemeIcon>(diskIcon(disk), SummaryItem::kPictureSize));
    // TRANSLATORS: Under a disk's name; {disk} is its size, such as "512.1 GB".
    m_disk->setText(name, plan ? format(tr("Partitioned manually, {disk}"), {{"disk", formatSize(disk.size)}})
                               : formatSize(disk.size));
    if (options.picture.empty())
        m_account->setPicture(std::make_unique<ThemeIcon>("preferences-desktop-user", SummaryItem::kPictureSize));
    else
        m_account->setPicture(std::make_unique<AccountPicture>(accountPicturePath(options.picture)));
    m_account->setText(options.fullName, options.userName + " · " + options.hostName);
    m_zone->setText(zone.country.empty() || zone.country == zone.city ? zone.city : zone.city + ", " + zone.country,
                    zone.offset);

    m_erases = !plan || plan->hasNewTable() || !plan->erasedPartitions().empty();
    std::string warning = tr("No existing partition is erased.");
    if (!plan) {
        warning = format(tr("Everything on {disk} will be erased. This cannot be undone."), {{"disk", name}});
    } else if (plan->hasNewTable()) {
        warning = format(tr("Every partition on {disk} will be deleted. This cannot be undone."), {{"disk", name}});
    } else if (const std::vector<std::string> erased = plan->erasedPartitions(); !erased.empty()) {
        warning = format(tr("Everything on {partitions} will be erased. This cannot be undone."),
                         {{"partitions", strings::join(erased, ", ")}});
    }
    m_warning->setText(std::move(warning), m_erases);
    requestLayout();
}

double SummaryPage::contentHeight() const
{
    double height = kSummaryGap + m_warning->heightForWidth(kContentWidth);
    for (const SummaryItem* item : {m_disk, m_account, m_zone})
        height += item->heightForWidth(kContentWidth);
    return height;
}

void SummaryPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    double cardHeight = 0;
    for (SummaryItem* item : {m_disk, m_account, m_zone}) {
        const double itemHeight = item->heightForWidth(bounds.width);
        item->setGeometry({0, cardHeight, bounds.width, itemHeight});
        cardHeight += itemHeight;
    }
    m_card->setGeometry({0, 0, bounds.width, cardHeight});
    m_warning->setGeometry({0, cardHeight + kSummaryGap, bounds.width, m_warning->heightForWidth(bounds.width)});
}

ProgressPage::ProgressPage()
    : Page(Kind::Hero)
{
    setIcon(std::make_unique<Picture>(dataPath("logo.svg")), kLogoWidth, kLogoHeight);
    setTitle(tr("Installing Cutefish"));
    m_bar = content()->addChild<ProgressBar>();
    m_stageText = addLabel(content(), {}, 13);
    m_stageText->setTextAlignment(Label::TextAlignment::Center);
    m_hint = addLabel(this, tr("Keep the computer on until the installation is complete."), 12);
    m_hint->setTextAlignment(Label::TextAlignment::Center);
    setProgress(installer::kStagePrepare, 0);
}

void ProgressPage::setProgress(const std::string& stage, double fraction)
{
    m_stage = stage;
    m_fraction = fraction;
    std::string text = tr("Preparing the disk…");
    if (stage == installer::kStageCopy)
        text = tr("Copying the system…");
    else if (stage == installer::kStageConfigure)
        text = tr("Setting up your account and settings…");
    else if (stage == installer::kStageBootloader)
        text = tr("Installing the boot loader…");
    else if (stage == installer::kStageFinish)
        text = tr("Finishing…");
    m_stageText->setText(text);
    m_bar->setValue(std::clamp(fraction, 0.0, 1.0));
}

void ProgressPage::takeStateFrom(const ProgressPage& other)
{
    setProgress(other.m_stage, other.m_fraction);
}

double ProgressPage::contentHeight() const
{
    return kProgressHeight + kProgressGap + m_stageText->sizeHint().height;
}

void ProgressPage::layout()
{
    Page::layout();
    const RectF bounds = localRect();
    const double hintHeight = m_hint->sizeHint().height;
    m_hint->setColor(Theme::current().disabledText);
    m_hint->setGeometry({kWindowMargin,
                         bounds.height - kWindowMargin - kButtonHeight
                                 + rounding::centeredStart(kButtonHeight, hintHeight),
                         std::max(0.0, bounds.width - 2 * kWindowMargin), hintHeight});
}

void ProgressPage::layoutContent()
{
    const double width = content()->geometry().width;
    const double barWidth = std::min(kProgressWidth, width);
    m_bar->setGeometry({rounding::centeredStart(width, barWidth), 0, barWidth, kProgressHeight});
    m_stageText->setColor(Theme::current().disabledText);
    m_stageText->setGeometry({0, kProgressHeight + kProgressGap, width, m_stageText->sizeHint().height});
}

ResultPage::ResultPage()
    : Page(Kind::Hero)
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
    setIcon(std::make_unique<Badge>(glyph, color), kBadgeSize, kBadgeSize);
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
    setSubtitle(format(tr("Cutefish could not be installed. The details are in {path}."),
                       {{"path", "/var/log/cutefish-installer.log"}}));
    m_error->setText(error);
    requestLayout();
}

double ResultPage::contentHeight() const
{
    if (m_error->text().empty())
        return 0;
    return m_error->heightForWidth(kContentWidth - 2 * kCardPadding) + 2 * kErrorPadding;
}

void ResultPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    m_card->setVisible(!m_error->text().empty());
    m_card->setGeometry(bounds);
    m_error->setColor(Theme::current().disabledText);
    m_error->setGeometry(bounds.adjusted(kCardPadding, kErrorPadding, -kCardPadding, -kErrorPadding));
}
