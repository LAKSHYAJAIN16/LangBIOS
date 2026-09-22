# Pass -HostArch to build for this machine's native architecture instead
# of x86_64 (useful for local development on ARM64 Windows).
param([switch]$HostArch)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src"
$win = Join-Path $src "windows"
$inc = Join-Path $root "include"
$out = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $out | Out-Null

# Platform-agnostic (shared with the Linux build via build.sh).
$commonSources = @("rule_parser.cpp", "engine.cpp", "llm_fallback.cpp") | ForEach-Object { Join-Path $src $_ }

# Windows-specific: Win32 firmware APIs + WMI/COM.
$windowsSources = @(
    "win_strings.cpp", "wmi_session.cpp", "smbios.cpp", "uefi_vars.cpp", "tpm.cpp",
    "vendor_detect.cpp", "vendor_dell.cpp", "vendor_hp.cpp", "vendor_lenovo.cpp"
) | ForEach-Object { Join-Path $win $_ }

$libs = @("-lwbemuuid", "-lole32", "-loleaut32", "-ladvapi32", "-lkernel32", "-luser32")
$defs = @("-DUNICODE", "-D_UNICODE")
$includes = @("-I", $inc, "-I", $win)

# Both targets are cross-compiled for x86_64 by default, for broadest
# compatibility: the DLL must match whatever Python interpreter loads it
# via ctypes (x86_64 is by far the most common), and the standalone CLI
# targets x86_64 too since ARM64 Windows runs x86_64 binaries fine via
# emulation but not the other way around - this is what the installer
# packages.
[string[]]$target = if ($HostArch) { @() } else { @("--target=x86_64-pc-windows-msvc") }

Write-Host "Building langbios_native.dll ..."
& clang++ -std=c++20 @target @defs @includes -shared -o (Join-Path $out "langbios_native.dll") `
    @commonSources @windowsSources (Join-Path $src "capi.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "DLL build failed" }

Write-Host "Building langbios_cli.exe ..."
& clang++ -std=c++20 @target @defs @includes -o (Join-Path $out "langbios_cli.exe") `
    @commonSources @windowsSources (Join-Path $src "cli.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "CLI build failed" }

# Bundled local LLM assets (fetched separately via fetch-llm.ps1, since
# they're large binaries not committed to git) get copied alongside the
# built exe so the CLI's llm_fallback can find them at ./llamacpp and
# ./models relative to itself - same layout the installer packages.
$vendorLlamacppBin = Join-Path $root "vendor\llamacpp\bin"
$vendorModels = Join-Path $root "vendor\models"
if ((Test-Path $vendorLlamacppBin) -and (Test-Path $vendorModels)) {
    Write-Host "Copying bundled LLM assets into build/ ..."
    $destBin = Join-Path $out "llamacpp\bin"
    $destModels = Join-Path $out "models"
    New-Item -ItemType Directory -Force -Path $destBin, $destModels | Out-Null
    Copy-Item -Force (Join-Path $vendorLlamacppBin "*") $destBin
    Copy-Item -Force (Join-Path $vendorModels "*.gguf") $destModels
} else {
    Write-Host "No bundled LLM assets found (run native\fetch-llm.ps1 to get them) - CLI will run rule-parser-only."
}

Write-Host "Build succeeded -> $out"
