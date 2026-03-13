# RCC Compiler Runtime - bin/

This directory contains the complete RCC compiler toolchain, ready to use
as a standalone C compiler on Windows x64.

## Contents

| File | Description |
|------|-------------|
| `rcc.exe` | RCC compiler - compiles C source to x86-64 assembly |
| `jwasm.exe` | JWasm assembler - assembles MASM-syntax .asm to .obj |
| `lld-link.exe` | LLVM linker - links .obj to .exe (drop-in MSVC link.exe replacement) |
| `rcc_threads.lib` | C11 threads runtime library |
| `libc/` | Complete C23 standard library headers |

## Quick Start

```cmd
:: Compile + assemble + link in one step
bin\rcc.exe hello.c -link

:: Or step by step:
bin\rcc.exe hello.c -o hello.asm
bin\jwasm.exe -win64 -Fo hello.obj hello.asm
bin\lld-link.exe /OUT:hello.exe /SUBSYSTEM:CONSOLE /ENTRY:main hello.obj ^
  /LIBPATH:"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\lib\x64" ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" ^
  msvcrt.lib kernel32.lib ucrt.lib legacy_stdio_definitions.lib
```

## Linking with lld-link

`lld-link.exe` is the LLVM project's drop-in replacement for Microsoft's `link.exe`.
It accepts the same command-line options and can link against the same Windows SDK
and MSVC runtime libraries.

Required libraries for a basic console program:
- `msvcrt.lib` - C runtime (from MSVC)
- `kernel32.lib` - Windows kernel API (from Windows SDK)
- `ucrt.lib` - Universal C runtime (from Windows SDK)
- `legacy_stdio_definitions.lib` - printf/scanf definitions (from MSVC)

Library search paths needed:
- MSVC lib: `...\VC\Tools\MSVC\<version>\lib\x64`
- Windows SDK um: `...\Windows Kits\10\Lib\<version>\um\x64`
- Windows SDK ucrt: `...\Windows Kits\10\Lib\<version>\ucrt\x64`

## Requirements

- **Windows 10/11 x64**
- **Windows SDK** and **MSVC libraries** (from Visual Studio or Build Tools)
  - Only the .lib files are needed; no MSVC executables required
- No other dependencies - rcc, jwasm, lld-link, and libc headers are all bundled

## Rebuilding

From the repository root:
```powershell
cmake --build build --config Release
powershell -ExecutionPolicy Bypass -File package_bin.ps1
```

Note: `lld-link.exe` is downloaded from the LLVM project releases, not built
from source. The packaging script preserves it if already present in bin/.

## Version

Run `rcc.exe --version` to see the current version and build number.
