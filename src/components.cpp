// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "components.h"

#include "ui/cell_layout.h"
#include "ui/color_provider.h"
#include "ui/icon.h"
#include "ui/painter.h"
#include "ui/theme.h"

#include <algorithm>
#include <cstdio>

using namespace cutefish::ui;

static constexpr double kIconGap = 20;
static constexpr double kTitleGap = 8;
static constexpr double kContentGap = 28;
// The least space above a page's icon.
static constexpr double kTopMargin = 32;
// Around the rows' text, from the card's edges.
static constexpr double kRowPadding = 16;

static std::string& dataDirectory()
{
    static std::string directory = INSTALLER_DATA_DIR;
    return directory;
}

std::string dataPath(const std::string& name)
{
    return dataDirectory() + "/" + name;
}

void setDataDirectory(std::string directory)
{
    dataDirectory() = std::move(directory);
}

Label* addLabel(View* parent, std::string text, double size, int weight)
{
    Label* label = parent->addChild<Label>(std::move(text));
    if (size > 0)
        label->setFontSize(size);
    label->setWeight(weight);
    return label;
}

Color cardColor()
{
    const Theme& theme = Theme::current();
    return theme.darkMode ? theme.alternateBackground : theme.background;
}

Panel::Panel()
{
    setClipsChildren(true);
    setChildClipRadius(kPanelRadius);
}

void Panel::paint(Painter& painter)
{
    painter.drawSmoothRectangle(localRect(), kPanelRadius, Theme::current().secondBackground);
}

void PanelShadow::paint(Painter& painter)
{
    const RectF panel = localRect().adjusted(kReach, kReach, -kReach, -kReach);
    painter.drawShadow(panel, kPanelRadius, 32, Color{0, 0, 0, 44}, {0, 12});
}

Picture::Picture(std::string path)
    : m_path(std::move(path))
{
}

void Picture::paint(Painter& painter)
{
    if (!m_path.empty())
        painter.drawSvg(m_path, localRect());
}

void Card::setRowSeparators(double rowHeight)
{
    m_rowHeight = rowHeight;
    requestPaint();
}

void Card::paint(Painter& painter)
{
    const RectF bounds = localRect();
    painter.fillRoundedRect(bounds, kCardRadius, cardColor());
    if (m_rowHeight <= 0)
        return;
    for (double y = m_rowHeight; y + 1 < bounds.height; y += m_rowHeight)
        painter.drawHorizontalLine(kRowPadding, y - 1 / painter.scale(), bounds.width - kRowPadding,
                                   color(ColorId::Separator));
}

ChoiceRow::ChoiceRow()
{
    m_icon = addChild<ThemeIcon>(std::string(), 32);
    m_icon->setVisible(false);
    m_title = addLabel(this, {}, 14, 500);
    m_title->setElide(Label::Elide::Right);
    m_detail = addLabel(this, {}, 12);
    m_detail->setElide(Label::Elide::Right);
    m_note = addLabel(this, {}, 12);
    m_check = addChild<Icon>("check", 16);
    m_check->setVisible(false);
    setTracksHover(true);
}

void ChoiceRow::setIconName(const std::string& name)
{
    m_icon->setIconName(name);
    m_icon->setVisible(!name.empty());
    requestLayout();
}

void ChoiceRow::setTitle(std::string title)
{
    m_title->setText(std::move(title));
}

void ChoiceRow::setDetail(std::string detail)
{
    m_detail->setText(std::move(detail));
    requestLayout();
}

void ChoiceRow::setNote(std::string note)
{
    m_note->setText(std::move(note));
    requestLayout();
}

void ChoiceRow::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    m_check->setVisible(selected);
    requestLayout();
    requestPaint();
}

void ChoiceRow::layout()
{
    const Theme& theme = Theme::current();
    const double width = geometry().width;
    const double height = geometry().height;
    double x = kRowPadding;
    if (m_icon->isVisible()) {
        m_icon->setGeometry({x, rounding::centeredStart(height, 32), 32, 32});
        x += 32 + 12;
    }

    double right = width - kRowPadding;
    m_check->setColor(theme.highlight);
    m_check->setGeometry({right - 16, rounding::centeredStart(height, 16), 16, 16});
    if (m_selected)
        right -= 16 + 12;
    const double noteWidth = m_note->text().empty() ? 0 : m_note->sizeHint().width;
    m_note->setColor(theme.disabledText);
    m_note->setGeometry({right - noteWidth, rounding::centeredStart(height, m_note->sizeHint().height), noteWidth,
                         m_note->sizeHint().height});
    if (noteWidth > 0)
        right -= noteWidth + 12;

    const double textWidth = std::max(0.0, right - x);
    const double titleHeight = m_title->sizeHint().height;
    m_detail->setColor(theme.disabledText);
    m_detail->setVisible(!m_detail->text().empty());
    if (!m_detail->isVisible()) {
        m_title->setGeometry({x, rounding::centeredStart(height, titleHeight), textWidth, titleHeight});
        return;
    }
    const double detailHeight = m_detail->sizeHint().height;
    const double top = rounding::centeredStart(height, titleHeight + 2 + detailHeight);
    m_title->setGeometry({x, top, textWidth, titleHeight});
    m_detail->setGeometry({x, top + titleHeight + 2, textWidth, detailHeight});
}

