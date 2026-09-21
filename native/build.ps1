$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src"
$inc = Join-Path $root "include"
$out = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$commonSources = @(
    "wmi_session.cpp", "smbios.cpp", "uefi_vars.cpp", "tpm.cpp",
    "vendor_detect.cpp", "vendor_dell.cpp", "vendor_hp.cpp", "vendor_lenovo.cpp",
    "rule_parser.cpp", "engine.cpp"
) | ForEach-Object { Join-Path $src $_ }

$libs = @("-lwbemuuid", "-lole32", "-loleaut32", "-ladvapi32", "-lkernel32", "-luser32")

$defs = @("-DUNICODE", "-D_UNICODE")

# The DLL is loaded by whatever Python interpreter is on PATH via ctypes,
# which must match architecture exactly - on this ARM64 Windows machine
# the installed Python is x64 (running under emulation), so the DLL is
# cross-compiled for x86_64 even though the native CLI below targets the
# host's real architecture (ARM64) directly.
Write-Host "Building langbios_native.dll (x86_64, to match the system Python) ..."
& clang++ -std=c++20 --target=x86_64-pc-windows-msvc @defs -I $inc -shared -o (Join-Path $out "langbios_native.dll") `
    @commonSources (Join-Path $src "capi.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "DLL build failed" }

Write-Host "Building langbios_cli.exe (native host architecture) ..."
& clang++ -std=c++20 @defs -I $inc -o (Join-Path $out "langbios_cli.exe") `
    @commonSources (Join-Path $src "cli.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "CLI build failed" }

Write-Host "Build succeeded -> $out"
