// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "components.h"

#include "common/partitioning.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cutefish::ui {
class BusyIndicator;
class ConfirmDialog;
class DialogButton;
class DialogWindow;
class ListView;
class Window;
}

class PartitionBar;

// The disk's partitions as they will be, edited by hand: a new partition
// table, partitions added in free space or deleted, and what each is used for.
class PartitionPage : public Page
{
public:
    // One row: a planned partition or free space.
    struct Entry
    {
        // Into the plan's partitions; -1 for free space.
        int partition = -1;
        uint64_t start = 0;
        uint64_t size = 0;
    };

    explicit PartitionPage(bool uefi);
    ~PartitionPage() override;

    void setLoading(const std::string& diskName);
    void setLayout(installer::DiskLayout layout, std::vector<std::string> fileSystems);
    void setError(const std::string& error);

    const installer::PartitionPlan& plan() const { return m_plan; }
    // As if edited into this, for tests.
    void setPlanForCapture(installer::PartitionPlan plan);
    bool isComplete() const;

    // The dialogs open over this window.
    void setWindow(cutefish::ui::Window* window) { m_window = window; }

    std::function<void()> onChanged;

    // For the views.
    const std::vector<Entry>& entries() const { return m_entries; }
    int selected() const { return m_selected; }
    void select(int row);

    double contentWidth() const override;
    double contentHeight() const override;

protected:
    void layoutContent() override;

private:
    void changed();
    void updateActions();
    void newTable();
    void addPartition();
    void editPartition();
    void deletePartition();

    bool m_uefi;
    installer::PartitionPlan m_plan;
    std::vector<std::string> m_fileSystems;
    std::vector<Entry> m_entries;
    int m_selected = -1;
    bool m_loading = true;
    cutefish::ui::Window* m_window = nullptr;
    std::unique_ptr<cutefish::ui::DialogWindow> m_dialog;
    std::unique_ptr<cutefish::ui::ConfirmDialog> m_confirm;

    PartitionBar* m_bar;
    Card* m_card;
    cutefish::ui::ListView* m_list;
    cutefish::ui::BusyIndicator* m_busy;
    cutefish::ui::DialogButton* m_newTable;
    cutefish::ui::DialogButton* m_add;
    cutefish::ui::DialogButton* m_edit;
    cutefish::ui::DialogButton* m_delete;
    std::vector<cutefish::ui::Label*> m_headers;
    cutefish::ui::Label* m_message;
};

// What a mount point is shown as: "/ (system)", "Swap".
std::string mountPointName(const std::string& mountPoint);
