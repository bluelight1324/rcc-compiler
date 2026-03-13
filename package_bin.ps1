# package_bin.ps1 - Package RCC compiler runtime into bin/
# Usage: powershell -ExecutionPolicy Bypass -File package_bin.ps1
#
# This script copies all files needed to use rcc.exe as a standalone
# C compiler into the bin/ directory.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$binDir   = Join-Path $root "bin"
$libcDir  = Join-Path $binDir "libc"
$buildDir = Join-Path $root "build\Release"

# Create directories
if (-not (Test-Path $binDir))  { New-Item -ItemType Directory -Path $binDir  | Out-Null }
if (-not (Test-Path $libcDir)) { New-Item -ItemType Directory -Path $libcDir | Out-Null }

# 1. rcc.exe - the compiler
Write-Host "Copying rcc.exe..."
Copy-Item (Join-Path $buildDir "rcc.exe") $binDir -Force

# 2. jwasm.exe - bundled assembler (no MASM/Visual Studio required)
$jwasm = Join-Path $root "external\jwasm\build\MSVC64R\jwasm.exe"
if (Test-Path $jwasm) {
    Write-Host "Copying jwasm.exe..."
    Copy-Item $jwasm $binDir -Force
} else {
    Write-Warning "jwasm.exe not found - skipping"
}

# 3. lld-link.exe - LLVM linker (bundled, no MSVC link.exe required)
# Note: lld-link.exe is NOT rebuilt from source; it is downloaded once from
# the LLVM project releases. The packaging script preserves it if already present.
$lldLink = Join-Path $binDir "lld-link.exe"
if (-not (Test-Path $lldLink)) {
    Write-Warning "lld-link.exe not found in bin/ - download from LLVM releases"
    Write-Warning "  https://github.com/llvm/llvm-project/releases"
} else {
    Write-Host "lld-link.exe already present - keeping"
}

# 4. rcc_threads.lib - C11 threads runtime (optional, for threaded programs)
$threads = Join-Path $buildDir "rcc_threads.lib"
if (Test-Path $threads) {
    Write-Host "Copying rcc_threads.lib..."
    Copy-Item $threads $binDir -Force
}

# 5. libc/ headers - complete C23 standard library headers
Write-Host "Copying libc/ headers..."
$srcLibc = Join-Path $root "libc"
Get-ChildItem -Path $srcLibc -Filter "*.h" | ForEach-Object {
    Copy-Item $_.FullName $libcDir -Force
}

# 6. Version info
$rccExe = Join-Path $binDir "rcc.exe"
Write-Host ""
Write-Host "=== Package complete ==="
& $rccExe --version
Write-Host ""
Write-Host "Contents of bin/:"
Get-ChildItem $binDir -Recurse | ForEach-Object {
    $rel = $_.FullName.Substring($binDir.Length + 1)
    if ($_.PSIsContainer) {
        $sz = "dir"
    } else {
        $sz = "{0:N0} bytes" -f $_.Length
    }
    Write-Host "  $rel  ($sz)"
}
