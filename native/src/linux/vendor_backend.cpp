// Real vendor BIOS settings on Linux via the kernel's unified
// /sys/class/firmware-attributes ABI (Documentation/ABI/testing/
// sysfs-class-firmware-attributes). Unlike Windows - where Dell, HP, and
// Lenovo each publish their own separate WMI classes - the Linux drivers
// for all three (dell-wmi-sysman, hp-wmi, think-lmi) normalize into this
// SAME sysfs shape, so one backend implementation covers all of them:
// no per-vendor code needed here at all.
//
// NOTE: written against the documented kernel ABI but not compiled or
// run on real Linux hardware in this environment - please verify on an
// actual machine, ideally one of the three OEMs above with the relevant
// kernel module loaded.
#include "langbios/vendor_bios.hpp"
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace langbios {

namespace {

const char* kFwAttrRoot = "/sys/class/firmware-attributes";
const char* kDmiVendorFile = "/sys/class/dmi/id/sys_vendor";

bool Exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}

std::string ReadTrimmed(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

bool WriteFile(const std::string& path, const std::string& value) {
    std::ofstream f(path);
    if (!f) return false;
    f << value;
    return f.good();
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Finds the one driver-provided subdirectory under
// /sys/class/firmware-attributes/ (e.g. "dell-wmi-sysman", "think-lmi",
// "hp-wmi-sysman") - whichever OEM driver is actually bound on this
// machine. Empty string if none is.
std::string FindDriverDir() {
    DIR* dir = opendir(kFwAttrRoot);
    if (!dir) return "";
    std::string found;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        found = name;
        break;
    }
    closedir(dir);
    return found;
}

class FirmwareAttributesBackend : public IVendorBiosBackend {
public:
    explicit FirmwareAttributesBackend(std::string driverDir) : driverDir_(std::move(driverDir)) {}

    std::string Name() const override { return driverDir_.empty() ? "none" : driverDir_; }

    bool Supported() override { return !driverDir_.empty(); }

    Result Enumerate(std::vector<BiosAttribute>& out) override {
        if (driverDir_.empty()) {
            return Result::Failure(
                "No /sys/class/firmware-attributes driver bound on this machine "
                "(no dell-wmi-sysman/think-lmi/hp-wmi-sysman module loaded) - "
                "settings beyond Secure Boot/TPM/boot order can't be read or "
                "changed from software here.");
        }

        std::string attrsDir = AttributesDir();
        DIR* dir = opendir(attrsDir.c_str());
        if (!dir) {
            return Result::Failure("Could not open " + attrsDir);
        }

        out.clear();
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string base = attrsDir + "/" + name + "/";
            if (!Exists(base + "current_value")) continue;

            BiosAttribute attr;
            attr.name = name;
            attr.currentValue = ReadTrimmed(base + "current_value");

            std::string possible = ReadTrimmed(base + "possible_values");
            if (!possible.empty()) {
                std::stringstream ss(possible);
                std::string item;
                while (std::getline(ss, item, ';')) attr.possibleValues.push_back(item);
            }
            out.push_back(std::move(attr));
        }
        closedir(dir);
        return Result::Success(
            "Enumerated " + std::to_string(out.size()) + " real BIOS attributes via " + driverDir_ + ".");
    }

    Result SetAttribute(const std::string& name, const std::string& value) override {
        if (driverDir_.empty()) {
            return Result::Failure("No firmware-attributes driver bound on this machine.");
        }
        std::string path = AttributesDir() + "/" + name + "/current_value";
        if (!Exists(path)) {
            return Result::Failure("No such attribute: " + name);
        }
        if (!WriteFile(path, value)) {
            return Result::Failure(
                "Writing " + path + " failed - real firmware attribute writes "
                "require root, and some attributes require a pending-reboot "
                "commit or an admin password set via the sibling "
                "authentication sysfs file.");
        }
        return Result::Success("Set " + name + " = " + value + " via " + driverDir_ + ".");
    }

private:
    std::string AttributesDir() const {
        return std::string(kFwAttrRoot) + "/" + driverDir_ + "/attributes";
    }

    std::string driverDir_;
};

} // namespace

Vendor DetectVendor() {
    std::string m = ToLower(ReadTrimmed(kDmiVendorFile));
    if (m.find("dell") != std::string::npos) return Vendor::Dell;
    if (m.find("hewlett") != std::string::npos || m.find("hp") != std::string::npos) return Vendor::Hp;
    if (m.find("lenovo") != std::string::npos) return Vendor::Lenovo;
    return Vendor::Unknown;
}

const char* VendorName(Vendor v) {
    switch (v) {
        case Vendor::Dell: return "Dell";
        case Vendor::Hp: return "HP";
        case Vendor::Lenovo: return "Lenovo";
        default: return "Unknown";
    }
}

std::unique_ptr<IVendorBiosBackend> CreateVendorBackend(Vendor /*v*/) {
    // Deliberately ignores the detected Vendor: on Linux, whichever OEM
    // driver is bound already normalizes into the same sysfs shape, so
    // there's no need to branch by vendor the way Windows does.
    return std::make_unique<FirmwareAttributesBackend>(FindDriverDir());
}

} // namespace langbios
