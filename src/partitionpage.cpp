// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "partitionpage.h"

#include "base/i18n.h"
#include "base/strings.h"
#include "ui/cell_layout.h"
#include "ui/check_box.h"
#include "ui/combo_box.h"
#include "ui/dialog_window.h"
#include "ui/list_view.h"
#include "ui/painter.h"
#include "ui/progress_bar.h"
#include "ui/text_field.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace cutefish::ui;
using cutefish::i18n::format;
using cutefish::i18n::tr;
namespace strings = cutefish::base::strings;

static constexpr double kPartitionContentWidth = 720;
static constexpr double kBarHeight = 24;
static constexpr double kBarRadius = 6;
static constexpr double kRowHeight = 40;
static constexpr int kRowsShown = 5;
static constexpr double kHeaderHeight = 18;
static constexpr double kGap = 12;
static constexpr double kActionHeight = 30;
// From the card's edge to the columns.
static constexpr double kRowPadding = 16;
// Where each column starts in a row, after the colour swatch.
static constexpr double kFileSystemColumn = 210;
static constexpr double kUseColumn = 390;
static constexpr double kFormatColumn = 540;
static constexpr double kDialogWidth = 420;
static constexpr double kFieldHeight = 32;
static constexpr double kDialogRow = 44;
static constexpr double kCaptionWidth = 120;
static constexpr uint64_t kGB = 1'000'000'000;

static Color colorOf(const installer::PlannedPartition* partition)
{
    const Theme& theme = Theme::current();
    if (!partition)
        return theme.text.withAlpha(theme.darkMode ? 0.16 : 0.08);
    const std::string& use = partition->mountPoint;
    if (use == "/")
        return accent::blue;
    if (use == "/home")
        return accent::green;
    if (use == "/boot/efi")
        return accent::orange;
    if (use == installer::kMountSwap)
        return accent::purple;
    if (!use.empty())
        return accent::pink;
    return Color::rgb(0x9AA3B2);
}

static std::string sizeText(uint64_t bytes)
{
    return formatSize(bytes);
}

std::string mountPointName(const std::string& mountPoint)
{
    if (mountPoint.empty())
        return tr("Not used");
    if (mountPoint == "/")
        return tr("/ (system)");
    if (mountPoint == "/boot/efi")
        return tr("/boot/efi (EFI system)");
    if (mountPoint == installer::kMountSwap)
        return tr("Swap");
    if (mountPoint == installer::kMountBiosBoot)
        return tr("BIOS boot");
    return mountPoint;
}

// The disk drawn to scale, a segment for each partition and free space.
class PartitionBar : public View
{
public:
    explicit PartitionBar(PartitionPage& page)
        : m_page(page)
    {
    }

    bool acceptsPointer() const override { return true; }

protected:
    void paint(Painter& painter) override
    {
        const RectF bounds = localRect();
        painter.save();
        painter.clipRoundedRect(bounds, kBarRadius);
        painter.fillRect(bounds, colorOf(nullptr));
        const std::vector<RectF> rects = segments();
        const auto& entries = m_page.entries();
        for (size_t i = 0; i < rects.size(); ++i) {
            const int index = entries[i].partition;
            const installer::PlannedPartition* partition =
                    index >= 0 ? &m_page.plan().partitions()[size_t(index)] : nullptr;
            painter.fillRect(rects[i], colorOf(partition));
        }
        painter.restore();
        if (m_page.selected() >= 0 && size_t(m_page.selected()) < rects.size()) {
            const RectF selected = rects[size_t(m_page.selected())];
            painter.strokeRoundedRect(selected, std::min(kBarRadius, selected.width / 2), Theme::current().highlight, 2);
        }
    }

    void pointerRelease(const PointerEvent& event) override
    {
        const std::vector<RectF> rects = segments();
        for (size_t i = 0; i < rects.size(); ++i) {
            if (rects[i].contains(event.position))
                m_page.select(int(i));
        }
    }

private:
    // Each at least a few pixels wide, so the smallest stays visible; a
    // device pixel of the background between them.
    std::vector<RectF> segments() const
    {
        const RectF bounds = localRect();
        const auto& entries = m_page.entries();
        const double total = double(std::max<uint64_t>(m_page.plan().layout().size, 1));
        std::vector<RectF> rects;
        double x = 0;
        for (size_t i = 0; i < entries.size(); ++i) {
            const double width = std::max(4.0, std::round(bounds.width * double(entries[i].size) / total));
            const double right = i + 1 == entries.size() ? bounds.width : std::min(bounds.width, x + width);
            rects.push_back({x, 0, std::max(0.0, right - x - (i + 1 == entries.size() ? 0 : 2)), bounds.height});
            x = right;
        }
        return rects;
    }

