#include "langbios/vendor_bios.hpp"
#include "langbios/smbios.hpp"
#include <algorithm>
#include <cwctype>

namespace langbios {

// Defined in vendor_dell.cpp / vendor_hp.cpp / vendor_lenovo.cpp.
std::unique_ptr<IVendorBiosBackend> CreateDellBackend();
std::unique_ptr<IVendorBiosBackend> CreateHpBackend();
std::unique_ptr<IVendorBiosBackend> CreateLenovoBackend();

namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return std::towlower(c); });
    return s;
}

class NullBiosBackend : public IVendorBiosBackend {
public:
    explicit NullBiosBackend(std::wstring manufacturer) : manufacturer_(std::move(manufacturer)) {}
    std::wstring Name() const override { return L"none"; }
    bool Supported() override { return false; }
    Result Enumerate(std::vector<BiosAttribute>&) override {
        return Result::Failure(
            L"No vendor BIOS management interface available for manufacturer '" +
            manufacturer_ + L"' - only Dell, HP, and Lenovo publish one, and it "
            L"has to be installed by the OEM. This machine has none.");
    }
    Result SetAttribute(const std::wstring&, const std::wstring&) override {
        return Result::Failure(
            L"No vendor BIOS management interface available for manufacturer '" +
            manufacturer_ + L"'. This setting can only be changed from the "
            L"physical UEFI setup menu on this machine.");
    }

private:
    std::wstring manufacturer_;
};

} // namespace

Vendor DetectVendor() {
    try {
        SmbiosInfo info = ReadSmbiosInfo();
        std::wstring m = ToLower(info.systemManufacturer);
        if (m.find(L"dell") != std::wstring::npos) return Vendor::Dell;
        if (m.find(L"hewlett") != std::wstring::npos || m.find(L"hp") != std::wstring::npos) return Vendor::Hp;
        if (m.find(L"lenovo") != std::wstring::npos) return Vendor::Lenovo;
        return Vendor::Unknown;
    } catch (...) {
        return Vendor::Unknown;
    }
}

const wchar_t* VendorName(Vendor v) {
    switch (v) {
        case Vendor::Dell: return L"Dell";
        case Vendor::Hp: return L"HP";
        case Vendor::Lenovo: return L"Lenovo";
        default: return L"Unknown";
    }
}

std::unique_ptr<IVendorBiosBackend> CreateVendorBackend(Vendor v) {
    switch (v) {
        case Vendor::Dell: return CreateDellBackend();
        case Vendor::Hp: return CreateHpBackend();
        case Vendor::Lenovo: return CreateLenovoBackend();
        default: {
            std::wstring manufacturer = L"Unknown";
            try {
                manufacturer = ReadSmbiosInfo().systemManufacturer;
            } catch (...) {
            }
            return std::make_unique<NullBiosBackend>(manufacturer);
        }
    }
}

} // namespace langbios
