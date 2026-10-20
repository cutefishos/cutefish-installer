// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "partitioning.h"

#include "base/i18n.h"

#include <algorithm>
#include <set>

namespace dbus = cutefish::base::dbus;
using cutefish::i18n::format;
using cutefish::i18n::tr;

static constexpr char kBiosBootType[] = "21686148-6449-6e6f-744e-656564454649";

static uint64_t alignUp(uint64_t value)
{
    return (value + installer::kMiB - 1) / installer::kMiB * installer::kMiB;
}

static uint64_t alignDown(uint64_t value)
{
    return value / installer::kMiB * installer::kMiB;
}

static bool isDirectory(const std::string& mountPoint)
{
    return !mountPoint.empty() && mountPoint.front() == '/';
}

// Kept as they are, these hold data worth keeping; anything else is part of
// the system and starts empty.
static bool canKeep(const std::string& mountPoint)
{
    return mountPoint == "/home" || mountPoint == "/boot/efi" || mountPoint == installer::kMountSwap;
}

static bool isLinuxFileSystem(const std::string& fsType)
{
    return fsType == "ext4" || fsType == "btrfs" || fsType == "xfs";
}

template<typename T>
static T value(const dbus::VariantMap& map, const char* key, T fallback = T())
{
    const auto found = map.find(key);
    return found == map.end() ? fallback : found->second.value<T>(fallback);
}