    PartitionPage& m_page;
};

// One row of the list: the partition, its file system, its use and size.
class PartitionRow : public View
{
public:
    PartitionRow()
    {
        m_device = addLabel(this, {}, 13, 500);
        m_device->setElide(Label::Elide::Right);
        m_fileSystem = addLabel(this, {}, 13);
        m_fileSystem->setElide(Label::Elide::Right);
        m_use = addLabel(this, {}, 13);
        m_use->setElide(Label::Elide::Right);
        m_format = addLabel(this, {}, 13);
        m_size = addLabel(this, {}, 13);
        m_size->setTextAlignment(Label::TextAlignment::Right);
        setTracksHover(true);
    }

    void bind(const installer::PlannedPartition* partition, uint64_t size, bool selected)
    {
        m_color = colorOf(partition);
        m_selected = selected;
        m_size->setText(sizeText(size));
        if (!partition) {
            m_device->setText(tr("Free space"));
            m_fileSystem->setText({});
            m_use->setText({});
            m_format->setText({});
        } else {
            m_device->setText(partition->isNew() ? tr("New partition") : partition->path);
            std::string fileSystem = partition->format ? partition->fsType : partition->existingFsType;
            if (!partition->format && !partition->label.empty())
                fileSystem += (fileSystem.empty() ? "" : " · ") + partition->label;
            m_fileSystem->setText(fileSystem.empty() ? std::string("—") : fileSystem);
            m_use->setText(partition->mountPoint.empty() ? std::string("—") : mountPointName(partition->mountPoint));
            m_format->setText(partition->format && !partition->isNew() ? tr("Format") : std::string());
        }
        requestLayout();
        requestPaint();
    }

    bool acceptsPointer() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override
    {
        const Theme& theme = Theme::current();
        const double width = geometry().width;
        const double height = geometry().height;
        const double textHeight = m_device->sizeHint().height;
        const double top = rounding::centeredStart(height, textHeight);
        const double left = kRowPadding + 10 + 10;
        m_device->setGeometry({left, top, kFileSystemColumn - left - 8, textHeight});
        m_fileSystem->setColor(theme.disabledText);
        m_fileSystem->setGeometry({kFileSystemColumn, top, kUseColumn - kFileSystemColumn - 8, textHeight});
        m_use->setGeometry({kUseColumn, top, kFormatColumn - kUseColumn - 8, textHeight});
        m_format->setColor(accent::red);
        m_format->setGeometry({kFormatColumn, top, 80, textHeight});
        m_size->setColor(theme.disabledText);
        m_size->setGeometry({width - kRowPadding - 100, top, 100, textHeight});
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const RectF background = localRect().adjusted(kCardInset, 0, -kCardInset, 0);
        const double radius = kCardRadius - kCardInset;
        if (m_selected)
            painter.fillRoundedRect(background, radius, theme.highlight.withAlpha(theme.darkMode ? 0.24 : 0.12));
        else if (isHovered())
            painter.fillRoundedRect(background, radius, theme.text.withAlpha(0.04));
        const double swatch = 10;
        painter.fillRoundedRect({kRowPadding, rounding::centeredStart(geometry().height, swatch), swatch, swatch}, 3,
                                m_color);
    }

    void hoverChanged() override { requestPaint(); }

    void pointerRelease(const PointerEvent& event) override
    {
        if (localRect().contains(event.position) && onClicked)
            onClicked();
    }

private:
    Label* m_device;
    Label* m_fileSystem;
    Label* m_use;
    Label* m_format;
    Label* m_size;
    Color m_color;
    bool m_selected = false;
};

// The dialog's form: size for a new partition, its use, the file system and,
// for one that exists, whether to format it.
class PartitionEditor : public View
{
public:
    enum class Mode {
        Add,
        EditNew,
        EditExisting,
    };

    PartitionEditor(Mode mode, bool uefi, bool gpt, const std::vector<std::string>& fileSystems)
        : m_mode(mode)
        , m_available(fileSystems)
    {
        const bool isNew = mode == Mode::Add;
        m_sizeCaption = addLabel(this, tr("Size"), 13);
        m_size = addChild<TextField>();
        m_size->setValidator([](const std::string& text) {
            return std::all_of(text.begin(), text.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; });
        });
        m_unit = addLabel(this, "GB", 13);
        m_sizeCaption->setVisible(isNew);
        m_size->setVisible(isNew);
        m_unit->setVisible(isNew);