void ChoiceRow::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF background = localRect().adjusted(kCardInset, 0, -kCardInset, 0);
    const double radius = kCardRadius - kCardInset;
    if (m_selected)
        painter.fillRoundedRect(background, radius, theme.highlight.withAlpha(theme.darkMode ? 0.24 : 0.1));
    else if (isEnabled() && (m_pressed || isHovered()))
        painter.fillRoundedRect(background, radius, theme.text.withAlpha(m_pressed ? 0.08 : 0.04));
}

void ChoiceRow::hoverChanged()
{
    requestPaint();
}

void ChoiceRow::enabledChanged()
{
    setOpacity(isEnabled() ? 1 : 0.5);
}

void ChoiceRow::pointerPress(const PointerEvent&)
{
    m_pressed = true;
    requestPaint();
}

void ChoiceRow::pointerRelease(const PointerEvent& event)
{
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position) && onClicked)
        onClicked();
}

SummaryRow::SummaryRow(std::string text, std::string value)
{
    m_text = addLabel(this, std::move(text), 14);
    m_value = addLabel(this, std::move(value), 14);
    m_value->setElide(Label::Elide::Right);
    m_value->setTextAlignment(Label::TextAlignment::Right);
}

void SummaryRow::setValue(std::string value)
{
    m_value->setText(std::move(value));
}

void SummaryRow::setSeparatorVisible(bool visible)
{
    m_separator = visible;
    requestPaint();
}

void SummaryRow::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    const SizeF text = m_text->sizeHint();
    m_text->setGeometry({kRowPadding, rounding::centeredStart(height, text.height), text.width, text.height});
    const double valueLeft = kRowPadding + text.width + 24;
    const double valueHeight = m_value->sizeHint().height;
    m_value->setColor(Theme::current().disabledText);
    m_value->setGeometry({valueLeft, rounding::centeredStart(height, valueHeight),
                          std::max(0.0, width - kRowPadding - valueLeft), valueHeight});
}

void SummaryRow::paint(Painter& painter)
{
    if (m_separator) {
        const RectF bounds = localRect();
        painter.drawHorizontalLine(kRowPadding, bounds.height - 1 / painter.scale(), bounds.width - kRowPadding,
                                   color(ColorId::Separator));
    }
}

Page::Page()
{
    m_title = addLabel(this, {}, 24, 600);
    m_title->setTextAlignment(Label::TextAlignment::Center);
    m_title->setWrap(true);
    m_subtitle = addLabel(this, {}, 14);
    m_subtitle->setTextAlignment(Label::TextAlignment::Center);
    m_subtitle->setWrap(true);
    m_content = addChild<View>();
}

void Page::setIcon(std::unique_ptr<View> icon, double width, double height)
{
    if (m_icon)
        takeChild(m_icon);
    m_icon = icon.get();
    m_iconWidth = width;
    m_iconHeight = height;
    addChild(std::move(icon));
    requestLayout();
}

void Page::setThemeIcon(const std::string& name)
{
    setIcon(std::make_unique<ThemeIcon>(name, kIconSize), kIconSize, kIconSize);
}

void Page::setTitle(std::string title)
{
    m_title->setText(std::move(title));
    requestLayout();
}

void Page::setSubtitle(std::string subtitle)
{
    m_subtitle->setText(std::move(subtitle));
    m_subtitle->setVisible(!m_subtitle->text().empty());
    requestLayout();
}

void Page::layout()
{
    const double width = geometry().width;
    const double height = std::max(0.0, geometry().height - kFooterHeight);
    const double columnWidth = std::min(kContentWidth, std::max(0.0, width - 32));
    const double left = rounding::centeredStart(width, columnWidth);
    const double contentWidth = std::min(this->contentWidth(), std::max(0.0, width - 32));

    const double titleHeight = m_title->heightForWidth(columnWidth);
    const double subtitleHeight = m_subtitle->isVisible() ? m_subtitle->heightForWidth(columnWidth) : 0;
    const double contentHeight = this->contentHeight();
    double block = titleHeight;
    if (m_icon)
        block += m_iconHeight + kIconGap;
    if (subtitleHeight > 0)
        block += kTitleGap + subtitleHeight;
    if (contentHeight > 0)
        block += kContentGap + contentHeight;

    double y = std::max(kTopMargin, rounding::centeredStart(height, block));
    if (m_icon) {
        m_icon->setGeometry({rounding::centeredStart(width, m_iconWidth), y, m_iconWidth, m_iconHeight});
        y += m_iconHeight + kIconGap;
    }
    m_title->setGeometry({left, y, columnWidth, titleHeight});
    y += titleHeight;
    m_subtitle->setColor(Theme::current().disabledText);
    if (subtitleHeight > 0) {
        y += kTitleGap;
        m_subtitle->setGeometry({left, y, columnWidth, subtitleHeight});
        y += subtitleHeight;
    }
    if (contentHeight > 0)
        y += kContentGap;
    m_content->setGeometry({rounding::centeredStart(width, contentWidth), y, contentWidth, contentHeight});
    layoutContent();
}

std::string formatSize(uint64_t bytes)
{
    static const char* const kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = double(bytes);
    size_t unit = 0;
    while (value >= 1000 && unit + 1 < std::size(kUnits)) {
        value /= 1000;
        ++unit;
    }
    char text[32];
    if (unit == 0 || value >= 100)
        std::snprintf(text, sizeof text, "%.0f %s", value, kUnits[unit]);
    else
        std::snprintf(text, sizeof text, "%.1f %s", value, kUnits[unit]);
    return text;
}