namespace installer {

const std::vector<std::string>& mountPoints()
{
    static const std::vector<std::string> points {"/",    "/boot/efi", "/home", "/boot",    "/var",
                                                  "/opt", "/srv",      "/tmp",  kMountSwap, kMountBiosBoot};
    return points;
}

const std::vector<std::string>& fileSystems()
{
    static const std::vector<std::string> types {"ext4", "btrfs", "xfs", "vfat", "swap"};
    return types;
}

dbus::VariantMap toMap(const DiskLayout& layout)
{
    std::vector<dbus::VariantMap> partitions;
    for (const ExistingPartition& partition : layout.partitions) {
        partitions.push_back({
                {"number", dbus::Variant(int32_t(partition.number))},
                {"path", dbus::Variant(partition.path)},
                {"start", dbus::Variant(partition.start)},
                {"size", dbus::Variant(partition.size)},
                {"type", dbus::Variant(partition.type)},
                {"fsType", dbus::Variant(partition.fsType)},
                {"label", dbus::Variant(partition.label)},
        });
    }
    return {
            {"disk", dbus::Variant(layout.disk)},
            {"size", dbus::Variant(layout.size)},
            {"sectorSize", dbus::Variant(layout.sectorSize)},
            {"table", dbus::Variant(layout.table)},
            {"partitions", dbus::Variant(partitions)},
    };
}

std::optional<DiskLayout> layoutFromMap(const dbus::VariantMap& map)
{
    DiskLayout layout;
    layout.disk = value<std::string>(map, "disk");
    layout.size = value<uint64_t>(map, "size");
    layout.sectorSize = value<uint64_t>(map, "sectorSize", 512);
    layout.table = value<std::string>(map, "table");
    if (layout.disk.empty() || layout.size == 0 || layout.sectorSize == 0)
        return std::nullopt;
    for (const dbus::VariantMap& entry : value<std::vector<dbus::VariantMap>>(map, "partitions")) {
        ExistingPartition partition;
        partition.number = value<int32_t>(entry, "number");
        partition.path = value<std::string>(entry, "path");
        partition.start = value<uint64_t>(entry, "start");
        partition.size = value<uint64_t>(entry, "size");
        partition.type = value<std::string>(entry, "type");
        partition.fsType = value<std::string>(entry, "fsType");
        partition.label = value<std::string>(entry, "label");
        layout.partitions.push_back(std::move(partition));
    }
    return layout;
}

PartitionPlan::PartitionPlan(DiskLayout layout)
    : m_layout(std::move(layout))
{
    for (const ExistingPartition& existing : m_layout.partitions) {
        PlannedPartition partition;
        partition.number = existing.number;
        partition.start = existing.start;
        partition.size = existing.size;
        partition.fsType = existing.fsType;
        partition.path = existing.path;
        partition.label = existing.label;
        partition.existingFsType = existing.fsType;
        partition.type = existing.type;
        m_partitions.push_back(std::move(partition));
    }
    sort();
}

PartitionPlan PartitionPlan::eraseDisk(DiskLayout layout, bool uefi)
{
    PartitionPlan plan(std::move(layout));
    plan.clearTable();
    const uint64_t start = plan.firstUsable();
    if (uefi)
        plan.add(start, kEspSize, "/boot/efi", "vfat");
    else
        plan.add(start, kMiB, kMountBiosBoot, {});
    const Region rest = plan.freeRegions().back();
    plan.add(rest.start, rest.size, "/", "ext4");
    return plan;
}

std::string PartitionPlan::table() const
{
    return m_newTable ? "gpt" : m_layout.table;
}

uint64_t PartitionPlan::firstUsable() const
{
    return kMiB;
}

uint64_t PartitionPlan::lastUsable() const
{
    // The backup GPT header and table take the disk's last sectors.
    return alignDown(table() == "gpt" ? m_layout.size - kMiB : m_layout.size);
}

std::vector<PartitionPlan::Region> PartitionPlan::freeRegions() const
{
    std::vector<Region> regions;
    if (table().empty())
        return regions;
    uint64_t cursor = firstUsable();
    const auto addGap = [&regions](uint64_t from, uint64_t to) {
        const uint64_t start = alignUp(from);
        const uint64_t end = alignDown(to);
        if (end > start && end - start >= kMiB)
            regions.push_back({start, end - start});
    };
    for (const PlannedPartition& partition : m_partitions) {
        addGap(cursor, std::min(partition.start, lastUsable()));
        cursor = std::max(cursor, partition.start + partition.size);
    }
    addGap(cursor, lastUsable());
    return regions;
}

void PartitionPlan::clearTable()
{
    m_newTable = true;
    m_deleted.clear();
    m_partitions.clear();
}

bool PartitionPlan::remove(size_t index)
{
    if (index >= m_partitions.size())
        return false;
    if (!m_partitions[index].isNew())
        m_deleted.push_back(m_partitions[index].number);
    m_partitions.erase(m_partitions.begin() + long(index));
    return true;
}

bool PartitionPlan::add(uint64_t start, uint64_t size, std::string mountPoint, std::string fsType)
{
    for (const Region& region : freeRegions()) {
        const uint64_t end = region.start + region.size;
        if (start < region.start || start >= end)
            continue;
        const uint64_t aligned = alignUp(start);
        if (aligned >= end)
            return false;
        const uint64_t length = std::min(alignDown(size), end - aligned);
        if (length < kMiB)
            return false;
        PlannedPartition partition;
        partition.start = aligned;
        partition.size = length;
        partition.mountPoint = std::move(mountPoint);
        partition.fsType = partition.mountPoint == kMountBiosBoot ? std::string() : std::move(fsType);
        partition.format = !partition.fsType.empty();
        m_partitions.push_back(std::move(partition));
        sort();
        return true;
    }
    return false;
}

bool PartitionPlan::assign(size_t index, std::string mountPoint, bool format, std::string fsType)
{
    if (index >= m_partitions.size())
        return false;
    PlannedPartition& partition = m_partitions[index];
    partition.mountPoint = std::move(mountPoint);
    if (partition.mountPoint == kMountBiosBoot)
        fsType.clear();
    if (partition.isNew()) {
        partition.fsType = std::move(fsType);
        partition.format = !partition.fsType.empty();
    } else if (format && !partition.mountPoint.empty()) {
        partition.fsType = std::move(fsType);
        partition.format = !partition.fsType.empty();
    } else {
        partition.fsType = partition.existingFsType;
        partition.format = false;
    }
    return true;
}

std::string PartitionPlan::problem(bool uefi) const
{
    if (table().empty())
        return tr("The disk has no partition table. Create one first.");
    if (table() == "dos" && m_partitions.size() > 4)
        return tr("A disk with an MBR partition table holds at most four partitions.");

    std::set<std::string> used;
    const PlannedPartition* root = nullptr;
    const PlannedPartition* esp = nullptr;
    const PlannedPartition* biosBoot = nullptr;
    for (const PlannedPartition& partition : m_partitions) {
        const std::string& mountPoint = partition.mountPoint;
        if (mountPoint.empty())
            continue;
        if (std::find(mountPoints().begin(), mountPoints().end(), mountPoint) == mountPoints().end())
            return format(tr("“{mount}” is not a mount point the system can use."), {{"mount", mountPoint}});
        if (!used.insert(mountPoint).second)
            return format(tr("Two partitions are given “{mount}”."), {{"mount", mountPoint}});
        if (mountPoint == "/")
            root = &partition;
        else if (mountPoint == "/boot/efi")
            esp = &partition;
        else if (mountPoint == kMountBiosBoot)
            biosBoot = &partition;

        if (mountPoint == kMountBiosBoot) {
            if (table() != "gpt")
                return tr("Only a disk with a GPT partition table needs a BIOS boot partition.");
            if (!partition.isNew() && partition.type != kBiosBootType)
                return tr("Only a new partition, or one made for it, can be the BIOS boot partition.");
            continue;
        }
        if (!partition.format && !canKeep(mountPoint))
            return format(tr("The partition for “{mount}” must be formatted."), {{"mount", mountPoint}});
        const std::string& fsType = partition.fsType;
        const bool fits = mountPoint == "/boot/efi" ? fsType == "vfat"
                : mountPoint == kMountSwap         ? fsType == "swap"
                                                   : isLinuxFileSystem(fsType);
        if (!fits && partition.format)
            return format(tr("“{type}” cannot be used for “{mount}”."), {{"type", fsType}, {"mount", mountPoint}});
        if (!fits)
            return format(tr("The partition for “{mount}” holds “{type}”; format it to use it there."),
                          {{"mount", mountPoint}, {"type", fsType.empty() ? tr("nothing") : fsType}});
    }

    if (!root)
        return tr("Choose a partition for the system: give one the mount point “/”.");
    if (root->size < kMinimumRootSize)
        return format(tr("The partition for “/” needs at least {size} GB."),
                      {{"size", std::to_string(kMinimumRootSize / 1'000'000'000)}});
    if (uefi && !esp)
        return tr("Choose an EFI system partition: give one the mount point “/boot/efi”.");
    if (esp && esp->size < kMinimumEspSize)
        return tr("The EFI system partition needs at least 32 MB.");
    if (!uefi && table() == "gpt" && !biosBoot)
        return tr("Add a 1 MB partition used as BIOS boot, for the boot loader.");
    return {};
}

std::vector<std::string> PartitionPlan::erasedPartitions() const
{
    std::vector<std::string> erased;
    const auto pathOf = [this](int number) {
        for (const ExistingPartition& partition : m_layout.partitions) {
            if (partition.number == number)
                return partition.path;
        }
        return std::string();
    };
    if (m_newTable) {
        for (const ExistingPartition& partition : m_layout.partitions)
            erased.push_back(partition.path);
    }
    for (const int number : m_deleted)
        erased.push_back(pathOf(number));
    for (const PlannedPartition& partition : m_partitions) {
        if (!partition.isNew() && partition.format)
            erased.push_back(partition.path);
    }
    return erased;
}

dbus::VariantMap PartitionPlan::toMap() const
{
    std::vector<int32_t> deleted(m_deleted.begin(), m_deleted.end());
    std::vector<dbus::VariantMap> partitions;
    for (const PlannedPartition& partition : m_partitions) {
        partitions.push_back({
                {"number", dbus::Variant(int32_t(partition.number))},
                {"start", dbus::Variant(partition.start)},
                {"size", dbus::Variant(partition.size)},
                {"mountPoint", dbus::Variant(partition.mountPoint)},
                {"format", dbus::Variant(partition.format)},
                {"fsType", dbus::Variant(partition.fsType)},
        });
    }
    return {
            {"newTable", dbus::Variant(m_newTable)},
            {"deleted", dbus::Variant(deleted)},
            {"partitions", dbus::Variant(partitions)},
    };
}

std::optional<PartitionPlan> PartitionPlan::fromMap(const dbus::VariantMap& map, DiskLayout layout)
{
    PartitionPlan plan(std::move(layout));
    if (value<bool>(map, "newTable"))
        plan.clearTable();
    for (const int32_t number : value<std::vector<int32_t>>(map, "deleted")) {
        const auto found = std::find_if(plan.m_partitions.begin(), plan.m_partitions.end(),
                                        [number](const PlannedPartition& partition) { return partition.number == number; });
        if (number <= 0 || found == plan.m_partitions.end())
            return std::nullopt;
        plan.remove(size_t(found - plan.m_partitions.begin()));
    }

    const std::vector<dbus::VariantMap> entries = value<std::vector<dbus::VariantMap>>(map, "partitions");
    // The new partitions first, so that the indexes found for existing ones stay.
    for (const dbus::VariantMap& entry : entries) {
        if (value<int32_t>(entry, "number") != 0)
            continue;
        const uint64_t start = value<uint64_t>(entry, "start");
        const uint64_t size = value<uint64_t>(entry, "size");
        if (!plan.add(start, size, value<std::string>(entry, "mountPoint"), value<std::string>(entry, "fsType")))
            return std::nullopt;
        const auto added = std::find_if(plan.m_partitions.begin(), plan.m_partitions.end(),
                                        [start](const PlannedPartition& partition) { return partition.start == start; });
        if (added == plan.m_partitions.end() || added->size != size || !added->isNew())
            return std::nullopt;
    }
    size_t existing = 0;
    for (const dbus::VariantMap& entry : entries) {
        const int32_t number = value<int32_t>(entry, "number");
        if (number == 0)
            continue;
        const auto found = std::find_if(plan.m_partitions.begin(), plan.m_partitions.end(),
                                        [number](const PlannedPartition& partition) { return partition.number == number; });
        if (found == plan.m_partitions.end() || found->start != value<uint64_t>(entry, "start")
            || found->size != value<uint64_t>(entry, "size"))
            return std::nullopt;
        plan.assign(size_t(found - plan.m_partitions.begin()), value<std::string>(entry, "mountPoint"),
                    value<bool>(entry, "format"), value<std::string>(entry, "fsType"));
        ++existing;
    }
    size_t kept = 0;
    for (const PlannedPartition& partition : plan.m_partitions)
        kept += partition.isNew() ? 0 : 1;
    if (existing != kept)
        return std::nullopt;
    return plan;
}

void PartitionPlan::sort()
{
    std::sort(m_partitions.begin(), m_partitions.end(),
              [](const PlannedPartition& a, const PlannedPartition& b) { return a.start < b.start; });
}

std::string partitionType(const std::string& mountPoint)
{
    if (mountPoint == kMountBiosBoot)
        return kBiosBootType;
    if (mountPoint == "/boot/efi")
        return "U";
    if (mountPoint == kMountSwap)
        return "S";
    return "L";
}

} // namespace installer
