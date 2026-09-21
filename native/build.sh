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

COMMON_SOURCES=("$SRC/rule_parser.cpp" "$SRC/engine.cpp")
LINUX_SOURCES=(
    "$LNX/efivarfs.cpp" "$LNX/uefi_vars.cpp" "$LNX/tpm.cpp" "$LNX/vendor_backend.cpp"
)

echo "Building langbios_native.so ..."
"$CXX" -std=c++20 -fPIC -I "$INC" -shared -o "$OUT/langbios_native.so" \
    "${COMMON_SOURCES[@]}" "${LINUX_SOURCES[@]}" "$SRC/capi.cpp"

echo "Building langbios_cli ..."
"$CXX" -std=c++20 -I "$INC" -o "$OUT/langbios_cli" \
    "${COMMON_SOURCES[@]}" "${LINUX_SOURCES[@]}" "$SRC/cli.cpp"

echo "Build succeeded -> $OUT"
