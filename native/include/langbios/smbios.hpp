#pragma once
// Reads the real SMBIOS firmware table straight from the kernel
// (GetSystemFirmwareTable) - no WMI needed. This is genuine low-level
// firmware identity info: BIOS vendor/version/date, system manufacturer
// and product name. Used both to show real info and to pick which
// vendor BIOS backend to try.
#include <string>

namespace langbios {

struct SmbiosInfo {
    std::wstring biosVendor;
    std::wstring biosVersion;
    std::wstring biosReleaseDate;
    std::wstring systemManufacturer;
    std::wstring systemProductName;
};

// Throws std::runtime_error if the firmware table can't be read/parsed.
SmbiosInfo ReadSmbiosInfo();

} // namespace langbios
