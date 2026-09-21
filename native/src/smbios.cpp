#include "langbios/smbios.hpp"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace langbios {

namespace {

#pragma pack(push, 1)
struct RawSmbiosData {
    BYTE Used20CallingMethod;
    BYTE SMBIOSMajorVersion;
    BYTE SMBIOSMinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
};
#pragma pack(pop)

std::wstring Widen(const char* s) {
    if (!s) return L"";
    int len = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, w.data(), len);
    return w;
}

// index is 1-based per the SMBIOS spec; 0 means "no string associated".
std::wstring StringByIndex(const char* stringAreaStart, int index) {
    if (index <= 0) return L"";
    const char* p = stringAreaStart;
    for (int i = 1; i < index; ++i) {
        p += std::strlen(p) + 1;
    }
    return Widen(p);
}

} // namespace

SmbiosInfo ReadSmbiosInfo() {
    const DWORD signature = 'RSMB'; // GetSystemFirmwareTable provider signature
    DWORD needed = GetSystemFirmwareTable(signature, 0, nullptr, 0);
    if (needed == 0) {
        throw std::runtime_error("GetSystemFirmwareTable(RSMB) returned no data");
    }
    std::vector<BYTE> buffer(needed);
    DWORD written = GetSystemFirmwareTable(signature, 0, buffer.data(), needed);
    if (written == 0 || written > buffer.size()) {
        throw std::runtime_error("GetSystemFirmwareTable(RSMB) failed to fill buffer");
    }

    auto* raw = reinterpret_cast<RawSmbiosData*>(buffer.data());
    const BYTE* table = raw->SMBIOSTableData;
    DWORD tableLen = raw->Length;

    SmbiosInfo info;
    DWORD pos = 0;
    while (pos + 4 <= tableLen) {
        BYTE type = table[pos];
        BYTE len = table[pos + 1];
        if (type == 127) break; // end-of-table marker
        if (len < 4) break; // malformed, bail out safely

        const BYTE* structStart = table + pos;
        const char* stringArea = reinterpret_cast<const char*>(structStart + len);

        if (type == 0 && len >= 0x09) {
            info.biosVendor = StringByIndex(stringArea, structStart[0x04]);
            info.biosVersion = StringByIndex(stringArea, structStart[0x05]);
            info.biosReleaseDate = StringByIndex(stringArea, structStart[0x08]);
        } else if (type == 1 && len >= 0x06) {
            info.systemManufacturer = StringByIndex(stringArea, structStart[0x04]);
            info.systemProductName = StringByIndex(stringArea, structStart[0x05]);
        }

        // Skip past the string-table area: scan for the terminating double-NUL.
        const BYTE* p = reinterpret_cast<const BYTE*>(stringArea);
        const BYTE* tableEnd = table + tableLen;
        if (p[0] == 0 && p[1] == 0) {
            p += 2;
        } else {
            while (p + 1 < tableEnd && !(p[0] == 0 && p[1] == 0)) ++p;
            p += 2;
        }
        pos = static_cast<DWORD>(p - table);
    }

    return info;
}

} // namespace langbios
