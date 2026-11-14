// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "backend.h"
#include "components.h"
#include "languages.h"
#include "timezones.h"

#include "common/installer.h"
#include "common/partitioning.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class BusyIndicator;
class DropDownPopup;
class ListView;
class ProgressBar;
class SearchField;
class TextField;
}


// The languages, each in its own name; the one chosen is spoken from then on.
class LanguagePage : public Page
{
public:
    LanguagePage();

    const InstallerLanguage& selectedLanguage() const;
    // Its own text again, in the language now spoken.
    void retranslate();

    std::function<void()> onSelectionChanged;

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    Card* m_card;
    cutefish::ui::ListView* m_list;
    size_t m_selected = 0;
    bool m_scrolled = false;
};

class WelcomePage : public Page
{
public:
    WelcomePage();
};

// The disks to install on, one chosen: side by side as tiles, or as a list
// when there are more than fit.
class DiskPage : public Page
{
public:
    DiskPage();

    void setLoading();
    // `preferred` is chosen again when it is still there.
    void setDisks(std::vector<DiskInfo> disks, const std::string& preferred = {});
    void setError(const std::string& error);
    // Null while none is chosen.
    const DiskInfo* selectedDisk() const;
    // The disks and the choice of a page made in another language.
    void takeStateFrom(const DiskPage& other);

    std::function<void()> onSelectionChanged;

    double contentWidth() const override;
    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    class Tile;

    bool isUsable(const DiskInfo& disk) const;
    std::string detailOf(const DiskInfo& disk, bool withPath) const;
    void select(int index);
    bool showsTiles() const;
    double tileHeight() const;
    double listHeight() const;

    cutefish::ui::View* m_row;
    std::vector<Tile*> m_tiles;
    Card* m_card;
    cutefish::ui::ListView* m_list;
    cutefish::ui::BusyIndicator* m_busy;
    cutefish::ui::Label* m_message;
    std::vector<DiskInfo> m_disks;
    int m_selected = -1;
    bool m_loading = true;
};

// The first user and their picture, and the computer's name.
class AccountPage : public Page
{
public:
    AccountPage();
    ~AccountPage() override;

    // Everything filled in and valid.
    bool isComplete() const;
    void fill(installer::Options& options) const;
    void focusFirstField();
    void takeStateFrom(const AccountPage& other);

    std::function<void()> onChanged;
    // Enter in the last field.
    std::function<void()> onAccepted;

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    class Form;
    class PictureGrid;

    struct Field
    {
        cutefish::ui::Label* caption;
        cutefish::ui::TextField* field;
    };

    Field addField(const std::string& caption);
    cutefish::ui::TextField* addTextField();
    void choosePicture();
    std::string problem() const;
    void changed();

    // Null without pictures to choose from.
    AccountPicture* m_picture = nullptr;
    std::unique_ptr<cutefish::ui::DropDownPopup> m_picker;
    PictureGrid* m_grid = nullptr;
    Form* m_card;
    Field m_fullName;
    Field m_userName;
    Field m_hostName;
    Field m_password;
    cutefish::ui::TextField* m_confirm;
    cutefish::ui::Label* m_message;
    bool m_userNameEdited = false;
    bool m_hostNameEdited = false;
};

// The zone chosen is a pin on the map; a press on the map takes the nearest
// city's, and the search lists the cities and countries by name.
class TimeZonePage : public Page
{
public:
    TimeZonePage();

    // Null while none is chosen.
    const TimeZone* selectedZone() const;
    void scrollToSelection();
    // What the search field holds.
    void setQuery(const std::string& query);
    void takeStateFrom(const TimeZonePage& other);

    std::function<void()> onSelectionChanged;

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    void select(int index);
    void filter();

    cutefish::ui::SearchField* m_search;
    Card* m_card;
    cutefish::ui::ListView* m_list;
    cutefish::ui::Label* m_empty;
    std::vector<TimeZone> m_zones;
    std::vector<size_t> m_shown;
    int m_selected = -1;
};

// What is about to happen, before it does.
class SummaryPage : public Page
{
public:
    SummaryPage();

    // With a plan, the partitions chosen by hand; without, the disk erased.
    void setSummary(const installer::Options& options, const DiskInfo& disk, const TimeZone& zone,
                    const installer::PartitionPlan* plan);
    // Installing erases data on the disk.
    bool erases() const { return m_erases; }

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    Card* m_card;
    SummaryItem* m_disk;
    SummaryItem* m_account;
    SummaryItem* m_zone;
    Callout* m_warning;
    bool m_erases = true;
};

class ProgressPage : public Page
{
public:
    ProgressPage();

    void setProgress(const std::string& stage, double fraction);
    void takeStateFrom(const ProgressPage& other);

    double contentHeight() const override;

protected:
    void layout() override;
    void layoutContent() override;

private:
    cutefish::ui::ProgressBar* m_bar;
    cutefish::ui::Label* m_stageText;
    // Where the window's buttons are on the other pages.
    cutefish::ui::Label* m_hint;
    std::string m_stage;
    double m_fraction = 0;
};

class ResultPage : public Page
{
public:
    ResultPage();

    void setSucceeded();
    void setFailed(const std::string& error);

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    void setBadge(const char* glyph, cutefish::ui::Color color);

    Card* m_card;
    cutefish::ui::Label* m_error;
};
