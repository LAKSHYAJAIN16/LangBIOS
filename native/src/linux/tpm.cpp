// Real TPM presence/state on Linux via sysfs (/sys/class/tpm/tpm0/...) -
// the kernel's tpm_tis/tpm_crb driver exposes this directly, no WMI-style
// query layer needed the way Windows requires. Read-only, same as
// Windows: enabling/disabling the TPM is a firmware setup action.
//
// NOTE: written against the documented kernel ABI but not compiled or
// run on real Linux hardware in this environment - please verify on an
// actual machine.
#include "langbios/tpm.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace langbios {

namespace {

bool Exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}

std::string ReadTrimmed(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "(unknown)";
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line.empty() ? "(unknown)" : line;
}

} // namespace

Result GetTpmState() {
    const char* base = "/sys/class/tpm/tpm0";
    if (!Exists(base)) {
        return Result::Failure("No TPM device at /sys/class/tpm/tpm0 - either absent, or its "
                                "kernel driver (tpm_tis/tpm_crb) isn't bound on this machine.");
    }

    // A bound /sys/class/tpm/tpmN device node implies the TPM is already
    // enabled and active at the firmware level - unlike Windows' Win32_Tpm,
    // there isn't a separate sysfs "is it enabled" boolean to query,
    // because a firmware-disabled TPM simply wouldn't enumerate a device
    // here at all.
    std::string version = ReadTrimmed(std::string(base) + "/tpm_version_major");
    if (version == "(unknown)") {
        // Older sysfs layout / TPM 1.2 stack doesn't always expose this file.
        version = Exists(std::string(base) + "/caps") ? "1" : "(unknown)";
    }

    return Result::Success(
        "tpm = present, spec_version=" + version +
        " (read-only: TPM enable/disable is a firmware setup action, not "
        "an OS-writable setting on general hardware)",
        "present");
}

} // namespace langbios
