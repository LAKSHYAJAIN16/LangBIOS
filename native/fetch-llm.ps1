# Fetches the bundled local natural-language-understanding assets into
# native/vendor/ (gitignored - large binaries, never committed). Run this
# once before building the installer, or before running the native CLI
# if you want phrasing the rule parser misses to be understood without
# Ollama.
#
# Architecture: semantic similarity matching, not text generation. The
# actual task here is classification (map one sentence onto one of a
# few dozen known settings/actions), not open-ended chat - a small
# *embedding* model that turns text into a comparable vector fits that
# shape far better than a generative LLM, and is dramatically smaller
# since it doesn't need a large vocabulary-sized output layer. Canonical
# example phrases (native/data/canonical_intents.json) are embedded once
# offline (see embed-intents.ps1); at runtime, the fallback embeds the
# user's text and finds the nearest canonical example by cosine
# similarity - deterministic, can't hallucinate an invalid setting, and
# gives a real confidence score to reject nonsense input.
#
# Downloads:
#   - llama.cpp (CPU-only, x64) prebuilt binaries, run in --embedding
#     server mode (not the text-generation CLI)
#   - bge-small-en-v1.5, Q8_0 quantized GGUF (~37MB) from Hugging Face
#     (MIT licensed, 384-dim sentence embeddings, 33M params)
#
# Tried and rejected (generative LLM approach): Qwen2.5-0.5B-Instruct at
# Q4_K_M (~490MB) worked but was 13x the size of this approach for worse
# reliability (occasional malformed JSON, no natural confidence signal
# to reject gibberish). SmolLM2-360M-Instruct (~270MB) was smaller but
# got actions backwards and hallucinated settings for nonsense input.
# The embedding-similarity approach beats both on size, latency, AND
# correctness - it's simply the right tool for a classification task.
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

# llama-server.exe's real, traced dependency chain (verified with
# llvm-objdump -p): llama-server.exe -> llama-server-impl.dll ->
# mtmd.dll, llama.dll, ggml-base.dll, ggml.dll, llama-common.dll;
# llama-common.dll -> llama.dll, ggml.dll, ggml-base.dll; ggml-base.dll
# -> libomp.dll. Every ggml-cpu-<arch>.dll variant is kept - those are
# loaded dynamically at runtime based on the actual CPU, not visible in
# the static import table, so trimming them would break whichever end
# user doesn't have that exact microarchitecture. Everything else in
# the release zip (~20 standalone tools this project never invokes,
# plus llama-cli.exe itself - superseded here by llama-server.exe in
# embedding mode) is dropped.
$neededFiles = @(
    "llama-server.exe", "llama-server-impl.dll",
    "llama-common.dll", "llama.dll", "mtmd.dll", "ggml.dll", "ggml-base.dll",
    "libomp.dll", "LICENSE-LLVM-OpenMP"
)

if (-not (Test-Path (Join-Path $binDir "llama-server.exe"))) {
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

$modelUrl = "https://huggingface.co/CompendiumLabs/bge-small-en-v1.5-gguf/resolve/main/bge-small-en-v1.5-q8_0.gguf"
$modelPath = Join-Path $modelsDir "bge-small-en-v1.5-q8_0.gguf"

if (-not (Test-Path $modelPath)) {
    Write-Host "Downloading bge-small-en-v1.5 Q8_0 (~37MB) ..."
    Invoke-WebRequest -Uri $modelUrl -OutFile $modelPath
    Write-Host "Model -> $modelPath"
} else {
    Write-Host "Model already present, skipping."
}

Write-Host "Done. LLM assets ready in $vendor"

$embeddingsPath = Join-Path $root "data\canonical_embeddings.bin"
if (-not (Test-Path $embeddingsPath)) {
    Write-Host ""
    Write-Host "Precomputing canonical intent embeddings ..."
    & (Join-Path $root "embed-intents.ps1")
} else {
    Write-Host "Canonical embeddings already present ($embeddingsPath) - re-run embed-intents.ps1 directly if you edited canonical_intents.json."
}
