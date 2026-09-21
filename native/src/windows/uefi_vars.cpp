#include "langbios/uefi_vars.hpp"
#include "win_strings.hpp"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

// These SDK constants are gated behind an NTDDI_VERSION check that isn't
// always satisfied depending on how WINVER/_WIN32_WINNT resolve for a
// given toolchain; the values themselves are stable and documented, so
// define them defensively if the headers didn't.
#ifndef VARIABLE_ATTRIBUTE_NON_VOLATILE
#define VARIABLE_ATTRIBUTE_NON_VOLATILE 0x00000001
#endif
#ifndef VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS
#define VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS 0x00000002
#endif
#ifndef VARIABLE_ATTRIBUTE_RUNTIME_ACCESS
#define VARIABLE_ATTRIBUTE_RUNTIME_ACCESS 0x00000004
#endif

namespace langbios {

namespace {

const wchar_t* kGlobalGuid = L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}";

std::string LastErrorMessage(DWORD err) {
    LPWSTR buf = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::string msg = buf ? WideToUtf8(buf) : "unknown error";
    if (buf) LocalFree(buf);
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
    return msg;
}

Result PrivilegeAwareFailure(const std::string& what) {
    DWORD err = GetLastError();
    if (err == ERROR_PRIVILEGE_NOT_HELD) {
        return Result::Failure(
            what + ": privilege not held. This requires an elevated "
            "(Administrator) process - run this tool from an elevated "
            "terminal (e.g. `sudo` if enabled in Developer Settings, or "
            "'Run as administrator').");
    }
    return Result::Failure(what + ": " + LastErrorMessage(err) + " (code " + std::to_string(err) + ")");
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

Result EnableFirmwareVariablePrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return PrivilegeAwareFailure("OpenProcessToken failed");
    }

    LUID luid;
    if (!LookupPrivilegeValueW(nullptr, SE_SYSTEM_ENVIRONMENT_NAME, &luid)) {
        CloseHandle(token);
        return PrivilegeAwareFailure("LookupPrivilegeValue(SeSystemEnvironmentPrivilege) failed");
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL adjusted = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD err = GetLastError();
    CloseHandle(token);

    if (!adjusted) {
        SetLastError(err);
        return PrivilegeAwareFailure("AdjustTokenPrivileges failed");
    }
    if (err == ERROR_NOT_ALL_ASSIGNED) {
        return Result::Failure(
            "This process's token does not hold SeSystemEnvironmentPrivilege. "
            "Real firmware variable access requires running elevated as "
            "Administrator - non-admin accounts are never granted this "
            "privilege, by design.");
    }
    return Result::Success("Firmware variable privilege enabled.");
}

Result ListBootEntries(std::vector<BootEntry>& out) {
    Result priv = EnableFirmwareVariablePrivilege();
    if (!priv.ok) return priv;

    BYTE orderBuf[2048];
    DWORD orderLen = GetFirmwareEnvironmentVariableW(L"BootOrder", kGlobalGuid, orderBuf, sizeof(orderBuf));
    if (orderLen == 0) {
        return PrivilegeAwareFailure("Reading BootOrder failed");
    }

    size_t count = orderLen / sizeof(uint16_t);
    auto* ids = reinterpret_cast<uint16_t*>(orderBuf);

    out.clear();
    for (size_t i = 0; i < count; ++i) {
        wchar_t name[16];
        swprintf_s(name, L"Boot%04X", ids[i]);

        BYTE optBuf[4096];
        DWORD optLen = GetFirmwareEnvironmentVariableW(name, kGlobalGuid, optBuf, sizeof(optBuf));
        std::string desc = "(unreadable)";
        if (optLen >= 6) {
            // EFI_LOAD_OPTION: UINT32 Attributes; UINT16 FilePathListLength; CHAR16 Description[];
            const wchar_t* descStart = reinterpret_cast<const wchar_t*>(optBuf + 6);
            size_t maxChars = (optLen - 6) / sizeof(wchar_t);
            std::wstring raw(descStart, wcsnlen_s(descStart, maxChars));
            if (!raw.empty()) desc = WideToUtf8(raw);
        }
        out.push_back(BootEntry{ids[i], desc});
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

    DWORD attrs = VARIABLE_ATTRIBUTE_NON_VOLATILE | VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS |
                  VARIABLE_ATTRIBUTE_RUNTIME_ACCESS;
    BOOL wrote = SetFirmwareEnvironmentVariableExW(
        L"BootOrder", kGlobalGuid,
        const_cast<uint16_t*>(ids.data()),
        static_cast<DWORD>(ids.size() * sizeof(uint16_t)),
        attrs);
    if (!wrote) {
        return PrivilegeAwareFailure("Writing BootOrder failed");
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
                "first (e.g. 'Windows Boot Manager').");
        }
        newIds.push_back(it->id);
    }

    // Refuse partial reorders: this must be a permutation of ALL existing
    // entries so we never silently drop a boot option.
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

    BYTE val = 0;
    DWORD len = GetFirmwareEnvironmentVariableW(L"SecureBoot", kGlobalGuid, &val, sizeof(val));
    if (len == 0) {
        return PrivilegeAwareFailure("Reading SecureBoot failed (some firmware/CSM-mode systems don't expose it)");
    }
    std::string state = val ? "enabled" : "disabled";
    return Result::Success(
        "secure_boot = " + state + " (read-only from software by firmware design; "
        "toggle it from the physical UEFI setup menu)",
        state);
}

} // namespace langbios
