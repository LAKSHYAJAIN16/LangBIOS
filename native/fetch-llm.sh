#!/usr/bin/env bash
# Linux/macOS counterpart to fetch-llm.ps1: fetches the bundled local
# natural-language-understanding assets into native/vendor/ (gitignored -
# large binaries, never committed). Run this once before running the
# native CLI if you want phrasing the rule parser misses to be understood
# without an internet connection or an API key.
#
# Architecture: semantic similarity matching, not text generation - see
# fetch-llm.ps1's header comment for the full rationale (classification
# task, not open-ended chat; embedding model gives a real confidence
# score and can't hallucinate an invalid setting).
#
# Downloads:
#   - llama.cpp (CPU-only) prebuilt binaries for this OS/arch, run in
#     --embedding server mode (not the text-generation CLI)
#   - bge-small-en-v1.5, Q8_0 quantized GGUF (~37MB) from Hugging Face
#
# NOTE ON TRIMMING: the Windows fetch script's bundled-file list was
# verified against llama-server.exe's real dependency chain via
# `llvm-objdump -p`. This dev environment has no Linux/macOS box to run
# the equivalent (`ldd`/`otool -L`) against these binaries, so the list
# below is inferred from that verified Windows chain plus the matching
# library names visible in these release archives (llama-server-impl,
# mtmd, llama, ggml-base, ggml, llama-common - same names, same build,
# different OS toolchain). If you have real Linux/macOS hardware, running
# `ldd ./llama-server` / `otool -L ./llama-server` and diffing against
# this list is worth doing before trusting it blind.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENDOR="$ROOT/vendor"
LLAMACPP_DIR="$VENDOR/llamacpp"
BIN_DIR="$LLAMACPP_DIR/bin"
MODELS_DIR="$VENDOR/models"

mkdir -p "$BIN_DIR" "$MODELS_DIR"

RELEASE_TAG="b11094"
OS_NAME="$(uname -s)"
ARCH="$(uname -m)"

case "$OS_NAME" in
    Linux)
        LIB_EXT="so"
        case "$ARCH" in
            x86_64) ASSET="llama-${RELEASE_TAG}-bin-ubuntu-x64.tar.gz" ;;
            aarch64|arm64) ASSET="llama-${RELEASE_TAG}-bin-ubuntu-arm64.tar.gz" ;;
            *) echo "Unsupported Linux architecture: $ARCH" >&2; exit 1 ;;
        esac
        # ggml's per-microarchitecture CPU backends (x86_64 only - loaded
        # dynamically at runtime based on the actual CPU, so every variant
        # ships; arm64 has no equivalent split).
        CPU_BACKEND_GLOB="libggml-cpu-*.$LIB_EXT"
        EXTRA_LIBS=()
        ;;
    Darwin)
        LIB_EXT="dylib"
        case "$ARCH" in
            arm64) ASSET="llama-${RELEASE_TAG}-bin-macos-arm64.tar.gz" ;;
            x86_64) ASSET="llama-${RELEASE_TAG}-bin-macos-x64.tar.gz" ;;
            *) echo "Unsupported macOS architecture: $ARCH" >&2; exit 1 ;;
        esac
        # macOS ships one generic CPU backend (no per-microarch split like
        # x86_64 Linux/Windows) plus Metal (GPU) and BLAS (Accelerate)
        # backends - kept for the same "loaded dynamically, not in the
        # static import table" reason as the Windows/Linux CPU variants.
        CPU_BACKEND_GLOB="libggml-cpu*.$LIB_EXT"
        EXTRA_LIBS=("libggml-metal*.$LIB_EXT" "libggml-blas*.$LIB_EXT")
        ;;
    *)
        echo "Unsupported OS: $OS_NAME" >&2
        exit 1
        ;;
esac

SERVER_BIN="llama-server"
NEEDED_GLOBS=(
    "$SERVER_BIN" "libllama-server-impl.$LIB_EXT*" "libmtmd.$LIB_EXT*"
    "libllama.$LIB_EXT*" "libllama-common.$LIB_EXT*" "libggml.$LIB_EXT*"
    "libggml-base.$LIB_EXT*" "$CPU_BACKEND_GLOB" "LICENSE"
)
NEEDED_GLOBS+=("${EXTRA_LIBS[@]}")

if [ ! -f "$BIN_DIR/$SERVER_BIN" ]; then
    echo "Downloading llama.cpp $RELEASE_TAG ($ASSET) ..."
    ARCHIVE="$LLAMACPP_DIR/llamacpp.tar.gz"
    curl -fL -o "$ARCHIVE" "https://github.com/ggml-org/llama.cpp/releases/download/$RELEASE_TAG/$ASSET"
    EXTRACT_DIR="$LLAMACPP_DIR/_extract"
    rm -rf "$EXTRACT_DIR"
    mkdir -p "$EXTRACT_DIR"
    tar -xzf "$ARCHIVE" -C "$EXTRACT_DIR"
    rm -f "$ARCHIVE"

    RELEASE_ROOT="$(find "$EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d | head -n1)"
    for glob in "${NEEDED_GLOBS[@]}"; do
        # shellcheck disable=SC2086
        cp -a $RELEASE_ROOT/$glob "$BIN_DIR/" 2>/dev/null || true
    done
    rm -rf "$EXTRACT_DIR"

    if [ ! -f "$BIN_DIR/$SERVER_BIN" ]; then
        echo "Failed to extract $SERVER_BIN from $ASSET - archive layout may have changed." >&2
        exit 1
    fi
    chmod +x "$BIN_DIR/$SERVER_BIN"
    echo "llama.cpp binaries -> $BIN_DIR"
else
    echo "llama.cpp binaries already present, skipping."
fi

MODEL_URL="https://huggingface.co/CompendiumLabs/bge-small-en-v1.5-gguf/resolve/main/bge-small-en-v1.5-q8_0.gguf"
MODEL_PATH="$MODELS_DIR/bge-small-en-v1.5-q8_0.gguf"

if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading bge-small-en-v1.5 Q8_0 (~37MB) ..."
    curl -fL -o "$MODEL_PATH" "$MODEL_URL"
    echo "Model -> $MODEL_PATH"
else
    echo "Model already present, skipping."
fi

echo "Done. LLM assets ready in $VENDOR"

EMBEDDINGS_PATH="$ROOT/data/canonical_embeddings.bin"
if [ -f "$EMBEDDINGS_PATH" ]; then
    echo "Canonical embeddings already present ($EMBEDDINGS_PATH) - committed to git, no regeneration needed here."
else
    echo "WARNING: $EMBEDDINGS_PATH is missing. It's normally committed to git;" >&2
    echo "regenerating it requires embed-intents.ps1 (Windows/PowerShell only) - run that on a Windows machine after editing canonical_intents.json." >&2
fi
