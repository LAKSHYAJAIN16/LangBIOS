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

if (-not (Test-Path (Join-Path $binDir "llama-cli.exe"))) {
    Write-Host "Downloading llama.cpp $releaseTag (CPU x64, ~19MB) ..."
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath
    Expand-Archive -Path $zipPath -DestinationPath $binDir -Force
    Remove-Item $zipPath
    Write-Host "llama.cpp binaries -> $binDir"
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
