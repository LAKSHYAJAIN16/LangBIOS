#pragma once
// Dispatches a parsed Command to the real backend that owns that
// setting: standard UEFI variables for secure_boot/boot_order, WMI for
// tpm, and the detected vendor's BIOS backend for everything else.
#include "langbios/result.hpp"
#include "langbios/rule_parser.hpp"
#include <string>

namespace langbios {

Result Execute(const Command& cmd);

// Top-level entry point: text in, Result out. Tries the rule parser
// first; if it can't confidently match, falls back to the bundled
// local LLM (see llm_fallback.hpp) when one is present on disk.
Result Interpret(const std::string& text);

} // namespace langbios
