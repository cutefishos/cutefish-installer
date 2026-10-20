// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "common/partitioning.h"
#include "layout.h"

#include <gtest/gtest.h>

using namespace installer;

static constexpr uint64_t kGB = 1'000'000'000;

// A 100 GB disk with an EFI system partition, Windows and free space after it.
static DiskLayout windowsDisk()
{
    DiskLayout layout;
    layout.disk = "/dev/sda";
    layout.size = 100 * kGB;
    layout.table = "gpt";
    layout.partitions = {
            {1, "/dev/sda1", kMiB, 100 * kMiB, "c12a7328-f81f-11d2-ba4b-00a0c93ec93b", "vfat", "SYSTEM"},
            {2, "/dev/sda2", 101 * kMiB, 40 * kGB, "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7", "ntfs", "Windows"},
    };
    return layout;
}

TEST(Partitioning, eraseDisk)
{
    const PartitionPlan uefi = PartitionPlan::eraseDisk(windowsDisk(), true);
    EXPECT_TRUE(uefi.hasNewTable());
    ASSERT_EQ(uefi.partitions().size(), 2u);
    EXPECT_EQ(uefi.partitions()[0].mountPoint, "/boot/efi");
    EXPECT_EQ(uefi.partitions()[0].start, kMiB);
    EXPECT_EQ(uefi.partitions()[0].size, kEspSize);
    EXPECT_EQ(uefi.partitions()[1].mountPoint, "/");
    EXPECT_EQ(uefi.partitions()[1].start, kMiB + kEspSize);
    // Up to the last whole MiB before the backup GPT.
    EXPECT_EQ(uefi.partitions()[1].start + uefi.partitions()[1].size, (100 * kGB / kMiB - 1) * kMiB);
    EXPECT_EQ(uefi.problem(true), "");
    EXPECT_EQ(uefi.erasedPartitions(), (std::vector<std::string> {"/dev/sda1", "/dev/sda2"}));

    const PartitionPlan bios = PartitionPlan::eraseDisk(windowsDisk(), false);
    ASSERT_EQ(bios.partitions().size(), 2u);
    EXPECT_EQ(bios.partitions()[0].mountPoint, kMountBiosBoot);
    EXPECT_EQ(bios.partitions()[0].size, kMiB);
    EXPECT_TRUE(bios.partitions()[0].fsType.empty());
    EXPECT_EQ(bios.problem(false), "");
}

TEST(Partitioning, besideWindows)
{
    PartitionPlan plan(windowsDisk());
    const std::vector<PartitionPlan::Region> free = plan.freeRegions();
    ASSERT_EQ(free.size(), 1u);
    EXPECT_EQ(free[0].start, 101 * kMiB + 40 * kGB / kMiB * kMiB + kMiB);
    EXPECT_NE(plan.problem(true), "");

    // The free space for the system, the EFI system partition shared.
    ASSERT_TRUE(plan.add(free[0].start, free[0].size, "/", "ext4"));
    EXPECT_EQ(plan.problem(true), "Choose an EFI system partition: give one the mount point “/boot/efi”.");
    ASSERT_TRUE(plan.assign(0, "/boot/efi", false, {}));
    EXPECT_EQ(plan.problem(true), "");
    EXPECT_FALSE(plan.partitions()[0].format);
    EXPECT_EQ(plan.partitions()[0].fsType, "vfat");
    EXPECT_TRUE(plan.erasedPartitions().empty());
    EXPECT_TRUE(plan.freeRegions().empty());

    // Windows given to the system unformatted.
    ASSERT_TRUE(plan.assign(1, "/home", false, {}));
    EXPECT_EQ(plan.problem(true), "The partition for “/home” holds “ntfs”; format it to use it there.");
    ASSERT_TRUE(plan.assign(1, "/home", true, "ext4"));
    EXPECT_EQ(plan.problem(true), "");
    EXPECT_EQ(plan.erasedPartitions(), (std::vector<std::string> {"/dev/sda2"}));
}

TEST(Partitioning, rules)
{
    PartitionPlan plan(windowsDisk());
    ASSERT_TRUE(plan.remove(1));
    EXPECT_EQ(plan.deleted(), (std::vector<int> {2}));
    const PartitionPlan::Region free = plan.freeRegions().front();

    // Too small for the system.
    ASSERT_TRUE(plan.add(free.start, 5 * kGB, "/", "ext4"));
    ASSERT_TRUE(plan.assign(0, "/boot/efi", false, {}));
    EXPECT_EQ(plan.problem(true), "The partition for “/” needs at least 12 GB.");
    ASSERT_TRUE(plan.remove(1));

    ASSERT_TRUE(plan.add(free.start, 20 * kGB, "/", "ext4"));
    EXPECT_EQ(plan.problem(true), "");
    const uint64_t next = plan.freeRegions().front().start;
    ASSERT_TRUE(plan.add(next, 4 * kGB, "/", "xfs"));
    EXPECT_EQ(plan.problem(true), "Two partitions are given “/”.");
    ASSERT_TRUE(plan.assign(2, "swap", true, "ext4"));
    EXPECT_EQ(plan.problem(true), "“ext4” cannot be used for “swap”.");
    ASSERT_TRUE(plan.assign(2, "swap", true, "swap"));
    EXPECT_EQ(plan.problem(true), "");
    // A GPT disk booted by a BIOS needs a place for the boot loader.
    EXPECT_EQ(plan.problem(false), "Add a 1 MB partition used as BIOS boot, for the boot loader.");

    // A kept ESP may stay; a kept system partition may not.
    ASSERT_TRUE(plan.assign(0, "/", false, {}));
    ASSERT_TRUE(plan.assign(1, "/home", true, "ext4"));
    EXPECT_EQ(plan.problem(true), "The partition for “/” must be formatted.");

    // Nothing fits where a partition is.
    EXPECT_FALSE(plan.add(kMiB, kGB, "/var", "ext4"));
}

