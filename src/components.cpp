// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "components.h"

#include "ui/cell_layout.h"
#include "ui/color_provider.h"
#include "ui/icon.h"
#include "ui/painter.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace cutefish::ui;

// From the top of the window to a page's icon.
static constexpr double kTopMargin = kWindowMargin;
// Between a page's header and its content.
static constexpr double kContentGap = 24;
// The column of titles and subtitles too long for kContentWidth.
static constexpr double kWideColumnWidth = 720;
// A row's picture and the text beside it, and the title over the detail.
static constexpr double kRowPictureGap = 12;
static constexpr double kRowTextGap = 2;
static constexpr double kCheckSize = 16;
static constexpr double kCheckGap = 12;
// The check mark's drawing ends this far inside its box.
static constexpr double kCheckOverhang = 2;
static constexpr double kCalloutPadding = 16;
static constexpr double kCalloutVertical = 12;
static constexpr double kCalloutSign = 16;
static constexpr double kCalloutSignGap = 10;
// How far the cards' fill stands off the window's, of the theme's own step.
static constexpr double kCardTint = 0.6;
static constexpr char kAccountPictures[] = "/usr/share/cutefish/avatars";
static constexpr char kWarningSign[] =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\" viewBox=\"0 0 24 24\">"
        "<path fill=\"currentColor\" fill-rule=\"evenodd\" d=\"M10.27 3.9a2 2 0 0 1 3.46 0l8 13.9A2 2 0 0 1 20 20.8H4"
        "a2 2 0 0 1-1.73-3l8-13.9ZM12 8.5a1 1 0 0 0-1 1v4.2a1 1 0 0 0 2 0V9.5a1 1 0 0 0-1-1Zm0 7.4"
        "a1.15 1.15 0 1 0 0 2.3 1.15 1.15 0 0 0 0-2.3Z\"/></svg>";

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

std::vector<std::string> accountPictures()
{
    std::vector<std::string> pictures;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(kAccountPictures, error)) {
        if (entry.is_regular_file(error))
            pictures.push_back(entry.path().string());
    }
    std::sort(pictures.begin(), pictures.end());
    return pictures;
}

std::string accountPicturePath(const std::string& name)
{
    return std::string(kAccountPictures) + '/' + name;
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
    const Color window = theme.secondBackground;
    const Color plate = theme.darkMode ? theme.alternateBackground : theme.background;
    const auto mixed = [](uint8_t from, uint8_t to) {
        return uint8_t(std::lround(from + (to - from) * kCardTint));
    };
    return {mixed(window.r, plate.r), mixed(window.g, plate.g), mixed(window.b, plate.b)};
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
    painter.fillRoundedRect(bounds, kCardRadius, cardColor(), themeCorners());
    if (m_rowHeight <= 0)
        return;
    for (double y = m_rowHeight; y + 1 < bounds.height; y += m_rowHeight)
        painter.drawHorizontalLine(kCardPadding, y - 1 / painter.scale(), bounds.width - 2 * kCardPadding,
                                   color(ColorId::Separator));
}

ChoiceRow::ChoiceRow()
{
    m_icon = addChild<ThemeIcon>(std::string(), SummaryItem::kPictureSize);
    m_icon->setVisible(false);
    m_title = addLabel(this, {}, 14);
    m_title->setElide(Label::Elide::Right);
    m_detail = addLabel(this, {}, 12);
    m_detail->setElide(Label::Elide::Right);
    m_detail->setVisible(false);
    m_check = addChild<Icon>("check", kCheckSize);
    m_check->setVisible(false);
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
    m_detail->setVisible(!m_detail->text().empty());
    // The title of two lines leads; alone it reads as the list's text.
    m_title->setWeight(m_detail->isVisible() ? 500 : 400);
    requestLayout();
}

void ChoiceRow::setSelected(bool selected)
{
    m_selected = selected;
    m_check->setVisible(selected);
}

