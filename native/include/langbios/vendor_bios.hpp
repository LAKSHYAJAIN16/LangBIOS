#pragma once
// Abstraction over each OEM's real BIOS-management WMI interface. Only
// Dell, HP, and Lenovo publish one; on any other machine (including this
// project's own Surface dev box) CreateVendorBackend returns a backend
// whose Supported() is false, and callers must report that honestly
// rather than pretending the setting was changed.
#include "langbios/result.hpp"
#include <memory>
#include <string>
#include <vector>

namespace langbios {

struct BiosAttribute {
    std::wstring name;
    std::wstring currentValue;
    std::vector<std::wstring> possibleValues;
};

enum class Vendor { Dell, Hp, Lenovo, Unknown };

class IVendorBiosBackend {
public:
    virtual ~IVendorBiosBackend() = default;
    virtual std::wstring Name() const = 0;
    // False if this vendor's management namespace isn't present on this
    // machine (wrong OEM, or the OEM's management stack isn't installed).
    virtual bool Supported() = 0;
    virtual Result Enumerate(std::vector<BiosAttribute>& out) = 0;
    virtual Result SetAttribute(const std::wstring& name, const std::wstring& value) = 0;
};

Vendor DetectVendor();
const wchar_t* VendorName(Vendor v);
std::unique_ptr<IVendorBiosBackend> CreateVendorBackend(Vendor v);

} // namespace langbios
