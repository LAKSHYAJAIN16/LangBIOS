#pragma once
// Raw efivarfs access - Linux's real equivalent of Windows'
// GetFirmwareEnvironmentVariableW/SetFirmwareEnvironmentVariableExW.
// Every UEFI variable shows up as a file at
// /sys/firmware/efi/efivars/<Name>-<GUID>, whose first 4 bytes are the
// variable's attribute flags (little-endian uint32) followed immediately
// by the actual variable data - same underlying UEFI runtime service, a
// filesystem instead of a Win32 API as the OS-mediated interface to it.
#include <cstdint>
#include <string>
#include <vector>

namespace langbios {

// Throws std::runtime_error with a clear, specific message (not present,
// permission denied, etc.) on any failure.
std::vector<uint8_t> ReadEfiVariable(const std::string& name, const std::string& guid);

// Writes attributes (4 bytes, little-endian) + data in one syscall, which
// is what efivarfs requires for the write to be accepted as a single
// atomic variable update. Transparently clears+restores the immutable
// inode flag the kernel sets on these files (the same thing efibootmgr
// does), since a plain write() against an immutable file fails with
// EPERM even for root.
void WriteEfiVariable(const std::string& name, const std::string& guid,
                      uint32_t attributes, const std::vector<uint8_t>& data);

bool EfiVarFsAvailable();

} // namespace langbios