void ChoiceRow::setSeparatorVisible(bool visible)
{
    if (m_separator == visible)
        return;
    m_separator = visible;
    requestPaint();
}

void ChoiceRow::setCard(const View* card)
{
    m_card = card;
}

double ChoiceRow::textLeft() const
{
    return m_icon->isVisible() ? kCardPadding + SummaryItem::kPictureSize + kRowPictureGap : kCardPadding;
}

void ChoiceRow::layout()
{
    const Theme& theme = Theme::current();
    const double width = geometry().width;
    const double height = geometry().height;
    const double picture = SummaryItem::kPictureSize;
    m_icon->setGeometry({kCardPadding, rounding::centeredStart(height, picture), picture, picture});
    m_check->setColor(theme.highlight);
    const double checkLeft = width - kCardPadding + kCheckOverhang - kCheckSize;
    m_check->setGeometry({checkLeft, rounding::centeredStart(height, kCheckSize), kCheckSize, kCheckSize});

    // The check mark's place is kept on every row, so their text ends alike.
    const double left = textLeft();
    const double textWidth = std::max(0.0, checkLeft - kCheckGap - left);
    const double titleHeight = m_title->sizeHint().height;
    if (!m_detail->isVisible()) {
        m_title->setGeometry({left, rounding::centeredStart(height, titleHeight), textWidth, titleHeight});
        return;
    }
    const double detailHeight = m_detail->sizeHint().height;
    const double top = rounding::centeredStart(height, titleHeight + kRowTextGap + detailHeight);
    m_title->setGeometry({left, top, textWidth, titleHeight});
    m_detail->setColor(theme.disabledText);
    m_detail->setGeometry({left, top + titleHeight + kRowTextGap, textWidth, detailHeight});
}

void ChoiceRow::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    if (m_pressed && isEnabled()) {
        painter.save();
        if (m_card) {
            const PointF origin = mapToWindow({0, 0});
            const PointF card = m_card->mapToWindow({0, 0});
            painter.clipRoundedRect({card.x - origin.x, card.y - origin.y, m_card->geometry().width,
                                     m_card->geometry().height},
                                    kCardRadius, themeCorners());
        }
        painter.fillRect(bounds, theme.text.withAlpha(theme.darkMode ? 0.05 : 0.15));
        painter.restore();
        return;
    }
    if (m_separator)
        painter.drawHorizontalLine(textLeft(), bounds.height - 1 / painter.scale(),
                                   bounds.width - kCardPadding - textLeft(), color(ColorId::Separator));
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
    // A list binds its rows again from the click, which replaces onClicked.
    if (const std::function<void()> clicked = onClicked; clicked && localRect().contains(event.position))
        clicked();
}

AccountPicture::AccountPicture(const std::string& file)
{
    setTracksHover(true);
    setFile(file);
}

void AccountPicture::setFile(const std::string& file)
{
    if (file == m_file)
        return;
    m_file = file;
    m_image = file.empty() ? Image() : Image::fromFile(file);
    requestPaint();
}

void AccountPicture::setSelected(bool selected)
{
    m_selected = selected;
    requestPaint();
}

void AccountPicture::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    const double radius = std::min(bounds.width, bounds.height) / 2;
    if (m_image.isNull()) {
        painter.fillEllipse(bounds, theme.text.withAlpha(0.08));
    } else {
        painter.save();
        painter.clipRoundedRect(bounds, radius);
        painter.drawImage(m_image, bounds);
        painter.restore();
    }
    if (onClicked && (m_pressed || isHovered()))
        painter.fillEllipse(bounds, Color::rgb(0x000000).withAlpha(m_pressed ? 0.2 : 0.1));
    if (m_selected)
        painter.strokeRoundedRect(bounds.adjusted(-3, -3, 3, 3), radius + 3, theme.highlight, painter.borderWidth(2));
}

void AccountPicture::hoverChanged()
{
    requestPaint();
}

