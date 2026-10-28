// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "installjob.h"

#include "disks.h"
#include "layout.h"
#include "plan.h"

#include "base/file.h"
#include "base/strings.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace file = cutefish::base::file;
namespace fs = std::filesystem;
namespace strings = cutefish::base::strings;

// How much of the whole each stage has done when it ends.
static constexpr double kPrepared = 0.04;
static constexpr double kCopied = 0.70;
static constexpr double kConfigured = 0.82;
static constexpr double kBootable = 0.92;

// What every command runs with, the install's own and not the service's.
static const char* const kEnvironment[] = {
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "LC_ALL=C.UTF-8",
        "HOME=/root",
        "DEBIAN_FRONTEND=noninteractive",
        nullptr,
};

// The lines kept for the error message of a command that failed.
static constexpr size_t kErrorLines = 4;

static std::string quoted(const std::vector<std::string>& command)
{
    std::string text;
    for (const std::string& argument : command) {
        if (!text.empty())
            text += ' ';
        const bool plain = !argument.empty()
                && argument.find_first_of(" \t\n'\"\\$`*?&;|<>()") == std::string::npos;
        text += plain ? argument : '\'' + strings::replaceAll(argument, "'", "'\\''") + '\'';
    }
    return text;
}

// GECOS fields are separated by commas and records by colons.
static std::string gecos(const std::string& fullName)
{
    std::string text;
    for (const char c : fullName) {
        if (c != ':' && c != ',' && c != '\n' && c != '\r')
            text += c;
    }
    return text;
}

InstallJob::InstallJob(Paths paths, installer::Options options, installer::PartitionPlan plan)
    : m_paths(std::move(paths))
    , m_options(std::move(options))
    , m_plan(std::move(plan))
{
}

InstallJob::~InstallJob()
{
    if (m_log)
        std::fclose(m_log);
}

std::string InstallJob::run()
{
    m_log = std::fopen(m_paths.log.c_str(), "ae");
    if (m_log)
        ::fchmod(fileno(m_log), 0600);
    const std::time_t now = std::time(nullptr);
    log("Installing on " + m_options.disk + " at " + std::string(std::ctime(&now)));

    m_uefi = fs::exists("/sys/firmware/efi");
    const bool installed = install();
    unmountAll();
    if (installed) {
        log("Installed.\n");
        progress(installer::kStageFinish, 1);
        return {};
    }
    log("Failed: " + m_error + '\n');
    return m_error;
}

bool InstallJob::install()
{
    progress(installer::kStagePrepare, 0);
    if (!fs::exists(m_paths.squashfs))
        return fail("The live system image " + m_paths.squashfs + " is missing.");
    if (!releaseDisk() || !partition())
        return false;

    progress(installer::kStageCopy, kPrepared);
    const auto copied = [this](std::string_view line) {
        if (const std::optional<double> fraction = plan::unsquashfsProgress(line))
            progress(installer::kStageCopy, kPrepared + (kCopied - kPrepared) * *fraction);
    };
    if (!exec({"unsquashfs", "-f", "-percentage", "-d", m_paths.target, m_paths.squashfs}, {}, copied))
        return false;

    progress(installer::kStageConfigure, kCopied);
    if (!mountChrootFileSystems() || !configure() || !createUser() || !removeLivePackages())
        return false;

    progress(installer::kStageBootloader, kConfigured);
    if (!installBootloader())
        return false;

    progress(installer::kStageFinish, kBootable);
    if (!chroot({"update-initramfs", "-u", "-k", "all"}))
        return false;
    copyLog();
    return exec({"sync"});
}

