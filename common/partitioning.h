// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace installer {

inline constexpr uint64_t kMiB = uint64_t(1) << 20;
// The least for the system partition, the system and its updates.
inline constexpr uint64_t kMinimumRootSize = 12'000'000'000;
inline constexpr uint64_t kMinimumEspSize = 32 * kMiB;
inline constexpr uint64_t kEspSize = 512 * kMiB;

// What a partition is for, besides a directory: swap, or the space GRUB's
// core takes on a GPT disk booted by a BIOS.
inline constexpr char kMountSwap[] = "swap";
inline constexpr char kMountBiosBoot[] = "bios-boot";

// The mount points a partition can be given, in the order offered.
const std::vector<std::string>& mountPoints();
// The file systems a partition can be formatted with, in the order offered.
const std::vector<std::string>& fileSystems();

struct ExistingPartition
{
    int number = 0;
    std::string path;
    // In bytes.
    uint64_t start = 0;
    uint64_t size = 0;
    // A GPT type GUID or an MBR type such as "83", lower case.
    std::string type;
    std::string fsType;
    std::string label;
};

// A disk as sfdisk and blkid see it.
struct DiskLayout
{
    std::string disk;
    uint64_t size = 0;
    uint64_t sectorSize = 512;
    // "gpt", "dos", or empty without a partition table.
    std::string table;
    std::vector<ExistingPartition> partitions;
};

cutefish::base::dbus::VariantMap toMap(const DiskLayout& layout);
std::optional<DiskLayout> layoutFromMap(const cutefish::base::dbus::VariantMap& map);

// One partition as it will be: one there now that is kept, or a new one.
struct PlannedPartition
{
    // 0 for a new partition.
    int number = 0;
    uint64_t start = 0;
    uint64_t size = 0;
    // Empty when nothing is mounted from it.
    std::string mountPoint;
    // Always for a new partition with a file system.
    bool format = false;
    // What it is formatted with; what it has when kept.
    std::string fsType;
    // The existing partition's, for showing it.
    std::string path;
    std::string label;
    std::string existingFsType;
    std::string type;

    bool isNew() const { return number == 0; }
};

// The changes to make to one disk, kept as the partitions it will have. Edits
// apply to it in memory; nothing touches the disk until it is installed.
class PartitionPlan
{
public:
    struct Region
    {
        uint64_t start = 0;
        uint64_t size = 0;
    };

    PartitionPlan() = default;
    explicit PartitionPlan(DiskLayout layout);
    // A new GPT table with an EFI system partition, or a BIOS boot partition,
    // and the rest for the system.
    static PartitionPlan eraseDisk(DiskLayout layout, bool uefi);

    const DiskLayout& layout() const { return m_layout; }
    bool hasNewTable() const { return m_newTable; }
    // "gpt" or "dos"; empty while the disk has none and none is planned.
    std::string table() const;
    // By start.
    const std::vector<PlannedPartition>& partitions() const { return m_partitions; }
    // Existing partitions to delete.
    const std::vector<int>& deleted() const { return m_deleted; }
    // Unused space of at least a MiB, its edges on whole MiBs.
    std::vector<Region> freeRegions() const;

    // Deletes everything for an empty GPT table.
    void clearTable();
    bool remove(size_t index);
    // A new partition at the start of a free region; the size is rounded down
    // to whole MiBs and cut to the region. False when there is no room there.
    bool add(uint64_t start, uint64_t size, std::string mountPoint, std::string fsType);
    // An existing partition's use; a new one keeps being formatted.
    bool assign(size_t index, std::string mountPoint, bool format, std::string fsType);

    // What keeps the plan from being installed, for the user; empty when
    // nothing does.
    std::string problem(bool uefi) const;
    // The partitions that will be erased, by path or as "new partition".
    std::vector<std::string> erasedPartitions() const;

    cutefish::base::dbus::VariantMap toMap() const;
    // The plan sent as toMap() made again on the disk as it is now; empty
    // when it does not fit that disk.
    static std::optional<PartitionPlan> fromMap(const cutefish::base::dbus::VariantMap& map, DiskLayout layout);

private:
    void sort();
    uint64_t firstUsable() const;
    uint64_t lastUsable() const;

    DiskLayout m_layout;
    bool m_newTable = false;
    std::vector<int> m_deleted;
    std::vector<PlannedPartition> m_partitions;
};

// The partition type sfdisk takes for a new partition used for this, on a
// GPT or an MBR table alike.
std::string partitionType(const std::string& mountPoint);

} // namespace installer