void AccountPicture::pointerPress(const PointerEvent&)
{
    m_pressed = true;
    requestPaint();
}

void AccountPicture::pointerRelease(const PointerEvent& event)
{
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position) && onClicked)
        onClicked();
}

SummaryItem::SummaryItem()
{
    m_title = addLabel(this, {}, 14, 500);
    m_title->setWrap(true);
    m_detail = addLabel(this, {}, 12);
    m_detail->setWrap(true);
}

void SummaryItem::setPicture(std::unique_ptr<View> picture)
{
    if (m_picture)
        takeChild(m_picture);
    m_picture = picture.get();
    addChild(std::move(picture));
    requestLayout();
}

void SummaryItem::setText(std::string title, std::string detail)
{
    m_title->setText(std::move(title));
    m_detail->setText(std::move(detail));
    requestLayout();
}

void SummaryItem::setSeparatorVisible(bool visible)
{
    m_separator = visible;
    requestPaint();
}

static double summaryTextLeft()
{
    return kCardPadding + SummaryItem::kPictureSize + kRowPictureGap;
}

double SummaryItem::textHeight(double width) const
{
    const double textWidth = std::max(0.0, width - summaryTextLeft() - kCardPadding);
    return m_title->heightForWidth(textWidth) + kRowTextGap + m_detail->heightForWidth(textWidth);
}

double SummaryItem::heightForWidth(double width) const
{
    const double oneLine = m_title->sizeHint().height + kRowTextGap + m_detail->sizeHint().height;
    return kHeight - oneLine + textHeight(width);
}

void SummaryItem::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    if (m_picture)
        m_picture->setGeometry({kCardPadding, rounding::centeredStart(height, kPictureSize), kPictureSize, kPictureSize});
    const double left = summaryTextLeft();
    const double textWidth = std::max(0.0, width - left - kCardPadding);
    const double titleHeight = m_title->heightForWidth(textWidth);
    const double top = rounding::centeredStart(height, textHeight(width));
    m_title->setGeometry({left, top, textWidth, titleHeight});
    m_detail->setColor(Theme::current().disabledText);
    m_detail->setGeometry({left, top + titleHeight + kRowTextGap, textWidth, m_detail->heightForWidth(textWidth)});
}

void SummaryItem::paint(Painter& painter)
{
    if (m_separator) {
        const RectF bounds = localRect();
        painter.drawHorizontalLine(summaryTextLeft(), bounds.height - 1 / painter.scale(),
                                   bounds.width - kCardPadding - summaryTextLeft(), color(ColorId::Separator));
    }
}

Callout::Callout()
{
    Icon::registerGlyph("installer-warning", kWarningSign);
    m_sign = addChild<Icon>("installer-warning", kCalloutSign);
    m_sign->setColor(accent::red);
    m_text = addLabel(this, {}, 12);
    m_text->setWrap(true);
}

void Callout::setText(std::string text, bool warning)
{
    m_text->setText(std::move(text));
    m_warning = warning;
    m_sign->setVisible(warning);
    m_text->setTextAlignment(warning ? Label::TextAlignment::Left : Label::TextAlignment::Center);
    requestLayout();
    requestPaint();
}

static double calloutTextLeft(bool warning)
{
    return warning ? kCalloutPadding + kCalloutSign + kCalloutSignGap : 0;
}

double Callout::heightForWidth(double width) const
{
    const double textWidth = std::max(0.0, width - calloutTextLeft(m_warning) - (m_warning ? kCalloutPadding : 0));
    return m_text->heightForWidth(textWidth) + 2 * kCalloutVertical;
}

