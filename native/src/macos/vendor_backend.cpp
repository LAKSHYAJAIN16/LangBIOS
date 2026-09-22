// Apple publishes no vendor BIOS-management interface at all - there
// was never a "BIOS setup menu" concept to build one for, unlike
// Dell/HP/Lenovo on Windows or their Linux kernel drivers. Always
// reports honestly unsupported rather than faking a vendor match.
#include "langbios/vendor_bios.hpp"

namespace langbios {

namespace {

class NullBiosBackend : public IVendorBiosBackend {
public:
    std::string Name() const override { return "none"; }
    bool Supported() override { return false; }
    Result Enumerate(std::vector<BiosAttribute>&) override {
        return Result::Failure(
            "Apple doesn't publish a vendor BIOS/firmware management interface - "
            "there's no OS-level API for settings beyond boot disk selection "
            "(Intel Macs only) and the read-only checks this tool already does.");
    }
    Result SetAttribute(const std::string&, const std::string&) override {
        return Result::Failure(
            "Apple doesn't publish a vendor BIOS/firmware management interface. "
            "This setting can't be changed from software on macOS.");
    }
};

} // namespace

Vendor DetectVendor() {
    // Always Apple hardware if this binary is even running - no vendor
    // enum value fits, so Unknown routes to the NullBiosBackend below.
    return Vendor::Unknown;
}

const char* VendorName(Vendor) {
    return "Apple";
}

std::unique_ptr<IVendorBiosBackend> CreateVendorBackend(Vendor) {
    return std::make_unique<NullBiosBackend>();
}

} // namespace langbios
