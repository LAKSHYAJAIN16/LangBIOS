#pragma once
// Local LLM fallback for the native CLI/library - no Ollama, no API key,
// no network call. Shells out to a bundled llama.cpp binary
// (llamacpp/bin/llama-cli[.exe]) running a small bundled GGUF model
// (models/*.gguf), both located relative to the running executable
// (see native/fetch-llm.ps1 and the installer's [Files] section).
//
// This exists so the standalone installed langbios.exe has an LLM
// fallback out of the box; it's a separate, independent path from
// langbios/llm_parser.py's Ollama-based fallback (which Python users
// can still use instead/as well).
#include "langbios/rule_parser.hpp"
#include <string>

namespace langbios {

// True if both the bundled llama-cli binary and a model file are
// present on disk. Cheap - just two filesystem checks.
bool LlmFallbackAvailable();

// Runs `text` through the bundled model, asking it to map the request
// onto the same Command shape the rule parser produces. Returns false
// if the fallback isn't bundled, the process fails, or its output
// can't be parsed as valid JSON; callers should treat that the same as
// "couldn't understand" rather than a hard error.
bool LlmFallbackParse(const std::string& text, Command& out);

} // namespace langbios
