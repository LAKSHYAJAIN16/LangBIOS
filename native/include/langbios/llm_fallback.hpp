#pragma once
// Local natural-language fallback for the native CLI/library - no
// Ollama, no API key, no external network call. For phrasing the rule
// parser misses, embeds the user's text with a small bundled model
// (llamacpp/bin/llama-server[.exe] run in --embedding mode, serving
// bge-small-en-v1.5) and finds the nearest canonical example phrase by
// cosine similarity against native/data/canonical_embeddings.bin
// (precomputed offline by embed-intents.ps1).
//
// This is a classification task, not open-ended generation, so a small
// embedding model - which only needs to produce a fixed-size vector,
// not generate text over a large vocabulary - fits far better than a
// generative LLM: much smaller, comparable latency, and deterministic
// (it can only ever return one of the known canonical intents, so it
// can't hallucinate an invalid setting the way generation could).
//
// This exists so the standalone installed langbios.exe has this
// fallback out of the box; it's a separate, independent path from
// langbios/llm_parser.py's Ollama-based fallback (which Python users
// can still use instead/as well, for genuinely open-ended phrasing).
#include "langbios/rule_parser.hpp"
#include <string>

namespace langbios {

// True if the bundled embedding server, model, and precomputed
// canonical embeddings are all present on disk. Cheap filesystem checks.
bool LlmFallbackAvailable();

// Embeds `text` and matches it against the canonical intent set. Returns
// false if the fallback isn't bundled, the embedding process fails, or
// the best match's similarity falls below the confidence threshold;
// callers should treat that the same as "couldn't understand" rather
// than a hard error.
bool LlmFallbackParse(const std::string& text, Command& out);

} // namespace langbios
