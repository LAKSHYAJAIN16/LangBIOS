#include "langbios/vendor_bios.hpp"
#include "smbios.hpp"
#include "win_strings.hpp"
#include <algorithm>
#include <cctype>

namespace langbios {

// Defined in vendor_dell.cpp / vendor_hp.cpp / vendor_lenovo.cpp.
std::unique_ptr<IVendorBiosBackend> CreateDellBackend();
std::unique_ptr<IVendorBiosBackend> CreateHpBackend();
std::unique_ptr<IVendorBiosBackend> CreateLenovoBackend();

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

class NullBiosBackend : public IVendorBiosBackend {
public:
    explicit NullBiosBackend(std::string manufacturer) : manufacturer_(std::move(manufacturer)) {}
    std::string Name() const override { return "none"; }
    bool Supported() override { return false; }
    Result Enumerate(std::vector<BiosAttribute>&) override {
        return Result::Failure(
            "No vendor BIOS management interface available for manufacturer '" +
            manufacturer_ + "' - only Dell, HP, and Lenovo publish one, and it "
            "has to be installed by the OEM. This machine has none.");
    }
    Result SetAttribute(const std::string&, const std::string&) override {
        return Result::Failure(
            "No vendor BIOS management interface available for manufacturer '" +
            manufacturer_ + "'. This setting can only be changed from the "
            "physical UEFI setup menu on this machine.");
    }

private:
    std::string manufacturer_;
};

} // namespace

Vendor DetectVendor() {
    try {
        SmbiosInfo info = ReadSmbiosInfo();
        std::string m = ToLower(WideToUtf8(info.systemManufacturer));
        if (m.find("dell") != std::string::npos) return Vendor::Dell;
        if (m.find("hewlett") != std::string::npos || m.find("hp") != std::string::npos) return Vendor::Hp;
        if (m.find("lenovo") != std::string::npos) return Vendor::Lenovo;
        return Vendor::Unknown;
    } catch (...) {
        return Vendor::Unknown;
    }
}

const char* VendorName(Vendor v) {
    switch (v) {
        case Vendor::Dell: return "Dell";
        case Vendor::Hp: return "HP";
        case Vendor::Lenovo: return "Lenovo";
        default: return "Unknown";
    }
}

std::unique_ptr<IVendorBiosBackend> CreateVendorBackend(Vendor v) {
    switch (v) {
        case Vendor::Dell: return CreateDellBackend();
        case Vendor::Hp: return CreateHpBackend();
        case Vendor::Lenovo: return CreateLenovoBackend();
        default: {
            std::string manufacturer = "Unknown";
            try {
                manufacturer = WideToUtf8(ReadSmbiosInfo().systemManufacturer);
            } catch (...) {
            }
            return std::make_unique<NullBiosBackend>(manufacturer);
        }
    }
}

} // namespace langbios