        m_useCaption = addLabel(this, tr("Use as"), 13);
        m_uses.push_back({});
        for (const std::string& mountPoint : installer::mountPoints()) {
            if (mountPoint == installer::kMountBiosBoot && (uefi || !gpt))
                continue;
            m_uses.push_back(mountPoint);
        }
        std::vector<std::string> names;
        for (const std::string& use : m_uses)
            names.push_back(mountPointName(use));
        m_use = addChild<ComboBox>(names);

        m_fileSystemCaption = addLabel(this, tr("File system"), 13);
        m_fileSystem = addChild<ComboBox>();
        m_format = addChild<CheckBox>(tr("Format this partition"));
        m_format->setVisible(mode == Mode::EditExisting);

        m_cancel = addChild<DialogButton>(tr("Cancel"));
        m_accept = addChild<DialogButton>(isNew ? tr("Add") : tr("Save"), true);

        m_use->onActivated = [this](int) { useChanged(); };
        m_format->onToggled = [this](bool) { updateFileSystems(); };
        m_size->onTextEdited = [this] { m_sizeEdited = true; };
    }

    // The partition's state now; the size is the space it can take.
    void setValues(uint64_t size, const std::string& use, bool format, const std::string& fsType,
                   const std::string& existingFsType)
    {
        m_maximum = size;
        m_existingFsType = existingFsType;
        char text[32];
        std::snprintf(text, sizeof text, "%.1f", std::floor(double(size) / double(kGB) * 10) / 10);
        m_size->setText(text);
        const auto found = std::find(m_uses.begin(), m_uses.end(), use);
        m_use->setCurrentIndex(found == m_uses.end() ? 0 : int(found - m_uses.begin()));
        m_format->setChecked(format);
        m_initialFsType = fsType;
        useChanged();
    }

    std::string use() const { return m_uses[size_t(std::max(0, m_use->currentIndex()))]; }
    bool format() const { return m_mode != Mode::EditExisting || m_format->isChecked(); }
    std::string fsType() const
    {
        const int index = m_fileSystem->currentIndex();
        return index >= 0 && size_t(index) < m_fileSystemTypes.size() ? m_fileSystemTypes[size_t(index)] : std::string();
    }
    uint64_t size() const
    {
        if (use() == installer::kMountBiosBoot)
            return installer::kMiB;
        if (!m_sizeEdited)
            return m_maximum;
        const std::optional<double> gb = strings::toDouble(m_size->text());
        return gb ? std::min(m_maximum, uint64_t(*gb * double(kGB))) : 0;
    }

    DialogButton* cancelButton() const { return m_cancel; }
    DialogButton* acceptButton() const { return m_accept; }

    double heightForWidth(double) const override { return rows() * kDialogRow + kGap + kFieldHeight; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const Theme& theme = Theme::current();
        double y = 0;
        const auto row = [&](Label* caption, View* field, double fieldWidth) {
            const double captionHeight = caption->sizeHint().height;
            caption->setColor(theme.disabledText);
            caption->setGeometry({0, y + rounding::centeredStart(kDialogRow, captionHeight), kCaptionWidth, captionHeight});
            field->setGeometry({kCaptionWidth, y + rounding::centeredStart(kDialogRow, kFieldHeight), fieldWidth, kFieldHeight});
            y += kDialogRow;
        };
        if (m_mode == Mode::Add) {
            row(m_sizeCaption, m_size, width - kCaptionWidth - 40);
            const double unitHeight = m_unit->sizeHint().height;
            m_unit->setGeometry({width - 32, y - kDialogRow + rounding::centeredStart(kDialogRow, unitHeight), 32, unitHeight});
        }
        row(m_useCaption, m_use, width - kCaptionWidth);
        row(m_fileSystemCaption, m_fileSystem, width - kCaptionWidth);
        if (m_mode == Mode::EditExisting) {
            const double height = m_format->sizeHint().height;
            m_format->setGeometry({kCaptionWidth, y + rounding::centeredStart(kDialogRow, height), width - kCaptionWidth, height});
            y += kDialogRow;
        }
        y += kGap;
        const double buttonWidth = 96;
        m_accept->setGeometry({width - buttonWidth, y, buttonWidth, kFieldHeight});
        m_cancel->setGeometry({width - 2 * buttonWidth - 8, y, buttonWidth, kFieldHeight});
    }

