# Fetches the bundled local LLM assets into native/vendor/ (gitignored -
# these are large binaries, never committed). Run this once before
# building the installer, or before running the native CLI if you want
# the LLM fallback available without Ollama.
#
# Downloads:
#   - llama.cpp (CPU-only, x64) prebuilt binaries from its GitHub releases
#   - Qwen2.5-0.5B-Instruct, Q4_K_M quantized GGUF (~470MB) from Hugging Face
#     (Apache 2.0 licensed, small enough for CPU-only inference to be
#     reasonably fast, good enough for structured single-command JSON
#     output even though it's not a strong general chat model)
#
# Tried and rejected: SmolLM2-360M-Instruct (smaller vocab, ~270MB at
# Q4_K_M - 45% smaller). Tested head-to-head on this project's exact
# JSON-extraction prompts: it produced verbose multi-line JSON that
# got truncated at the token budget, got a plain "enable secure boot"
# backwards (returned action "get" instead of "set"), and confidently
# hallucinated a setting for pure gibberish input instead of reporting
# "unknown" - a materially worse failure mode than Qwen's. Not worth
# the size savings; re-test before swapping again.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vendor = Join-Path $root "vendor"
$llamacppDir = Join-Path $vendor "llamacpp"
$binDir = Join-Path $llamacppDir "bin"
$modelsDir = Join-Path $vendor "models"

New-Item -ItemType Directory -Force -Path $binDir, $modelsDir | Out-Null

$releaseTag = "b11094"
$zipUrl = "https://github.com/ggml-org/llama.cpp/releases/download/$releaseTag/llama-$releaseTag-bin-win-cpu-x64.zip"
$zipPath = Join-Path $llamacppDir "llama-cpu-x64.zip"

# Only llama-cli.exe's real, traced dependency chain (verified with
# llvm-objdump -p): llama-cli-impl -> llama-server-impl -> mtmd/llama/
# ggml-base/ggml; llama-common -> llama/ggml/ggml-base; ggml-base ->
# libomp. Every ggml-cpu-<arch>.dll variant is kept - those are loaded
# dynamically at runtime based on the actual CPU, not visible in the
# static import table, so trimming them would break the wrong end
# user's machine. The release zip also ships ~20 standalone tools
# (quantize/perplexity/bench/multimodal CLIs, an RPC server) this
# project never invokes; those and their private *-impl.dlls are
# dropped. Note this barely changes the final installer size (LZMA
# already compresses those tiny stub .exes to near nothing) - it's
# real, verified cleanup, just not a meaningful size lever. The model
# file (see below) is the actual size driver.
$neededFiles = @(
    "llama-cli.exe", "llama-cli-impl.dll", "llama-server-impl.dll",
    "llama-common.dll", "llama.dll", "mtmd.dll", "ggml.dll", "ggml-base.dll",
    "libomp.dll", "LICENSE-LLVM-OpenMP"
)

if (-not (Test-Path (Join-Path $binDir "llama-cli.exe"))) {
    Write-Host "Downloading llama.cpp $releaseTag (CPU x64, ~19MB) ..."
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath
    $extractDir = Join-Path $llamacppDir "_extract"
    Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force
    Remove-Item $zipPath

    foreach ($f in $neededFiles) {
        Copy-Item (Join-Path $extractDir $f) $binDir -Force
    }
    Copy-Item (Join-Path $extractDir "ggml-cpu-*.dll") $binDir -Force

    Remove-Item -Recurse -Force $extractDir
    Write-Host "llama.cpp binaries (trimmed) -> $binDir"
} else {
    Write-Host "llama.cpp binaries already present, skipping."
}

$modelUrl = "https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf"
$modelPath = Join-Path $modelsDir "qwen2.5-0.5b-instruct-q4_k_m.gguf"

if (-not (Test-Path $modelPath)) {
    Write-Host "Downloading Qwen2.5-0.5B-Instruct Q4_K_M (~470MB) ..."
    Invoke-WebRequest -Uri $modelUrl -OutFile $modelPath
    Write-Host "Model -> $modelPath"
} else {
    Write-Host "Model already present, skipping."
}

Write-Host "Done. LLM assets ready in $vendor"
