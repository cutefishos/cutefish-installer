// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/image.h"
#include "ui/label.h"
#include "ui/view.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cutefish::ui {
class Icon;
class ThemeIcon;
}

// The width of every page's text and content.
inline constexpr double kContentWidth = 480;
// The window the pages sit in.
inline constexpr double kWindowWidth = 880;
inline constexpr double kWindowHeight = 560;
// From the window's edges to its buttons and to a page's icon.
inline constexpr double kWindowMargin = 32;
// The window's buttons, kWindowMargin from its bottom.
inline constexpr double kButtonHeight = 36;
// The icon above a page's title.
inline constexpr double kIconSize = 64;
// The radius of the cards, and the gap a row's background keeps to them.
inline constexpr double kCardRadius = 12;
inline constexpr double kCardInset = 4;
// From a card's edges to the text and pictures on it.
inline constexpr double kCardPadding = 20;

// The path of one of the installer's own files, such as "logo.svg".
std::string dataPath(const std::string& name);
void setDataDirectory(std::string directory);

// The account pictures the system comes with, by path and in order.
std::vector<std::string> accountPictures();
// The path of the one named, as Options::picture names it.
std::string accountPicturePath(const std::string& name);

cutefish::ui::Label* addLabel(cutefish::ui::View* parent, std::string text, double size = 0, int weight = 400);

// The fill of cards and fields on the window's second background.
cutefish::ui::Color cardColor();

// An SVG drawn to fill the view, its shape kept.
class Picture : public cutefish::ui::View
{
public:
    explicit Picture(std::string path = {});

protected:
    void paint(cutefish::ui::Painter& painter) override;

private:
    std::string m_path;
};

// A plate of cardColor() on the window, the rows of a list or a form on it.
class Card : public cutefish::ui::View
{
public:
    // Lines between rows of this height, from the rows' text edge.
    void setRowSeparators(double rowHeight);

protected:
    void paint(cutefish::ui::Painter& painter) override;

private:
    double m_rowHeight = 0;
};

// A row of a card to choose: an icon and a title over a detail, with a check
// mark at its end on the one chosen. It reaches the card's edges, and a line
// parts it from the row under it.
class ChoiceRow : public cutefish::ui::View
{
public:
    ChoiceRow();

    void setIconName(const std::string& name);
    void setTitle(std::string title);
    void setDetail(std::string detail);
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void setSeparatorVisible(bool visible);
    // The card it lies on, whose corners its pressed background keeps to.
    void setCard(const cutefish::ui::View* card);

    // With a title alone, and with a detail under it.
    static constexpr double kHeight = 44;
    static constexpr double kTallHeight = 60;

    bool acceptsPointer() const override { return isEnabled(); }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    void enabledChanged() override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;

private:
    double textLeft() const;

    cutefish::ui::ThemeIcon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_detail;
    cutefish::ui::Icon* m_check;
    const cutefish::ui::View* m_card = nullptr;
    bool m_selected = false;
    bool m_separator = true;
    bool m_pressed = false;
};

// An account picture, cut to a circle. With onClicked it is a button.
class AccountPicture : public cutefish::ui::View
{
public:
    explicit AccountPicture(const std::string& file = {});

    void setFile(const std::string& file);
    const std::string& file() const { return m_file; }
    // A ring of the highlight around it, for the one chosen among several.
    void setSelected(bool selected);

    bool acceptsPointer() const override { return bool(onClicked); }

    std::function<void()> onClicked;

protected:
    void paint(cutefish::ui::Painter& painter) override;
    void hoverChanged() override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;

private:
    std::string m_file;
    cutefish::ui::Image m_image;
    bool m_selected = false;
    bool m_pressed = false;
};

// A row of the summary: a picture, a title and a line under it. Text too
// long for one line wraps, and the row grows by its lines.
class SummaryItem : public cutefish::ui::View
{
public:
    SummaryItem();

    void setPicture(std::unique_ptr<cutefish::ui::View> picture);
    void setText(std::string title, std::string detail);
    void setSeparatorVisible(bool visible);
    double heightForWidth(double width) const override;

    // With a title and a detail of one line each.
    static constexpr double kHeight = 60;
    static constexpr double kPictureSize = 36;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    double textHeight(double width) const;

    cutefish::ui::View* m_picture = nullptr;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_detail;
    bool m_separator = true;
};

// A line under a card that must not be missed: on a tint of red, a warning
// sign before it. Not a warning, it is a plain centred line.
class Callout : public cutefish::ui::View
{
public:
    Callout();

    void setText(std::string text, bool warning);
    double heightForWidth(double width) const override;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    cutefish::ui::Icon* m_sign;
    cutefish::ui::Label* m_text;
    bool m_warning = true;
};

// A page of the installer: an icon, a title and a line under it, then the
// page's content.
class Page : public cutefish::ui::View
{
public:
    enum class Kind {
        // The header at the top, where every such page has it, and the
        // content centred in the space left above the buttons.
        Task,
        // Header and content as one block, centred above the buttons.
        Hero,
    };

    explicit Page(Kind kind = Kind::Task);

    void setIcon(std::unique_ptr<cutefish::ui::View> icon, double width, double height);
    void setThemeIcon(const std::string& name);
    void setTitle(std::string title);
    void setSubtitle(std::string subtitle);

    // Under the header, contentWidth() wide.
    cutefish::ui::View* content() const { return m_content; }
    virtual double contentWidth() const { return kContentWidth; }
    virtual double contentHeight() const { return 0; }

    // Taken off the bottom of the page for the window's buttons.
    static constexpr double kFooterHeight = 88;

protected:
    void layout() override;
    virtual void layoutContent() {}

private:
    Kind m_kind;
    cutefish::ui::View* m_icon = nullptr;
    double m_iconWidth = 0;
    double m_iconHeight = 0;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_subtitle;
    cutefish::ui::View* m_content;
};

// "476.9 GB", in powers of ten as disks are sold; a line never breaks before
// the unit.
std::string formatSize(uint64_t bytes);