private:
    double rows() const { return m_mode == Mode::EditNew ? 2 : 3; }

    void useChanged()
    {
        const std::string current = use();
        const bool canKeep = current == "/home" || current == "/boot/efi" || current == installer::kMountSwap;
        if (m_mode == Mode::EditExisting) {
            // A partition the system lives on starts empty.
            if (current.empty())
                m_format->setChecked(false);
            else if (!canKeep)
                m_format->setChecked(true);
            m_format->setEnabled(canKeep);
        }
        if (m_mode == Mode::Add && !m_sizeEdited && current == "/boot/efi") {
            m_size->setText("0.5");
            m_sizeEdited = true;
        }
        m_size->setReadOnly(current == installer::kMountBiosBoot);
        if (current == installer::kMountBiosBoot)
            m_size->setText("0.001");
        updateFileSystems();
    }

    void updateFileSystems()
    {
        const std::string current = use();
        m_fileSystemTypes.clear();
        // A new partition not used for anything gets no file system.
        if (!format() || current.empty() || current == installer::kMountBiosBoot) {
            const std::string shown = !format() && !current.empty() ? m_existingFsType : std::string();
            m_fileSystemTypes.push_back(shown);
            m_fileSystem->setItems({shown.empty() ? std::string("—") : shown});
            m_fileSystem->setEnabled(false);
            return;
        }
        for (const std::string& type : m_available) {
            const bool fits = current == "/boot/efi" ? type == "vfat"
                    : current == installer::kMountSwap ? type == "swap"
                                                        : type == "ext4" || type == "btrfs" || type == "xfs";
            if (fits)
                m_fileSystemTypes.push_back(type);
        }
        m_fileSystem->setItems(m_fileSystemTypes);
        m_fileSystem->setEnabled(m_fileSystemTypes.size() > 1);
        const auto found = std::find(m_fileSystemTypes.begin(), m_fileSystemTypes.end(), m_initialFsType);
        m_fileSystem->setCurrentIndex(found == m_fileSystemTypes.end() ? 0 : int(found - m_fileSystemTypes.begin()));
    }

    Mode m_mode;
    std::vector<std::string> m_available;
    std::vector<std::string> m_uses;
    std::vector<std::string> m_fileSystemTypes;
    std::string m_existingFsType;
    std::string m_initialFsType;
    uint64_t m_maximum = 0;
    bool m_sizeEdited = false;
    Label* m_sizeCaption;
    TextField* m_size;
    Label* m_unit;
    Label* m_useCaption;
    ComboBox* m_use;
    Label* m_fileSystemCaption;
    ComboBox* m_fileSystem;
    CheckBox* m_format;
    DialogButton* m_cancel;
    DialogButton* m_accept;
};

PartitionPage::PartitionPage(bool uefi)
    : m_uefi(uefi)
{
    setTitle(tr("Partition the Disk"));
    m_bar = content()->addChild<PartitionBar>(*this);
    m_card = content()->addChild<Card>();
    m_card->setClipsChildren(true);
    m_card->setChildClipRadius(kCardRadius);
    m_list = m_card->addChild<ListView>();
    m_list->setRowHeight(kRowHeight);
    m_list->setMargins({0, kCardInset, 0, kCardInset});
    m_list->createItem = [] { return std::make_unique<PartitionRow>(); };
    m_list->bindItem = [this](View* item, size_t row) {
        auto* partitionRow = static_cast<PartitionRow*>(item);
        const Entry& entry = m_entries[row];
        const installer::PlannedPartition* partition =
                entry.partition >= 0 ? &m_plan.partitions()[size_t(entry.partition)] : nullptr;
        partitionRow->bind(partition, entry.size, int(row) == m_selected);
        partitionRow->onClicked = [this, row] { select(int(row)); };
    };
    m_busy = content()->addChild<BusyIndicator>();

    m_newTable = content()->addChild<DialogButton>(tr("New Partition Table"));
    m_add = content()->addChild<DialogButton>(tr("Add…"));
    m_edit = content()->addChild<DialogButton>(tr("Edit…"));
    m_delete = content()->addChild<DialogButton>(tr("Delete"));
    m_newTable->onClicked = [this] { newTable(); };
    m_add->onClicked = [this] { addPartition(); };
    m_edit->onClicked = [this] { editPartition(); };
    m_delete->onClicked = [this] { deletePartition(); };

    for (const char* text : {tr("Partition"), tr("File system"), tr("Use as"), tr("Size")})
        m_headers.push_back(addLabel(content(), text, 12));
    m_headers.back()->setTextAlignment(Label::TextAlignment::Right);

    m_message = addLabel(content(), {}, 12);
    m_message->setWrap(true);
    setLoading({});
}

