// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "disks.h"

#include "base/file.h"

#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

static void write(const fs::path& path, const std::string& contents)
{
    fs::create_directories(path.parent_path());
    ASSERT_TRUE(cutefish::base::file::writeAtomically(path, contents));
}

TEST(Disks, list)
{
    cutefish::base::file::TemporaryDirectory sys("/tmp", "installer-test");
    ASSERT_TRUE(sys.isValid());
    const fs::path block = fs::path(sys.path()) / "block";
    write(block / "nvme0n1/size", "1000215216\n");
    write(block / "nvme0n1/ro", "0\n");
    write(block / "nvme0n1/removable", "0\n");
    write(block / "nvme0n1/device/model", "YMTC PC005 512GB   \n");
    write(block / "sda/size", "60000000\n");
    write(block / "sda/removable", "1\n");
    write(block / "sda/device/vendor", "SanDisk \n");
    write(block / "sda/device/model", "Ultra\n");
    write(block / "sdb/size", "60000000\n");
    write(block / "loop0/size", "100\n");
    write(block / "zram0/size", "100\n");
    write(block / "sr0/size", "100\n");
    write(block / "sdc/size", "100\n");
    write(block / "sdc/ro", "1\n");
    write(block / "sdd/size", "0\n");

    const std::vector<Disk> disks = listDisks(sys.path(), "sdb");
    ASSERT_EQ(disks.size(), 2u);
    EXPECT_EQ(disks[0].path, "/dev/nvme0n1");
    EXPECT_EQ(disks[0].model, "YMTC PC005 512GB");
    EXPECT_EQ(disks[0].size, 1000215216ull * 512);
    EXPECT_FALSE(disks[0].removable);
    EXPECT_EQ(disks[1].path, "/dev/sda");
    EXPECT_EQ(disks[1].model, "SanDisk Ultra");
    EXPECT_TRUE(disks[1].removable);
}