void Callout::layout()
{
    const RectF bounds = localRect();
    const double left = calloutTextLeft(m_warning);
    const double textWidth = std::max(0.0, bounds.width - left - (m_warning ? kCalloutPadding : 0));
    const double textHeight = m_text->heightForWidth(textWidth);
    m_text->setColor(m_warning ? std::nullopt : std::optional<ColorRecipe>(Theme::current().disabledText));
    m_text->setGeometry({left, rounding::centeredStart(bounds.height, textHeight), textWidth, textHeight});
    // Beside the first line, however many follow.
    const double firstLine = m_text->sizeHint().height;
    m_sign->setGeometry({kCalloutPadding, m_text->geometry().y + rounding::centeredStart(firstLine, kCalloutSign),
                         kCalloutSign, kCalloutSign});
}

void Callout::paint(Painter& painter)
{
    if (m_warning)
        painter.fillRoundedRect(localRect(), kCardRadius, accent::red.withAlpha(Theme::current().darkMode ? 0.2 : 0.12),
                                themeCorners());
}

Page::Page(Kind kind)
    : m_kind(kind)
{
    const bool hero = kind == Kind::Hero;
    m_title = addLabel(this, {}, hero ? 32 : 28, 700);
    m_title->setLetterSpacing(hero ? -0.5 : -0.4);
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
    const bool hero = m_kind == Kind::Hero;
    const double iconGap = hero ? 24 : 16;
    const double titleGap = hero ? 8 : 4;
    const double width = geometry().width;
    const double bottom = std::max(0.0, geometry().height - kFooterHeight);
    const double available = std::max(0.0, width - 2 * kWindowMargin);
    const double contentWidth = std::min(this->contentWidth(), available);
    const double contentHeight = this->contentHeight();
    double columnWidth = 0;
    double titleHeight = 0;
    double subtitleHeight = 0;
    double header = 0;
    bool showsIcon = m_icon != nullptr;
    const auto measure = [&](double column) {
        columnWidth = std::min(column, available);
        titleHeight = m_title->heightForWidth(columnWidth);
        subtitleHeight = m_subtitle->isVisible() ? m_subtitle->heightForWidth(columnWidth) : 0;
        header = titleHeight;
        if (showsIcon)
            header += m_iconHeight + iconGap;
        if (subtitleHeight > 0)
            header += titleGap + subtitleHeight;
    };
    measure(kContentWidth);
    // Longer text, as another language has it, takes one line across more
    // of the window, so the content starts where it does on every page.
    if (titleHeight > m_title->sizeHint().height + 0.5
        || (subtitleHeight > 0 && subtitleHeight > m_subtitle->sizeHint().height + 0.5))
        measure(kWideColumnWidth);
    const double below = contentHeight > 0 ? kContentGap + contentHeight : 0;
    // Then without the icon rather than reaching the buttons.
    if (kTopMargin + header + below > bottom && showsIcon) {
        showsIcon = false;
        measure(columnWidth);
    }
    const double left = rounding::centeredStart(width, columnWidth);

    double y = hero ? std::max(kTopMargin, rounding::centeredStart(bottom, header + below)) : kTopMargin;
    if (m_icon)
        m_icon->setVisible(showsIcon);
    if (showsIcon) {
        m_icon->setGeometry({rounding::centeredStart(width, m_iconWidth), y, m_iconWidth, m_iconHeight});
        y += m_iconHeight + iconGap;
    }
    m_title->setGeometry({left, y, columnWidth, titleHeight});
    y += titleHeight;
    m_subtitle->setColor(Theme::current().disabledText);
    if (subtitleHeight > 0) {
        y += titleGap;
        m_subtitle->setGeometry({left, y, columnWidth, subtitleHeight});
        y += subtitleHeight;
    }
    if (contentHeight > 0)
        y += kContentGap;
    if (!hero)
        y += std::max(0.0, rounding::centeredStart(bottom - y, contentHeight));
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
        std::snprintf(text, sizeof text, "%.0f\u00a0%s", value, kUnits[unit]);
    else
        std::snprintf(text, sizeof text, "%.1f\u00a0%s", value, kUnits[unit]);
    return text;
}
