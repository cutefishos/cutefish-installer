// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "disks.h"
#include "plan.h"

#include <gtest/gtest.h>

TEST(Plan, newPartition)
{
    EXPECT_EQ(plan::newPartition(2048, 1048576, "U", "EFI"), "start=2048, size=1048576, type=U, name=\"EFI\"\n");
    EXPECT_EQ(plan::newPartition(4096, 100, "L", ""), "start=4096, size=100, type=L\n");
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
    const std::string text = plan::fstab({{"S", "swap", "swap"},
                                          {"H", "/home", "btrfs"},
                                          {"E", "/boot/efi", "vfat"},
                                          {"R", "/", "ext4"},
                                          {"V", "/var", "ext4"}});
    EXPECT_EQ(text, "# <file system> <mount point> <type> <options> <dump> <pass>\n"
                    "UUID=R / ext4 defaults,noatime 0 1\n"
                    "UUID=H /home btrfs defaults,noatime 0 0\n"
                    "UUID=V /var ext4 defaults,noatime 0 2\n"
                    "UUID=E /boot/efi vfat umask=0077 0 1\n"
                    "UUID=S none swap sw 0 0\n");
}

TEST(Plan, mountOrder)
{
    EXPECT_EQ(plan::mountOrder({"/boot/efi", "swap", "/", "/boot", "/home"}),
              (std::vector<size_t> {2, 3, 4, 0, 1}));
}

TEST(Plan, groups)
{
    const std::string groups = "root:x:0:\nsudo:x:27:\naudio:x:29:\nnetdev:x:100:\n";
    EXPECT_EQ(plan::existingGroups(groups, plan::userGroups()), (std::vector<std::string> {"sudo", "audio", "netdev"}));
}

TEST(Plan, accountsServiceUser)
{
    EXPECT_EQ(plan::accountsServiceUser("/var/lib/AccountsService/icons/alice"),
              "[User]\nIcon=/var/lib/AccountsService/icons/alice\nSystemAccount=false\n");
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
