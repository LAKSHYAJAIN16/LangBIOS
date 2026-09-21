#pragma once
// Abstraction over each OEM's real BIOS-management interface: WMI classes
// on Windows (Dell/HP/Lenovo each publish their own), or the Linux
// kernel's unified /sys/class/firmware-attributes sysfs ABI on Linux
// (which already normalizes Dell/Lenovo/HP into one interface - no
// per-vendor backend needed there). On a machine with neither,
// CreateVendorBackend returns a backend whose Supported() is false, and
// callers must report that honestly rather than pretending the setting
// was changed.
#include "langbios/result.hpp"
#include <memory>
#include <string>
#include <vector>

namespace langbios {

struct BiosAttribute {
    std::string name;
    std::string currentValue;
    std::vector<std::string> possibleValues;
};

enum class Vendor { Dell, Hp, Lenovo, Unknown };

class IVendorBiosBackend {
public:
    virtual ~IVendorBiosBackend() = default;
    virtual std::string Name() const = 0;
    // False if this vendor's management interface isn't present on this
    // machine (wrong OEM, or the OEM's management stack isn't installed).
    virtual bool Supported() = 0;
    virtual Result Enumerate(std::vector<BiosAttribute>& out) = 0;
    virtual Result SetAttribute(const std::string& name, const std::string& value) = 0;
};

Vendor DetectVendor();
const char* VendorName(Vendor v);
std::unique_ptr<IVendorBiosBackend> CreateVendorBackend(Vendor v);

} // namespace langbios
