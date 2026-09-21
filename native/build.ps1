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

Write-Host "Building langbios_native.dll ..."
& clang++ -std=c++20 @defs -I $inc -shared -o (Join-Path $out "langbios_native.dll") `
    @commonSources (Join-Path $src "capi.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "DLL build failed" }

Write-Host "Building langbios_cli.exe ..."
& clang++ -std=c++20 @defs -I $inc -o (Join-Path $out "langbios_cli.exe") `
    @commonSources (Join-Path $src "cli.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "CLI build failed" }

Write-Host "Build succeeded -> $out"
