// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "disks.h"
#include "plan.h"

#include <gtest/gtest.h>

TEST(Plan, partitionTable)
{
    EXPECT_EQ(plan::partitionTable(true),
              "label: gpt\nsize=512MiB, type=uefi, name=\"EFI\"\ntype=linux, name=\"Cutefish\"\n");
    EXPECT_EQ(plan::partitionTable(false), "label: gpt\nsize=1MiB, type=21686148-6449-6E6F-744E-656564454649, "
                                           "name=\"BIOS\"\ntype=linux, name=\"Cutefish\"\n");
}

TEST(Plan, partitionPaths)
{
    EXPECT_EQ(partitionPath("/dev/sda", 2), "/dev/sda2");
    EXPECT_EQ(partitionPath("/dev/nvme0n1", 1), "/dev/nvme0n1p1");
    EXPECT_EQ(partitionPath("/dev/mmcblk0", 2), "/dev/mmcblk0p2");
    EXPECT_EQ(partitionPath("/dev/loop7", 2), "/dev/loop7p2");
}

TEST(Plan, onDisk)
{
    EXPECT_TRUE(isOnDisk("/dev/sda", "/dev/sda"));
    EXPECT_TRUE(isOnDisk("/dev/sda1", "/dev/sda"));
    EXPECT_TRUE(isOnDisk("/dev/sda12", "/dev/sda"));
    EXPECT_FALSE(isOnDisk("/dev/sdaa", "/dev/sda"));
    EXPECT_FALSE(isOnDisk("/dev/sdb1", "/dev/sda"));
    EXPECT_TRUE(isOnDisk("/dev/nvme0n1p2", "/dev/nvme0n1"));
    EXPECT_FALSE(isOnDisk("/dev/nvme0n10", "/dev/nvme0n1"));
    EXPECT_FALSE(isOnDisk("/dev/loop10", "/dev/loop1"));
    EXPECT_TRUE(isOnDisk("/dev/loop1p1", "/dev/loop1"));
    EXPECT_FALSE(isOnDisk("/dev/loop1p", "/dev/loop1"));
}

TEST(Plan, fstab)
{
    const std::string uefi = plan::fstab("1111", "AB-CD");
    EXPECT_NE(uefi.find("UUID=1111 / ext4 defaults,noatime 0 1\n"), std::string::npos);
    EXPECT_NE(uefi.find("UUID=AB-CD /boot/efi vfat umask=0077 0 1\n"), std::string::npos);
    EXPECT_EQ(plan::fstab("1111", "").find("/boot/efi"), std::string::npos);
}

TEST(Plan, groups)
{
    const std::string groups = "root:x:0:\nsudo:x:27:\naudio:x:29:\nnetdev:x:100:\n";
    EXPECT_EQ(plan::existingGroups(groups, plan::userGroups()), (std::vector<std::string> {"sudo", "audio", "netdev"}));
}

TEST(Plan, installedPackages)
{
    const std::string output = "ii  live-boot\nun  live-tools\nii  cutefish-installer:amd64\nrc  calamares\n";
    EXPECT_EQ(plan::installedPackages(output), (std::vector<std::string> {"live-boot", "cutefish-installer"}));
}

TEST(Plan, unsquashfsProgress)
{
    EXPECT_DOUBLE_EQ(*plan::unsquashfsProgress("42"), 0.42);
    EXPECT_DOUBLE_EQ(*plan::unsquashfsProgress("100"), 1);
    EXPECT_FALSE(plan::unsquashfsProgress("Parallel unsquashfs: Using 16 processors"));
    EXPECT_FALSE(plan::unsquashfsProgress("140"));
}

TEST(Plan, codename)
{
    EXPECT_EQ(plan::codename("PRETTY_NAME=\"Debian GNU/Linux 13 (trixie)\"\nVERSION_CODENAME=trixie\n"), "trixie");
    EXPECT_EQ(plan::codename("ID=debian\n"), "");
}
