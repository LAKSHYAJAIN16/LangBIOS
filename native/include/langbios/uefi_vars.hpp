#pragma once
// Real, standard UEFI runtime variable access - via the Win32 firmware
// APIs on Windows, or directly through the efivarfs mount (typically
// /sys/firmware/efi/efivars) on Linux. Same standardized UEFI variables
// either way: boot order is genuinely readable AND writable on any UEFI
// machine, vendor-independent. Secure Boot state is readable, but the
// firmware enforces (by design, per the UEFI spec's authenticated
// variable mechanism) that it can't be flipped by an OS-level write.
#include "langbios/result.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace langbios {

struct BootEntry {
    uint16_t id;            // e.g. 0x0003 for "Boot0003"
    std::string description; // human-readable name shown in firmware boot menu
};

// Must succeed before any function below will work.
// Windows: enables SE_SYSTEM_ENVIRONMENT_NAME, which requires the process
//   to already be running elevated (Administrator) - non-admin tokens
//   don't hold it at all.
// Linux: checks CAP_SYS_ADMIN (or effectively that efivarfs is writable),
//   which in practice means running as root.
Result EnableFirmwareVariablePrivilege();

Result ListBootEntries(std::vector<BootEntry>& out);
Result GetBootOrder();                                   // human-readable summary
Result SetBootOrderByDescriptions(const std::vector<std::string>& order);
Result SetBootOrderByIds(const std::vector<uint16_t>& ids);

Result GetSecureBootState();

} // namespace langbios