// Nothing on the disk may stay in use: what the live session mounted from it
// or swaps to.
bool InstallJob::releaseDisk()
{
    const auto onDisk = [this](const std::string& device) { return isOnDisk(device, m_options.disk); };

    if (const std::optional<std::string> swaps = file::read("/proc/swaps")) {
        for (const std::string& line : strings::split(*swaps, '\n')) {
            const std::vector<std::string> fields = strings::splitWhitespace(line);
            if (!fields.empty() && onDisk(fields[0]) && !exec({"swapoff", fields[0]}))
                return false;
        }
    }

    std::vector<std::string> mountPoints;
    if (const std::optional<std::string> mounts = file::read("/proc/self/mounts")) {
        for (const std::string& line : strings::split(*mounts, '\n')) {
            const std::vector<std::string> fields = strings::splitWhitespace(line);
            if (fields.size() > 1 && onDisk(fields[0]))
                mountPoints.push_back(strings::replaceAll(fields[1], "\\040", " "));
        }
    }
    // Inner mounts first.
    std::sort(mountPoints.begin(), mountPoints.end(),
              [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    for (const std::string& mountPoint : mountPoints) {
        if (!exec({"umount", mountPoint}))
            return false;
    }
    return true;
}

bool InstallJob::partition()
{
    const std::string& disk = m_options.disk;
    const installer::PartitionPlan& plan = m_plan;
    const uint64_t sector = plan.layout().sectorSize;
    if (plan.hasNewTable()) {
        if (!exec({"wipefs", "--all", "--force", disk})
            || !exec({"sfdisk", "--wipe", "always", disk}, "label: gpt\n"))
            return false;
    }
    if (!plan.deleted().empty()) {
        std::vector<std::string> command {"sfdisk", "--delete", disk};
        for (const int number : plan.deleted())
            command.push_back(std::to_string(number));
        if (!exec(command))
            return false;
    }
    for (const installer::PlannedPartition& partition : plan.partitions()) {
        if (!partition.isNew())
            continue;
        const std::string name = plan.table() != "gpt"           ? std::string()
                : partition.mountPoint == "/boot/efi"            ? "EFI"
                : partition.mountPoint == "/"                    ? "Cutefish"
                : partition.mountPoint == installer::kMountBiosBoot ? "BIOS"
                                                                 : std::string();
        const std::string line = plan::newPartition(partition.start / sector, partition.size / sector,
                                                    installer::partitionType(partition.mountPoint), name);
        if (!exec({"sfdisk", "--append", "--wipe-partitions", "always", disk}, line))
            return false;
    }
    exec({"udevadm", "settle", "--timeout=10"});

    // New partitions are numbered by sfdisk; their starts find them again.
    std::string json;
    if (!exec({"sfdisk", "--json", disk}, {}, {}, &json))
        return false;
    const std::optional<installer::DiskLayout> written = parseSfdisk(json);
    if (!written)
        return fail("The new partition table cannot be read.");
    for (const installer::PlannedPartition& partition : plan.partitions()) {
        if (partition.mountPoint.empty() || partition.mountPoint == installer::kMountBiosBoot)
            continue;
        std::string device = partition.path;
        for (const installer::ExistingPartition& existing : written->partitions) {
            if (existing.start == partition.start)
                device = existing.path;
        }
        if (device.empty())
            return fail("A new partition is missing from the partition table.");
        m_targets.push_back({device, partition.mountPoint, partition.fsType, partition.format});
    }

    for (const Target& target : m_targets) {
        // udev may still be making the nodes of a disk it was not watching.
        for (int attempt = 0; attempt < 50 && !fs::exists(target.device); ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!fs::exists(target.device))
            return fail("The partition " + target.device + " did not appear.");
        if (!target.format)
            continue;
        const std::string label = target.mountPoint == "/" ? "Cutefish" : target.mountPoint == "/boot/efi" ? "EFI" : "";
        std::vector<std::string> command = mkfsCommand(target.fsType, label);
        command.push_back(target.device);
        if (!exec(command))
            return false;
        if (target.mountPoint == "/boot/efi")
            m_freshEsp = true;
    }

    std::error_code error;
    fs::create_directories(m_paths.target, error);
    std::vector<std::string> mountPoints;
    for (const Target& target : m_targets)
        mountPoints.push_back(target.mountPoint);
    for (const size_t index : plan::mountOrder(mountPoints)) {
        const Target& target = m_targets[index];
        if (target.mountPoint == installer::kMountSwap)
            continue;
        const std::string mountPoint = target.mountPoint == "/" ? m_paths.target : inTarget(target.mountPoint);
        fs::create_directories(mountPoint, error);
        if (!mount({target.device}, mountPoint))
            return false;
    }
    return true;
}

bool InstallJob::mountChrootFileSystems()
{
    std::error_code error;
    fs::create_directories(inTarget("/run/udev"), error);
    if (!mount({"-t", "proc", "proc"}, inTarget("/proc")) || !mount({"-t", "sysfs", "sysfs"}, inTarget("/sys"))
        || !mount({"--bind", "/dev"}, inTarget("/dev")) || !mount({"--bind", "/dev/pts"}, inTarget("/dev/pts"))
        || !mount({"-t", "tmpfs", "tmpfs"}, inTarget("/run")))
        return false;
    fs::create_directories(inTarget("/run/udev"), error);
    if (!mount({"--bind", "/run/udev"}, inTarget("/run/udev")))
        return false;
    // Without it the boot loader's packages cannot add a boot entry; left out
    // when no entry is wanted, so that nothing can.
    if (m_uefi && m_paths.nvram && fs::exists("/sys/firmware/efi/efivars")
        && !mount({"-t", "efivarfs", "efivarfs"}, inTarget("/sys/firmware/efi/efivars")))
        return false;
    return true;
}

bool InstallJob::configure()
{
    std::vector<plan::Mount> mounts;
    for (const Target& target : m_targets) {
        const std::string uuid = uuidOf(target.device);
        if (uuid.empty())
            return fail("The file system on " + target.device + " has no UUID.");
        mounts.push_back({uuid, target.mountPoint, target.fsType});
    }
    if (!writeFile(inTarget("/etc/fstab"), plan::fstab(mounts))
        || !writeFile(inTarget("/etc/hostname"), m_options.hostName + '\n')
        || !writeFile(inTarget("/etc/hosts"), plan::hosts(m_options.hostName)))
        return false;

    std::error_code error;
    fs::remove(inTarget("/etc/localtime"), error);
    fs::create_symlink("/usr/share/zoneinfo/" + m_options.timeZone, inTarget("/etc/localtime"), error);
    if (error)
        return fail("Cannot set the time zone: " + error.message());

    // The language and keyboard the live system was started with.
    for (const char* path : {"/etc/default/locale", "/etc/locale.gen", "/etc/default/keyboard"}) {
        if (fs::exists(path))
            fs::copy_file(path, inTarget(path), fs::copy_options::overwrite_existing, error);
    }
    if (fs::exists(inTarget("/usr/sbin/locale-gen")) && !chroot({"locale-gen"}))
        return false;

    // A machine of its own, not the live image's.
    fs::remove(inTarget("/etc/machine-id"), error);
    fs::remove(inTarget("/var/lib/dbus/machine-id"), error);
    if (!exec({"systemd-machine-id-setup", "--root=" + m_paths.target}))
        return false;
    if (fs::exists(inTarget("/var/lib/dbus")))
        fs::create_symlink("/etc/machine-id", inTarget("/var/lib/dbus/machine-id"), error);

    // Networks joined while trying the system, Wi-Fi passwords included.
    const std::string connections = "/etc/NetworkManager/system-connections";
    if (fs::is_directory(connections) && fs::is_directory(inTarget(connections))) {
        for (const auto& entry : fs::directory_iterator(connections, error)) {
            const std::string destination = inTarget(connections) + '/' + entry.path().filename().string();
            if (entry.is_regular_file() && fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, error))
                ::chmod(destination.c_str(), 0600);
        }
    }
    return true;
}

bool InstallJob::createUser()
{
    const std::optional<std::string> groupFile = file::read(inTarget("/etc/group"));
    const std::vector<std::string> groups = plan::existingGroups(groupFile.value_or(""), plan::userGroups());
    std::vector<std::string> command {"useradd", "--create-home", "--shell", "/bin/bash", "--comment",
                                      gecos(m_options.fullName)};
    if (!groups.empty()) {
        command.push_back("--groups");
        command.push_back(strings::join(groups, ","));
    }
    command.push_back(m_options.userName);
    if (!chroot(command))
        return false;
    return chroot({"chpasswd"}, m_options.userName + ':' + m_options.password + '\n');
}

bool InstallJob::removeLivePackages()
{
    std::vector<std::string> query {"dpkg-query", "-W", "-f", "${db:Status-Abbrev} ${Package}\\n"};
    query.insert(query.end(), plan::livePackages().begin(), plan::livePackages().end());
    std::string output;
    // dpkg-query fails for the names it does not know, which most are.
    chroot(query, {}, &output);
    const std::vector<std::string> installed = plan::installedPackages(output);
    if (!installed.empty()) {
        std::vector<std::string> purge {"apt-get", "--yes", "purge"};
        purge.insert(purge.end(), installed.begin(), installed.end());
        if (!chroot(purge))
            return false;
    }
    std::error_code error;
    fs::remove_all(inTarget("/usr/lib/live"), error);
    return true;
}

bool InstallJob::installBootloader()
{
    // The live image carries the boot loader's programs; the packages that
    // keep it up to date come from the medium's pool where it has one.
    const std::vector<std::string> missing = missingPackages(plan::bootloaderPackages(m_uefi));
    if (!missing.empty() && fs::is_directory(m_paths.medium + "/dists")) {
        if (!installPackagesFromMedium(missing))
            return false;
    } else if (!missing.empty()) {
        log("The medium has no package pool; " + strings::join(missing, " ") + " stay uninstalled.\n");
    }

    std::vector<std::string> command;
    if (m_uefi) {
        command = {"grub-install", "--target=" + plan::efiTarget(), "--efi-directory=/boot/efi", "--bootloader-id=debian",
                   "--recheck"};
        // The removable media path boots firmware that forgets its entries;
        // on a shared EFI system partition it belongs to the other system.
        if (m_freshEsp)
            command.push_back("--force-extra-removable");
        if (!m_paths.nvram)
            command.push_back("--no-nvram");
    } else {
        command = {"grub-install", "--target=i386-pc", "--recheck", m_options.disk};
    }
    if (!chroot(command))
        return false;
    // The boot menu lists the other systems on this computer.
    std::error_code error;
    fs::create_directories(inTarget("/etc/default/grub.d"), error);
    if (!writeFile(inTarget("/etc/default/grub.d/cutefish-installer.cfg"), "GRUB_DISABLE_OS_PROBER=false\n"))
        return false;
    return chroot({"update-grub"});
}

// The medium carries the packages the live system lacks, as a repository of
// its own; nothing is downloaded.
bool InstallJob::installPackagesFromMedium(const std::vector<std::string>& packages)
{
    const std::string mountPoint = inTarget("/run/live/medium");
    std::error_code error;
    fs::create_directories(mountPoint, error);
    if (!mount({"--bind", m_paths.medium}, mountPoint))
        return false;

    const std::string codename = plan::codename(file::read(inTarget("/etc/os-release")).value_or(""));
    const std::string list = "/etc/apt/sources.list.d/cutefish-installer-medium.list";
    if (codename.empty() || !writeFile(inTarget(list), "deb [trusted=yes] file:/run/live/medium " + codename + " main\n"))
        return fail("Cannot add the live medium's packages.");
    const std::vector<std::string> onlyMedium {"-o", "Dir::Etc::SourceList=" + list, "-o", "Dir::Etc::SourceParts=-",
                                               "-o", "APT::Get::List-Cleanup=0"};

    std::vector<std::string> update {"apt-get"};
    update.insert(update.end(), onlyMedium.begin(), onlyMedium.end());
    update.push_back("update");
    std::vector<std::string> install {"apt-get"};
    install.insert(install.end(), onlyMedium.begin(), onlyMedium.end());
    install.insert(install.end(), {"--yes", "--no-install-recommends", "install"});
    install.insert(install.end(), packages.begin(), packages.end());
    const bool installed = chroot(update) && chroot(install);

    fs::remove(inTarget(list), error);
    for (const auto& entry : fs::directory_iterator(inTarget("/var/lib/apt/lists"), error)) {
        if (entry.path().filename().string().starts_with("_run_live_medium_"))
            fs::remove(entry.path(), error);
    }
    return installed;
}

void InstallJob::copyLog()
{
    std::fflush(m_log);
    std::error_code error;
    fs::create_directories(inTarget("/var/log/installer"), error);
    const std::string destination = inTarget("/var/log/installer/cutefish-installer.log");
    if (fs::copy_file(m_paths.log, destination, fs::copy_options::overwrite_existing, error))
        ::chmod(destination.c_str(), 0600);
}

void InstallJob::unmountAll()
{
    for (auto it = m_mounts.rbegin(); it != m_mounts.rend(); ++it) {
        if (!exec({"umount", *it}))
            exec({"umount", "--lazy", *it});
    }
    m_mounts.clear();
}

bool InstallJob::exec(const std::vector<std::string>& command, const std::string& input,
                      const std::function<void(std::string_view line)>& line, std::string* output)
{
    log("$ " + quoted(command) + '\n');

    int in[2] = {-1, -1};
    int out[2] = {-1, -1};
    if (::pipe2(in, O_CLOEXEC) != 0 || ::pipe2(out, O_CLOEXEC) != 0)
        return fail(std::string("Cannot start ") + command.front() + ": " + std::strerror(errno));

    std::vector<char*> argv;
    for (const std::string& argument : command)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid == 0) {
        ::dup2(in[0], STDIN_FILENO);
        ::dup2(out[1], STDOUT_FILENO);
        ::dup2(out[1], STDERR_FILENO);
        ::close_range(3, ~0U, 0);
        ::execvpe(argv[0], argv.data(), const_cast<char* const*>(kEnvironment));
        ::_exit(127);
    }
    ::close(in[0]);
    ::close(out[1]);
    if (pid < 0) {
        ::close(in[1]);
        ::close(out[0]);
        return fail(std::string("Cannot start ") + command.front() + ": " + std::strerror(errno));
    }

    // Small enough for the pipe: a password or a partition table.
    if (!input.empty())
        (void)file::writeWithoutSigpipe(in[1], input.data(), input.size());
    ::close(in[1]);

    std::string pending;
    std::vector<std::string> tail;
    const auto takeLine = [&](std::string text) {
        if (text.empty())
            return;
        log(text + '\n');
        if (output)
            *output += text + '\n';
        if (line)
            line(text);
        tail.push_back(std::move(text));
        if (tail.size() > kErrorLines)
            tail.erase(tail.begin());
    };
    char buffer[4096];
    for (;;) {
        const ssize_t read = ::read(out[0], buffer, sizeof buffer);
        if (read < 0 && errno == EINTR)
            continue;
        if (read <= 0)
            break;
        for (ssize_t i = 0; i < read; ++i) {
            // unsquashfs and mkfs redraw their progress with carriage returns.
            if (buffer[i] == '\n' || buffer[i] == '\r')
                takeLine(std::exchange(pending, {}));
            else
                pending += buffer[i];
        }
    }
    takeLine(std::move(pending));
    ::close(out[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return true;

    std::string message = command.front() + " failed";
    if (WIFEXITED(status))
        message += " with exit code " + std::to_string(WEXITSTATUS(status));
    if (!tail.empty())
        message += ":\n" + strings::join(tail, "\n");
    return fail(message);
}

bool InstallJob::chroot(std::vector<std::string> command, const std::string& input, std::string* output)
{
    command.insert(command.begin(), {"chroot", m_paths.target});
    return exec(command, input, {}, output);
}

bool InstallJob::mount(std::vector<std::string> arguments, const std::string& mountPoint)
{
    arguments.insert(arguments.begin(), "mount");
    arguments.push_back(mountPoint);
    if (!exec(arguments))
        return false;
    m_mounts.push_back(mountPoint);
    return true;
}

std::vector<std::string> InstallJob::missingPackages(const std::vector<std::string>& packages)
{
    std::vector<std::string> query {"dpkg-query", "-W", "-f", "${db:Status-Abbrev} ${Package}\\n"};
    query.insert(query.end(), packages.begin(), packages.end());
    std::string output;
    chroot(query, {}, &output);
    const std::vector<std::string> installed = plan::installedPackages(output);
    std::vector<std::string> missing;
    for (const std::string& package : packages) {
        if (std::find(installed.begin(), installed.end(), package) == installed.end())
            missing.push_back(package);
    }
    m_error.clear();
    return missing;
}

std::string InstallJob::uuidOf(const std::string& device)
{
    std::string output;
    if (!exec({"blkid", "--match-tag", "UUID", "--output", "value", device}, {}, {}, &output))
        return {};
    return std::string(strings::trimmed(output));
}

bool InstallJob::writeFile(const std::string& path, const std::string& contents, unsigned mode)
{
    if (!file::writeAtomically(path, contents))
        return fail("Cannot write " + path.substr(m_paths.target.size()) + '.');
    ::chmod(path.c_str(), mode);
    return true;
}

void InstallJob::progress(const char* stage, double fraction)
{
    if (onProgress)
        onProgress(stage, fraction);
}

bool InstallJob::fail(std::string message)
{
    m_error = std::move(message);
    return false;
}

void InstallJob::log(std::string_view text)
{
    if (!m_log)
        return;
    std::fwrite(text.data(), 1, text.size(), m_log);
    std::fflush(m_log);
}
