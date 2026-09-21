$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src"
$win = Join-Path $src "windows"
$inc = Join-Path $root "include"
$out = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $out | Out-Null

# Platform-agnostic (shared with the Linux build via build.sh).
$commonSources = @("rule_parser.cpp", "engine.cpp") | ForEach-Object { Join-Path $src $_ }

# Windows-specific: Win32 firmware APIs + WMI/COM.
$windowsSources = @(
    "win_strings.cpp", "wmi_session.cpp", "smbios.cpp", "uefi_vars.cpp", "tpm.cpp",
    "vendor_detect.cpp", "vendor_dell.cpp", "vendor_hp.cpp", "vendor_lenovo.cpp"
) | ForEach-Object { Join-Path $win $_ }

$libs = @("-lwbemuuid", "-lole32", "-loleaut32", "-ladvapi32", "-lkernel32", "-luser32")
$defs = @("-DUNICODE", "-D_UNICODE")
$includes = @("-I", $inc, "-I", $win)

# The DLL is loaded by whatever Python interpreter is on PATH via ctypes,
# which must match architecture exactly - on this ARM64 Windows machine
# the installed Python is x64 (running under emulation), so the DLL is
# cross-compiled for x86_64 even though the native CLI below targets the
# host's real architecture (ARM64) directly. Adjust --target if your
# system Python's architecture differs.
Write-Host "Building langbios_native.dll (x86_64, to match the system Python) ..."
& clang++ -std=c++20 --target=x86_64-pc-windows-msvc @defs @includes -shared -o (Join-Path $out "langbios_native.dll") `
    @commonSources @windowsSources (Join-Path $src "capi.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "DLL build failed" }

Write-Host "Building langbios_cli.exe (native host architecture) ..."
& clang++ -std=c++20 @defs @includes -o (Join-Path $out "langbios_cli.exe") `
    @commonSources @windowsSources (Join-Path $src "cli.cpp") @libs
if ($LASTEXITCODE -ne 0) { throw "CLI build failed" }

Write-Host "Build succeeded -> $out"
