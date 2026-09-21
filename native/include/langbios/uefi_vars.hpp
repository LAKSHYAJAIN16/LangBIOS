#pragma once
// Real, standard UEFI runtime variable access via the Win32 firmware
// APIs. Boot order is genuinely readable AND writable here on any UEFI
// machine, vendor-independent. Secure Boot state is readable, but the
// firmware enforces (by design, per the UEFI spec's authenticated
// variable mechanism) that it can't be flipped by an OS-level write.
#include "langbios/result.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace langbios {

struct BootEntry {
    uint16_t id;             // e.g. 0x0003 for "Boot0003"
    std::wstring description; // human-readable name shown in firmware boot menu
};

// Must succeed before any function below will work. Requires the process
// to be running elevated (Administrator) - non-admin tokens don't hold
// SE_SYSTEM_ENVIRONMENT_NAME at all, so this can't be worked around
// short of actually elevating the process.
Result EnableFirmwareVariablePrivilege();

Result ListBootEntries(std::vector<BootEntry>& out);
Result GetBootOrder();                                   // human-readable summary
Result SetBootOrderByDescriptions(const std::vector<std::wstring>& order);
Result SetBootOrderByIds(const std::vector<uint16_t>& ids);

Result GetSecureBootState();

} // namespace langbios