TEST(Partitioning, noTable)
{
    DiskLayout layout;
    layout.disk = "/dev/vda";
    layout.size = 50 * kGB;
    PartitionPlan plan(layout);
    EXPECT_TRUE(plan.freeRegions().empty());
    EXPECT_EQ(plan.problem(true), "The disk has no partition table. Create one first.");
    plan.clearTable();
    EXPECT_EQ(plan.table(), "gpt");
    EXPECT_EQ(plan.freeRegions().size(), 1u);
}

TEST(Partitioning, roundTrip)
{
    PartitionPlan plan(windowsDisk());
    ASSERT_TRUE(plan.assign(0, "/boot/efi", false, {}));
    const PartitionPlan::Region free = plan.freeRegions().front();
    ASSERT_TRUE(plan.add(free.start, 30 * kGB, "/", "btrfs"));
    ASSERT_TRUE(plan.add(plan.freeRegions().front().start, 8 * kGB, "swap", "swap"));

    const std::optional<PartitionPlan> back = PartitionPlan::fromMap(plan.toMap(), windowsDisk());
    ASSERT_TRUE(back);
    ASSERT_EQ(back->partitions().size(), plan.partitions().size());
    for (size_t i = 0; i < plan.partitions().size(); ++i) {
        EXPECT_EQ(back->partitions()[i].start, plan.partitions()[i].start);
        EXPECT_EQ(back->partitions()[i].size, plan.partitions()[i].size);
        EXPECT_EQ(back->partitions()[i].mountPoint, plan.partitions()[i].mountPoint);
        EXPECT_EQ(back->partitions()[i].format, plan.partitions()[i].format);
        EXPECT_EQ(back->partitions()[i].fsType, plan.partitions()[i].fsType);
    }

    // A disk that changed since is refused.
    DiskLayout changed = windowsDisk();
    changed.partitions[1].size += kGB;
    EXPECT_FALSE(PartitionPlan::fromMap(plan.toMap(), changed));
    DiskLayout fewer = windowsDisk();
    fewer.partitions.pop_back();
    EXPECT_FALSE(PartitionPlan::fromMap(plan.toMap(), fewer));

    const std::optional<DiskLayout> layout = layoutFromMap(toMap(windowsDisk()));
    ASSERT_TRUE(layout);
    EXPECT_EQ(layout->partitions.size(), 2u);
    EXPECT_EQ(layout->partitions[1].label, "Windows");
}

TEST(Layout, parse)
{
    const std::string sfdisk = R"({
   "partitiontable": {
      "label": "gpt", "id": "X", "device": "/dev/nvme0n1", "unit": "sectors",
      "firstlba": 2048, "lastlba": 1000215182, "sectorsize": 512,
      "partitions": [
         {"node": "/dev/nvme0n1p1", "start": 2048, "size": 1048576, "type": "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "uuid": "A"},
         {"node": "/dev/nvme0n1p2", "start": 1050624, "size": 999161856, "type": "0FC63DAF-8483-4772-8E79-3D69D8477DE4", "uuid": "B"}
      ]
   }
})";
    std::optional<DiskLayout> layout = parseSfdisk(sfdisk);
    ASSERT_TRUE(layout);
    EXPECT_EQ(layout->table, "gpt");
    ASSERT_EQ(layout->partitions.size(), 2u);
    EXPECT_EQ(layout->partitions[1].number, 2);
    EXPECT_EQ(layout->partitions[1].start, 1050624ull * 512);
    EXPECT_EQ(layout->partitions[0].type, "c12a7328-f81f-11d2-ba4b-00a0c93ec93b");

    addFileSystems(*layout, R"({"blockdevices": [{"path": "/dev/nvme0n1", "fstype": null, "label": null,
        "children": [{"path": "/dev/nvme0n1p1", "fstype": "vfat", "label": null},
                     {"path": "/dev/nvme0n1p2", "fstype": "ext4", "label": "root"}]}]})");
    EXPECT_EQ(layout->partitions[0].fsType, "vfat");
    EXPECT_EQ(layout->partitions[1].label, "root");

    // Partitions beside the disk rather than under it.
    layout->partitions[1].fsType.clear();
    addFileSystems(*layout, R"({"blockdevices": [{"path": "/dev/nvme0n1", "fstype": null, "label": null},
        {"path": "/dev/nvme0n1p2", "fstype": "btrfs", "label": null}]})");
    EXPECT_EQ(layout->partitions[1].fsType, "btrfs");
    EXPECT_FALSE(parseSfdisk("sfdisk: failed"));
}
