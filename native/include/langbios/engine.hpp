#pragma once
// Dispatches a parsed Command to the real backend that owns that
// setting: standard UEFI variables for secure_boot/boot_order, WMI for
// tpm, and the detected vendor's BIOS backend for everything else.
#include "langbios/result.hpp"
#include "langbios/rule_parser.hpp"

namespace langbios {

Result Execute(const Command& cmd);

} // namespace langbios