PartitionPage::~PartitionPage() = default;

void PartitionPage::setLoading(const std::string& diskName)
{
    m_loading = true;
    m_plan = installer::PartitionPlan();
    m_entries.clear();
    m_selected = -1;
    setSubtitle(format(tr("Nothing changes on {disk} until you install."), {{"disk", diskName}}));
    m_message->setText({});
    m_list->setRowCount(0);
    updateActions();
    requestLayout();
}

void PartitionPage::setLayout(installer::DiskLayout layout, std::vector<std::string> fileSystems)
{
    m_loading = false;
    m_fileSystems = std::move(fileSystems);
    m_plan = installer::PartitionPlan(std::move(layout));
    m_selected = -1;
    changed();
}

void PartitionPage::setPlanForCapture(installer::PartitionPlan plan)
{
    m_loading = false;
    m_plan = std::move(plan);
    changed();
}

void PartitionPage::setError(const std::string& error)
{
    m_loading = false;
    m_message->setText(format(tr("The partitions cannot be read: {error}"), {{"error", error}}));
    requestLayout();
}

bool PartitionPage::isComplete() const
{
    return !m_loading && m_plan.problem(m_uefi).empty();
}

void PartitionPage::select(int row)
{
    m_selected = row;
    m_list->reload();
    m_bar->requestPaint();
    updateActions();
}

void PartitionPage::changed()
{
    m_entries.clear();
    std::vector<installer::PartitionPlan::Region> free = m_plan.freeRegions();
    const auto& partitions = m_plan.partitions();
    size_t next = 0;
    for (size_t i = 0; i <= partitions.size(); ++i) {
        const uint64_t before = i < partitions.size() ? partitions[i].start : UINT64_MAX;
        while (next < free.size() && free[next].start < before) {
            m_entries.push_back({-1, free[next].start, free[next].size});
            ++next;
        }
        if (i < partitions.size())
            m_entries.push_back({int(i), partitions[i].start, partitions[i].size});
    }
    if (m_selected >= int(m_entries.size()))
        m_selected = int(m_entries.size()) - 1;
    m_list->setRowCount(m_entries.size());
    m_list->reload();
    m_bar->requestPaint();
    const std::string problem = m_plan.problem(m_uefi);
    m_message->setText(problem);
    updateActions();
    requestLayout();
    if (onChanged)
        onChanged();
}

void PartitionPage::updateActions()
{
    const bool loaded = !m_loading && !m_plan.layout().disk.empty();
    const Entry* entry = m_selected >= 0 && size_t(m_selected) < m_entries.size() ? &m_entries[size_t(m_selected)] : nullptr;
    m_newTable->setEnabled(loaded);
    m_add->setEnabled(loaded && entry && entry->partition < 0);
    m_edit->setEnabled(loaded && entry && entry->partition >= 0);
    m_delete->setEnabled(loaded && entry && entry->partition >= 0);
}

void PartitionPage::newTable()
{
    m_confirm = std::make_unique<ConfirmDialog>(
            m_window, tr("Create a new partition table?"),
            format(tr("Every partition on {disk} will be deleted when Cutefish is installed."),
                   {{"disk", m_plan.layout().disk}}));
    m_confirm->setConfirmText(tr("New Partition Table"));
    m_confirm->setDestructive(true);
    m_confirm->onAccepted = [this] {
        m_plan.clearTable();
        m_selected = 0;
        changed();
    };
    m_confirm->show();
}

