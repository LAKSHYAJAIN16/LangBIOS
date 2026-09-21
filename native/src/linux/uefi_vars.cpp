// Real, standard UEFI variable access on Linux via efivarfs - same
// underlying UEFI runtime services as the Windows implementation, just
// reached through a filesystem instead of a Win32 API. Boot order is
// genuinely readable AND writable here on any UEFI Linux machine (as
// root). Secure Boot state is readable, but - same as Windows, per the
// UEFI spec's authenticated variable mechanism - the firmware itself
// refuses to let software flip it.
//
// NOTE: written against the documented efivarfs/kernel ABI but not
// compiled or run on real Linux hardware in the environment this was
// developed in (Windows-only) - please test on an actual Linux UEFI
// machine before relying on the write path.
#include "langbios/uefi_vars.hpp"
#include "efivarfs.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unistd.h>

namespace langbios {

namespace {

const char* kGlobalGuid = "8be4df61-93ca-11d2-aa0d-00e098032b8c";

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string BootName(uint16_t id) {
    char buf[16];
    snprintf(buf, sizeof(buf), "Boot%04X", id);
    return buf;
}

Result WrapEfiError(const std::string& what, const std::exception& e) {
    std::string msg = what + ": " + e.what();
    if (msg.find("Permission denied") != std::string::npos ||
        msg.find("Operation not permitted") != std::string::npos) {
        msg += " - real firmware variable access requires running as root "
               "(CAP_SYS_ADMIN), by design.";
    }
    return Result::Failure(msg);
}

} // namespace

Result EnableFirmwareVariablePrivilege() {
    if (!EfiVarFsAvailable()) {
        return Result::Failure(
            "efivarfs (/sys/firmware/efi/efivars) is not mounted - this machine "
            "is either not booted in UEFI mode, or the kernel wasn't built with "
            "efivarfs support.");
    }
    if (geteuid() != 0) {
        return Result::Failure(
            "Real firmware variable access requires running as root "
            "(CAP_SYS_ADMIN) - re-run with sudo.");
    }
    return Result::Success("efivarfs available and running as root.");
}

Result ListBootEntries(std::vector<BootEntry>& out) {
    Result priv = EnableFirmwareVariablePrivilege();
    if (!priv.ok) return priv;

    std::vector<uint8_t> orderData;
    try {
        orderData = ReadEfiVariable("BootOrder", kGlobalGuid);
    } catch (const std::exception& e) {
        return WrapEfiError("Reading BootOrder failed", e);
    }

    size_t count = orderData.size() / sizeof(uint16_t);
    out.clear();
    for (size_t i = 0; i < count; ++i) {
        uint16_t id = static_cast<uint16_t>(orderData[i * 2]) |
                      (static_cast<uint16_t>(orderData[i * 2 + 1]) << 8);

        std::string desc = "(unreadable)";
        try {
            std::vector<uint8_t> opt = ReadEfiVariable(BootName(id), kGlobalGuid);
            // EFI_LOAD_OPTION: UINT32 Attributes; UINT16 FilePathListLength; CHAR16 Description[];
            if (opt.size() >= 6) {
                const uint8_t* p = opt.data() + 6;
                size_t maxBytes = opt.size() - 6;
                std::u16string wide;
                for (size_t b = 0; b + 1 < maxBytes; b += 2) {
                    uint16_t ch = static_cast<uint16_t>(p[b]) | (static_cast<uint16_t>(p[b + 1]) << 8);
                    if (ch == 0) break;
                    wide.push_back(static_cast<char16_t>(ch));
                }
                if (!wide.empty()) {
                    // Descriptions are firmware-provided boot entry names -
                    // ASCII/Latin-1 in the overwhelming majority of real
                    // systems, so a direct narrow cast is a pragmatic,
                    // good-enough conversion here (full UTF-16 handling
                    // would need <codecvt> or ICU for correctness on
                    // genuinely non-Latin entry names).
                    std::string narrow(wide.begin(), wide.end());
                    desc = narrow;
                }
            }
        } catch (...) {
            // leave desc as "(unreadable)"
        }
        out.push_back(BootEntry{id, desc});
    }
    return Result::Success("ok");
}

Result GetBootOrder() {
    std::vector<BootEntry> entries;
    Result r = ListBootEntries(entries);
    if (!r.ok) return r;

    std::stringstream ss;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) ss << " | ";
        char idBuf[8];
        snprintf(idBuf, sizeof(idBuf), "%04X", entries[i].id);
        ss << i << ":" << idBuf << " " << entries[i].description;
    }
    return Result::Success("Real boot order (from firmware): " + ss.str(), ss.str());
}

Result SetBootOrderByIds(const std::vector<uint16_t>& ids) {
    Result priv = EnableFirmwareVariablePrivilege();
    if (!priv.ok) return priv;

    if (ids.empty()) {
        return Result::Failure("Refusing to write an empty BootOrder.");
    }

    std::vector<uint8_t> data(ids.size() * 2);
    for (size_t i = 0; i < ids.size(); ++i) {
        data[i * 2] = static_cast<uint8_t>(ids[i] & 0xFF);
        data[i * 2 + 1] = static_cast<uint8_t>((ids[i] >> 8) & 0xFF);
    }

    // EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
    // EFI_VARIABLE_RUNTIME_ACCESS - same standard attribute set the
    // Windows implementation uses.
    constexpr uint32_t kAttrs = 0x00000001 | 0x00000002 | 0x00000004;
    try {
        WriteEfiVariable("BootOrder", kGlobalGuid, kAttrs, data);
    } catch (const std::exception& e) {
        return WrapEfiError("Writing BootOrder failed", e);
    }
    return Result::Success("Boot order updated in real firmware (" + std::to_string(ids.size()) + " entries).");
}

Result SetBootOrderByDescriptions(const std::vector<std::string>& order) {
    std::vector<BootEntry> entries;
    Result r = ListBootEntries(entries);
    if (!r.ok) return r;

    std::vector<uint16_t> newIds;
    for (const auto& want : order) {
        std::string wantLower = ToLower(want);
        auto it = std::find_if(entries.begin(), entries.end(), [&](const BootEntry& e) {
            return ToLower(e.description).find(wantLower) != std::string::npos;
        });
        if (it == entries.end()) {
            return Result::Failure(
                "No current boot entry matches '" + want + "'. Real boot entry "
                "names on this machine don't match generic categories like "
                "'ssd'/'hdd' - use `list settings` to see the actual entry names "
                "first (e.g. 'ubuntu').");
        }
        newIds.push_back(it->id);
    }

    if (newIds.size() != entries.size()) {
        return Result::Failure(
            "Boot order change must list every current boot entry, in the "
            "desired order - refusing a partial reorder to avoid dropping "
            "an entry from the real BootOrder variable.");
    }

    return SetBootOrderByIds(newIds);
}

Result GetSecureBootState() {
    Result priv = EnableFirmwareVariablePrivilege();
    if (!priv.ok) return priv;

    try {
        std::vector<uint8_t> val = ReadEfiVariable("SecureBoot", kGlobalGuid);
        if (val.empty()) {
            return Result::Failure("SecureBoot variable was empty/unreadable.");
        }
        std::string state = val[0] ? "enabled" : "disabled";
        return Result::Success(
            "secure_boot = " + state + " (read-only from software by firmware design; "
            "toggle it from the physical UEFI setup menu)",
            state);
    } catch (const std::exception& e) {
        return WrapEfiError("Reading SecureBoot failed (some CSM/legacy-mode systems don't expose it)", e);
    }
}

} // namespace langbios
