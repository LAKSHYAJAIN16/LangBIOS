#!/usr/bin/env bash
# Builds the native Linux backend: langbios_native.so (loaded by Python via
# ctypes) and langbios_cli (standalone native CLI). Requires a real Linux
# box with efivarfs (a UEFI machine) to actually exercise firmware access
# at runtime; the build itself works on any Linux with a C++20 compiler.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/src"
LNX="$SRC/linux"
INC="$ROOT/include"
OUT="$ROOT/build"
mkdir -p "$OUT"

CXX="${CXX:-g++}"

COMMON_SOURCES=("$SRC/rule_parser.cpp" "$SRC/engine.cpp" "$SRC/llm_fallback.cpp")
LINUX_SOURCES=(
    "$LNX/efivarfs.cpp" "$LNX/uefi_vars.cpp" "$LNX/tpm.cpp" "$LNX/vendor_backend.cpp" "$LNX/audio.cpp"
)

echo "Building langbios_native.so ..."
"$CXX" -std=c++20 -fPIC -I "$INC" -shared -o "$OUT/langbios_native.so" \
    "${COMMON_SOURCES[@]}" "${LINUX_SOURCES[@]}" "$SRC/capi.cpp"

echo "Building langbios_cli ..."
"$CXX" -std=c++20 -I "$INC" -o "$OUT/langbios_cli" \
    "${COMMON_SOURCES[@]}" "${LINUX_SOURCES[@]}" "$SRC/cli.cpp"

# Bundled local NL-understanding assets (fetched separately via
# fetch-llm.sh, since the binaries/model are too large to commit) get
# copied alongside the built binary so llm_fallback can find them at
# ./llamacpp, ./models, and ./data relative to itself. Destination is
# wiped first: a stale set of binaries/model left over from switching
# versions has silently accumulated hundreds of MB of dead weight here
# before (see build.ps1's own history) - a fresh copy every time avoids
# that regardless of platform.
VENDOR_BIN="$ROOT/vendor/llamacpp/bin"
VENDOR_MODELS="$ROOT/vendor/models"
if [ -d "$VENDOR_BIN" ] && [ -d "$VENDOR_MODELS" ]; then
    echo "Copying bundled LLM assets into build/ ..."
    DEST_BIN="$OUT/llamacpp/bin"
    DEST_MODELS="$OUT/models"
    rm -rf "$DEST_BIN" "$DEST_MODELS"
    mkdir -p "$DEST_BIN" "$DEST_MODELS"
    cp -a "$VENDOR_BIN/." "$DEST_BIN/"
    cp -a "$VENDOR_MODELS"/*.gguf "$DEST_MODELS/"
else
    echo "No bundled LLM assets found (run native/fetch-llm.sh to get them) - CLI will run rule-parser-only."
fi

CANONICAL_EMBEDDINGS="$ROOT/data/canonical_embeddings.bin"
if [ -f "$CANONICAL_EMBEDDINGS" ]; then
    mkdir -p "$OUT/data"
    cp -f "$CANONICAL_EMBEDDINGS" "$OUT/data/"
fi

echo "Build succeeded -> $OUT"
