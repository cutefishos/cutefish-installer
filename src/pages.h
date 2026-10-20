// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "backend.h"
#include "components.h"
#include "timezones.h"

#include "common/installer.h"
#include "common/partitioning.h"

#include <functional>
#include <string>
#include <vector>

namespace cutefish::ui {
class BusyIndicator;
class CheckBox;
class ListView;
class ProgressBar;
class SearchField;
class TextField;
}

class WelcomePage : public Page
{
public:
    WelcomePage();
};

// The disks to install on, one chosen.
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
    // The partitions are chosen by hand instead of erasing the disk.
    bool isManual() const;

    std::function<void()> onSelectionChanged;

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    void select(int index);
    double cardHeight() const;

    Card* m_card;
    cutefish::ui::BusyIndicator* m_busy;
    cutefish::ui::Label* m_message;
    cutefish::ui::CheckBox* m_manual;
    std::vector<ChoiceRow*> m_rows;
    std::vector<DiskInfo> m_disks;
    int m_selected = -1;
    bool m_loading = true;
};

// The first user, and the computer's name.
class AccountPage : public Page
{
public:
    AccountPage();

    // Everything filled in and valid.
    bool isComplete() const;
    void fill(installer::Options& options) const;
    void focusFirstField();

    std::function<void()> onChanged;
    // Enter in the last field.
    std::function<void()> onAccepted;

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    struct Field
    {
        cutefish::ui::Label* caption;
        cutefish::ui::TextField* field;
    };

    Field addField(const std::string& caption);
    std::string problem() const;
    void changed();

    Card* m_card;
    Field m_fullName;
    Field m_userName;
    Field m_hostName;
    Field m_password;
    Field m_confirm;
    cutefish::ui::Label* m_message;
    bool m_userNameEdited = false;
    bool m_hostNameEdited = false;
};

class TimeZonePage : public Page
{
public:
    TimeZonePage();

    // Empty while none is chosen.
    const TimeZone* selectedZone() const;
    void scrollToSelection();

    std::function<void()> onSelectionChanged;

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    void filter();

    cutefish::ui::SearchField* m_search;
    Card* m_card;
    cutefish::ui::ListView* m_list;
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
    std::vector<SummaryRow*> m_rows;
    cutefish::ui::Label* m_warning;
    bool m_erases = true;
};

class ProgressPage : public Page
{
public:
    ProgressPage();

    void setProgress(const std::string& stage, double fraction);

    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    cutefish::ui::ProgressBar* m_bar;
    cutefish::ui::Label* m_hint;
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
