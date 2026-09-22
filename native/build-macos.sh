#!/usr/bin/env bash
# Builds the native macOS backend: langbios_native.dylib (loaded by
# Python via ctypes) and langbios_cli (standalone native CLI).
#
# Scope is deliberately thin compared to Windows/Linux: Apple publishes
# no vendor BIOS-management interface at all, there's no TPM (Secure
# Enclave instead, closed/no public API), and Secure Boot is only
# reachable from recoveryOS - none of that is a gap in this code, see
# native/src/macos/*.cpp for why. Boot disk selection (the closest
# analog to boot_order) only works on Intel Macs via `bless`.
#
# NOTE: written for macOS's real toolchain (clang++ via Xcode Command
# Line Tools) but not run on real Mac hardware in this environment
# (Windows-only development machine) - please verify before trusting
# the write paths.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/src"
MAC="$SRC/macos"
INC="$ROOT/include"
OUT="$ROOT/build"
mkdir -p "$OUT"

CXX="${CXX:-clang++}"

COMMON_SOURCES=("$SRC/rule_parser.cpp" "$SRC/engine.cpp" "$SRC/llm_fallback.cpp")
MACOS_SOURCES=(
    "$MAC/uefi_vars.cpp" "$MAC/tpm.cpp" "$MAC/vendor_backend.cpp" "$MAC/audio.cpp"
)

echo "Building langbios_native.dylib ..."
"$CXX" -std=c++20 -fPIC -I "$INC" -shared -o "$OUT/langbios_native.dylib" \
    "${COMMON_SOURCES[@]}" "${MACOS_SOURCES[@]}" "$SRC/capi.cpp"

echo "Building langbios_cli ..."
"$CXX" -std=c++20 -I "$INC" -o "$OUT/langbios_cli" \
    "${COMMON_SOURCES[@]}" "${MACOS_SOURCES[@]}" "$SRC/cli.cpp"

echo "Build succeeded -> $OUT"
