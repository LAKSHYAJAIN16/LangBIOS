// macOS "firmware settings" access. This is deliberately much thinner
// than Windows/Linux, because Apple's platform genuinely doesn't offer
// the same surface - not a gap in this code, a gap in what Apple exposes:
//
// - Secure Boot: only configurable from recoveryOS's Startup Security
//   Utility, a separate boot environment reached by holding the power
//   button - specifically NOT reachable from a running OS process, by
//   Apple's design. No API exists for this project (or anyone) to touch.
// - TPM: does not exist on Mac. Apple uses the Secure Enclave instead, a
//   closed architecture with no public read/write API.
// - Boot order: Macs have a single default startup disk, not an ordered
//   list of entries like PC BootOrder. On Intel Macs this is genuinely
//   settable via the standard `bless` command line tool. On Apple Silicon,
//   boot/security policy is controlled by `bputil`, a fundamentally
//   different, security-policy-oriented tool - misusing it risks actually
//   weakening a Mac's boot security, so this project deliberately does
//   NOT automate it. Intel-only, and read via `bless`/`diskutil`.
//
// NOTE: written against Apple's documented `bless`/`diskutil` command line
// tools but not compiled or run on real Mac hardware in this environment
// (Windows-only development machine). Please verify on an actual Intel
// Mac before trusting the write path.
#include "langbios/uefi_vars.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <sys/utsname.h>
#include <unistd.h>

namespace langbios {

namespace {

std::string RunCommand(const std::string& cmd) {
    std::array<char, 256> buf;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((cmd + " 2>&1").c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) {
        result += buf.data();
    }
    return result;
}

bool IsAppleSilicon() {
    struct utsname u{};
    if (uname(&u) != 0) return false;
    return std::string(u.machine).find("arm64") != std::string::npos;
}

// Pulls "Volume Name: X" out of `diskutil info <device>` output.
std::string ParseVolumeName(const std::string& diskutilInfo) {
    size_t pos = diskutilInfo.find("Volume Name:");
    if (pos == std::string::npos) return "(unknown volume)";
    pos += std::string("Volume Name:").size();
    size_t end = diskutilInfo.find('\n', pos);
    std::string name = diskutilInfo.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    size_t start = name.find_first_not_of(" \t");
    size_t last = name.find_last_not_of(" \t\r");
    if (start == std::string::npos) return "(unknown volume)";
    return name.substr(start, last - start + 1);
}

std::string Trim(std::string s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

} // namespace

Result EnableFirmwareVariablePrivilege() {
    if (geteuid() != 0) {
        return Result::Failure(
            "Real firmware/startup-disk access requires running as root - re-run with sudo.");
    }
    return Result::Success("Running as root.");
}

Result ListBootEntries(std::vector<BootEntry>& out) {
    out.clear();
    if (IsAppleSilicon()) {
        return Result::Failure(
            "Apple Silicon Macs select boot/security policy via `bputil`, a "
            "fundamentally different mechanism from Intel's `bless` - this "
            "project deliberately doesn't automate it, since a mistake there "
            "can weaken the Mac's boot security rather than just pick the "
            "wrong disk.");
    }

    Result priv = EnableFirmwareVariablePrivilege();
    if (!priv.ok) return priv;

    std::string device = Trim(RunCommand("/usr/sbin/bless --getBoot"));
    if (device.empty()) {
        return Result::Failure("`bless --getBoot` returned nothing - could not determine the current startup disk.");
    }

    std::string info = RunCommand("/usr/sbin/diskutil info " + device);
    std::string volumeName = ParseVolumeName(info);

    out.push_back(BootEntry{0, volumeName});
    return Result::Success("ok");
}

Result GetBootOrder() {
    std::vector<BootEntry> entries;
    Result r = ListBootEntries(entries);
    if (!r.ok) return r;

    if (entries.empty()) {
        return Result::Failure("Could not determine the current startup disk.");
    }
    return Result::Success(
        "Real startup disk (from firmware, macOS has one default, not an ordered "
        "list like PC BootOrder): " + entries[0].description,
        entries[0].description);
}

Result SetBootOrderByIds(const std::vector<uint16_t>&) {
    return Result::Failure(
        "macOS has no numeric boot-entry IDs. Use the volume name directly, "
        "e.g. 'set boot order to Macintosh HD'.");
}

Result SetBootOrderByDescriptions(const std::vector<std::string>& order) {
    if (IsAppleSilicon()) {
        return Result::Failure(
            "Apple Silicon Macs select boot/security policy via `bputil`, a "
            "fundamentally different mechanism from Intel's `bless` - this "
            "project deliberately doesn't automate it.");
    }
    if (order.size() != 1) {
        return Result::Failure(
            "macOS only has a single default startup disk, not an ordered list "
            "like PC BootOrder - give exactly one volume name, e.g. "
            "'set boot order to Macintosh HD'.");
    }

    Result priv = EnableFirmwareVariablePrivilege();
    if (!priv.ok) return priv;

    std::string wanted = order[0];
    std::string list = RunCommand("/usr/sbin/diskutil list");
    // Find a line naming the wanted volume, then the disk identifier for it.
    // diskutil list output is columnar; the identifier is the last token.
    size_t pos = list.find(wanted);
    if (pos == std::string::npos) {
        return Result::Failure(
            "No current volume matches '" + wanted + "'. Run `diskutil list` "
            "yourself to see real volume names on this Mac.");
    }
    size_t lineEnd = list.find('\n', pos);
    size_t lineStart = list.rfind('\n', pos);
    lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
    std::string line = list.substr(lineStart, (lineEnd == std::string::npos ? list.size() : lineEnd) - lineStart);
    size_t lastSpace = line.find_last_not_of(" \t\r");
    size_t identStart = line.find_last_of(' ', lastSpace);
    std::string identifier = identStart == std::string::npos
        ? Trim(line)
        : Trim(line.substr(identStart + 1, lastSpace - identStart));

    if (identifier.empty()) {
        return Result::Failure("Could not parse a disk identifier for '" + wanted + "' from `diskutil list`.");
    }

    std::string device = "/dev/" + identifier;
    std::string out = RunCommand("/usr/sbin/bless --device " + device + " --setBoot");
    if (out.find("error") != std::string::npos || out.find("Error") != std::string::npos) {
        return Result::Failure("`bless --device " + device + " --setBoot` failed: " + Trim(out));
    }
    return Result::Success("Startup disk set to '" + wanted + "' (" + device + ") via bless.");
}

Result GetSecureBootState() {
    return Result::Failure(
        "secure_boot is not accessible from a running OS on macOS. It's only "
        "configurable from recoveryOS's Startup Security Utility (hold the "
        "power button on Apple Silicon, or Cmd+R at boot on Intel), by "
        "Apple's design - no third-party software, including this one, can "
        "read or write it from here.");
}

} // namespace langbios
