# Precomputes embeddings for native/data/canonical_intents.json using the
# bundled embedding model, and writes native/data/canonical_embeddings.bin -
# a small binary file the native LLM fallback loads at runtime instead of
# re-embedding every canonical example on every single command (which
# would be pure wasted latency, since the canonical set never changes at
# runtime). Re-run this whenever canonical_intents.json is edited.
#
# Binary format (little-endian):
#   uint32 count
#   repeated `count` times:
#     uint32 actionCode (0=get, 1=set, 2=list, 3=reset)
#     uint32 settingLen, <settingLen bytes UTF8>
#     uint32 valueLen,   <valueLen bytes UTF8>
#     384 x float32      (embedding vector; bge-small-en-v1.5 is 384-dim)
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dataDir = Join-Path $root "data"
$intentsPath = Join-Path $dataDir "canonical_intents.json"
$outPath = Join-Path $dataDir "canonical_embeddings.bin"
$serverExe = Join-Path $root "vendor\llamacpp\bin\llama-server.exe"
$modelPath = Join-Path $root "vendor\models\bge-small-en-v1.5-q8_0.gguf"

if (-not (Test-Path $serverExe)) { throw "llama-server.exe not found - run native\fetch-llm.ps1 first." }
if (-not (Test-Path $modelPath)) { throw "Embedding model not found - run native\fetch-llm.ps1 first." }

$port = 8931
Write-Host "Starting embedding server ..."
$proc = Start-Process -FilePath $serverExe -ArgumentList "-m", "`"$modelPath`"", "--embedding", "--port", $port, "--log-disable" -PassThru -WindowStyle Hidden

try {
    $ready = $false
    for ($i = 0; $i -lt 100; $i++) {
        try {
            $resp = Invoke-RestMethod -Uri "http://127.0.0.1:$port/health" -TimeoutSec 1 -ErrorAction Stop
            if ($resp.status -eq "ok") { $ready = $true; break }
        } catch {}
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw "Embedding server did not become ready in time." }

    $intents = Get-Content $intentsPath -Raw | ConvertFrom-Json
    Write-Host "Embedding $($intents.Count) canonical phrases ..."

    $actionCodes = @{ "get" = 0; "set" = 1; "list" = 2; "reset" = 3 }
    $stream = [System.IO.File]::Create($outPath)
    $writer = New-Object System.IO.BinaryWriter($stream)
    $writer.Write([uint32]$intents.Count)

    $utf8 = [System.Text.Encoding]::UTF8
    $i = 0
    foreach ($intent in $intents) {
        $i++
        $body = @{ input = @($intent.phrase) } | ConvertTo-Json -Compress
        $resp = Invoke-RestMethod -Uri "http://127.0.0.1:$port/v1/embeddings" -Method Post -ContentType "application/json" -Body $body
        $vec = $resp.data[0].embedding

        $writer.Write([uint32]$actionCodes[$intent.action])

        # Declare-then-assign, not an if/else *expression* result: piping a
        # byte[] through an expression's output stream silently unrolls it
        # into a boxed Object[], which BinaryWriter.Write() then serializes
        # completely wrong (corrupts every field after it). Plain
        # statement-form assignment inside a bare `if` keeps the byte[]
        # typing intact.
        [byte[]]$settingBytes = @()
        if ($intent.setting) { $settingBytes = $utf8.GetBytes([string]$intent.setting) }
        $writer.Write([uint32]$settingBytes.Length)
        if ($settingBytes.Length -gt 0) { $writer.Write($settingBytes) }

        [byte[]]$valueBytes = @()
        if ($null -ne $intent.value) { $valueBytes = $utf8.GetBytes([string]$intent.value) }
        $writer.Write([uint32]$valueBytes.Length)
        if ($valueBytes.Length -gt 0) { $writer.Write($valueBytes) }

        foreach ($f in $vec) { $writer.Write([float]$f) }

        if ($i % 10 -eq 0) { Write-Host "  $i / $($intents.Count)" }
    }

    $writer.Close()
    $stream.Close()
    Write-Host "Wrote $outPath ($([math]::Round((Get-Item $outPath).Length / 1KB, 1)) KB)"
} finally {
    Write-Host "Stopping embedding server ..."
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
