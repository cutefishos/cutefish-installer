// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/label.h"
#include "ui/view.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace cutefish::ui {
class Icon;
class ThemeIcon;
}

// The width of every page's text and content.
inline constexpr double kContentWidth = 480;
// The rounded panel the pages sit in, over the wallpaper.
inline constexpr double kPanelWidth = 880;
inline constexpr double kPanelHeight = 560;
inline constexpr double kPanelRadius = 28;
// The icon above a page's title.
inline constexpr double kIconSize = 56;
// The radius of the cards, and the gap a row's background keeps to them.
inline constexpr double kCardRadius = 12;
inline constexpr double kCardInset = 4;

// The path of one of the installer's own files, such as "logo.svg".
std::string dataPath(const std::string& name);
void setDataDirectory(std::string directory);

cutefish::ui::Label* addLabel(cutefish::ui::View* parent, std::string text, double size = 0, int weight = 400);

// The fill of cards and fields on the panel.
cutefish::ui::Color cardColor();

// The panel: the theme's second background with continuous corners, its
// children clipped to it.
class Panel : public cutefish::ui::View
{
public:
    Panel();

protected:
    void paint(cutefish::ui::Painter& painter) override;
};

// The panel's shadow over the wallpaper, PanelShadow::kReach beyond it all round.
class PanelShadow : public cutefish::ui::View
{
public:
    static constexpr double kReach = 64;

protected:
    void paint(cutefish::ui::Painter& painter) override;
};

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

// A plate of cardColor() on the panel, the rows of a list or a summary on it.
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

// A row of a card to choose: an icon, a title over a detail, a note on the
// right and a check mark when chosen. Its background keeps kCardInset to the
// card.
class ChoiceRow : public cutefish::ui::View
{
public:
    ChoiceRow();

    void setIconName(const std::string& name);
    void setTitle(std::string title);
    void setDetail(std::string detail);
    void setNote(std::string note);
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    bool acceptsPointer() const override { return isEnabled(); }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    void hoverChanged() override;
    void enabledChanged() override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;

private:
    cutefish::ui::ThemeIcon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_detail;
    cutefish::ui::Label* m_note;
    cutefish::ui::Icon* m_check;
    bool m_selected = false;
    bool m_pressed = false;
};

// A row of a summary: what on the left, its value on the right.
class SummaryRow : public cutefish::ui::View
{
public:
    SummaryRow(std::string text, std::string value);

    void setValue(std::string value);
    void setSeparatorVisible(bool visible);

    static constexpr double kHeight = 44;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    cutefish::ui::Label* m_text;
    cutefish::ui::Label* m_value;
    bool m_separator = true;
};

// A page of the installer: an icon, a title and a line under it, then the
// page's content, all centred in the space above the buttons.
class Page : public cutefish::ui::View
{
public:
    Page();

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
    cutefish::ui::View* m_icon = nullptr;
    double m_iconWidth = 0;
    double m_iconHeight = 0;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_subtitle;
    cutefish::ui::View* m_content;
};

// "476.9 GB", in powers of ten as disks are sold.
std::string formatSize(uint64_t bytes);