void PartitionPage::addPartition()
{
    const Entry entry = m_entries[size_t(m_selected)];
    m_dialog = std::make_unique<DialogWindow>(m_window, tr("Add Partition"), kDialogWidth);
    auto* editor = m_dialog->setContent<PartitionEditor>(PartitionEditor::Mode::Add, m_uefi, m_plan.table() == "gpt",
                                                         m_fileSystems);
    // The system's partition first, while it has none.
    const bool hasRoot = std::any_of(m_plan.partitions().begin(), m_plan.partitions().end(),
                                     [](const installer::PlannedPartition& partition) { return partition.mountPoint == "/"; });
    editor->setValues(entry.size, hasRoot ? std::string() : std::string("/"), true, "ext4", {});
    editor->cancelButton()->onClicked = [this] { m_dialog->close(); };
    editor->acceptButton()->onClicked = [this, editor, entry] {
        const uint64_t size = editor->size();
        if (size >= installer::kMiB && m_plan.add(entry.start, size, editor->use(), editor->fsType())) {
            const uint64_t start = entry.start;
            changed();
            for (size_t i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].partition >= 0 && m_entries[i].start == start)
                    select(int(i));
            }
        }
        m_dialog->close();
    };
    m_dialog->show();
}

void PartitionPage::editPartition()
{
    const Entry entry = m_entries[size_t(m_selected)];
    const installer::PlannedPartition& partition = m_plan.partitions()[size_t(entry.partition)];
    m_dialog = std::make_unique<DialogWindow>(m_window, partition.isNew() ? tr("New Partition") : partition.path,
                                              kDialogWidth);
    const PartitionEditor::Mode mode =
            partition.isNew() ? PartitionEditor::Mode::EditNew : PartitionEditor::Mode::EditExisting;
    auto* editor = m_dialog->setContent<PartitionEditor>(mode, m_uefi, m_plan.table() == "gpt", m_fileSystems);
    editor->setValues(partition.size, partition.mountPoint, partition.format || partition.isNew(),
                      partition.format ? partition.fsType : "ext4", partition.existingFsType);
    editor->cancelButton()->onClicked = [this] { m_dialog->close(); };
    editor->acceptButton()->onClicked = [this, editor, entry] {
        m_plan.assign(size_t(entry.partition), editor->use(), editor->format(), editor->fsType());
        changed();
        m_dialog->close();
    };
    m_dialog->show();
}

void PartitionPage::deletePartition()
{
    const Entry entry = m_entries[size_t(m_selected)];
    m_plan.remove(size_t(entry.partition));
    changed();
}

double PartitionPage::contentWidth() const
{
    return kPartitionContentWidth;
}

double PartitionPage::contentHeight() const
{
    return kBarHeight + kGap + kHeaderHeight + kRowsShown * kRowHeight + 2 * kCardInset + kGap + kActionHeight + kGap
            + m_message->sizeHint().height;
}

void PartitionPage::layoutContent()
{
    const RectF bounds = content()->localRect();
    const double width = bounds.width;
    double y = 0;
    m_bar->setGeometry({0, y, width, kBarHeight});
    m_bar->setVisible(!m_loading);
    y += kBarHeight + kGap;
    const double headerHeight = m_headers.front()->sizeHint().height;
    const double headerTop = y + kHeaderHeight - 4 - headerHeight;
    const double columns[] = {kRowPadding + 20, kFileSystemColumn, kUseColumn};
    for (size_t i = 0; i < 3; ++i)
        m_headers[i]->setGeometry({columns[i], headerTop, 150, headerHeight});
    m_headers[3]->setGeometry({width - kRowPadding - 100, headerTop, 100, headerHeight});
    for (Label* header : m_headers) {
        header->setColor(Theme::current().disabledText);
        header->setVisible(!m_loading);
    }
    y += kHeaderHeight;
    const double listHeight = kRowsShown * kRowHeight + 2 * kCardInset;
    m_card->setGeometry({0, y, width, listHeight});
    m_list->setGeometry(m_card->localRect());
    m_busy->setVisible(m_loading);
    m_busy->setRunning(m_loading);
    m_busy->setGeometry({rounding::centeredStart(width, 22), y + rounding::centeredStart(listHeight, 22), 22, 22});
    y += listHeight + kGap;

    double x = 0;
    const auto place = [&](DialogButton* button, double left) {
        const double buttonWidth = std::max(88.0, button->sizeHint().width);
        button->setGeometry({left, y, buttonWidth, kActionHeight});
        return buttonWidth;
    };
    place(m_newTable, 0);
    for (DialogButton* button : {m_delete, m_edit, m_add}) {
        const double buttonWidth = std::max(88.0, button->sizeHint().width);
        x += buttonWidth;
        button->setGeometry({width - x, y, buttonWidth, kActionHeight});
        x += 8;
    }
    y += kActionHeight + kGap;
    m_message->setColor(accent::red);
    m_message->setGeometry({0, y, width, m_message->heightForWidth(width)});
}
