#pragma once
// Real TPM presence/state via WMI (root\cimv2\Security\MicrosoftTpm ::
// Win32_Tpm). Read-only: enabling/disabling the TPM is a firmware setup
// action, not something Windows exposes a write API for on general
// hardware.
#include "langbios/result.hpp"

namespace langbios {

Result GetTpmState();

} // namespace langbios
