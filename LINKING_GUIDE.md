# RCC Linking Guide

How to link RCC-compiled programs using `lld-link.exe` (LLVM linker) on Windows x64.

## Quick Reference

```cmd
rcc.exe hello.c -o hello.asm --safety=none
jwasm.exe -win64 -Fo hello.obj hello.asm
lld-link.exe /OUT:hello.exe /SUBSYSTEM:CONSOLE /ENTRY:main hello.obj ^
  /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
  msvcrt.lib kernel32.lib ucrt.lib legacy_stdio_definitions.lib
```

## Library Paths

Three library directories are needed. The exact paths depend on your Visual Studio
and Windows SDK versions.

### 1. MSVC Runtime Libraries

Contains the C runtime (msvcrt.lib), C++ runtime, and stdio definitions.

```
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\<version>\lib\x64
```

Current version on this machine: **14.44.35207**

Full path:
```
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\lib\x64
```

To find your version:
```cmd
dir "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC"
```

Visual Studio editions: Enterprise, Professional, Community, or BuildTools.

### 2. Windows SDK "um" Libraries (User-Mode)

Contains Windows API import libraries (kernel32, user32, advapi32, etc.).

```
C:\Program Files (x86)\Windows Kits\10\Lib\<version>\um\x64
```

Current version on this machine: **10.0.26100.0**

Full path:
```
C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64
```

To find your version (use the latest):
```cmd
dir "C:\Program Files (x86)\Windows Kits\10\Lib"
```

### 3. Windows SDK "ucrt" Libraries (Universal CRT)

Contains the Universal C Runtime (printf, malloc, string functions, etc.).

```
C:\Program Files (x86)\Windows Kits\10\Lib\<version>\ucrt\x64
```

Same SDK version as um: **10.0.26100.0**

Full path:
```
C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
```

## Required Libraries by Use Case

### Minimal Console Program (printf, malloc, etc.)

```
msvcrt.lib                    # C runtime startup + atexit
kernel32.lib                  # Windows kernel (ExitProcess, etc.)
ucrt.lib                      # Universal CRT (printf, malloc, string)
legacy_stdio_definitions.lib  # printf/scanf symbol definitions
```

### Program Using Windows API (CreateFile, threads, etc.)

Add as needed from the SDK um directory:

```
kernel32.lib    # Process, thread, file, memory, console APIs
user32.lib      # Window management, message loop, dialog boxes
advapi32.lib    # Registry, security, service APIs
ws2_32.lib      # Winsock2 networking
shell32.lib     # Shell functions (ShellExecute, etc.)
ole32.lib       # COM runtime
gdi32.lib       # Graphics Device Interface
```

### Program Using C11 Threads (threads.h)

Add the RCC threads runtime:

```
rcc_threads.lib  # bundled in bin/
```

### Program Using Math Functions (math.h)

Math functions are in ucrt.lib (already included in the minimal set).
No additional library needed.

## lld-link Command-Line Reference

### Essential Options

| Option | Description |
|--------|-------------|
| `/OUT:file.exe` | Output filename |
| `/SUBSYSTEM:CONSOLE` | Console application |
| `/SUBSYSTEM:WINDOWS` | GUI application (WinMain entry) |
| `/ENTRY:main` | Entry point (use `mainCRTStartup` for full CRT init) |
| `/LIBPATH:dir` | Add library search path (repeatable) |

### Entry Points

| Entry | Use When |
|-------|----------|
| `main` | Simple programs (skips CRT global init) |
| `mainCRTStartup` | Programs needing full CRT initialization (atexit, globals) |
| `WinMainCRTStartup` | GUI programs with WinMain |

### Debugging Options

| Option | Description |
|--------|-------------|
| `/DEBUG` | Generate PDB debug info |
| `/VERBOSE` | Show detailed linker output |
| `/MAP` | Generate map file |
| `/WX` | Treat warnings as errors |

### Other Useful Options

| Option | Description |
|--------|-------------|
| `/NODEFAULTLIB` | Ignore default libraries |
| `/DEFAULTLIB:name` | Add default library |
| `/STACK:size` | Set stack size (default 1MB) |
| `/HEAP:size` | Set heap size |
| `/LARGEADDRESSAWARE` | Enable >2GB address space |
| `/DYNAMICBASE` | Enable ASLR (default on) |
| `/NXCOMPAT` | Enable DEP (default on) |
| `/MANIFEST:NO` | Skip manifest generation |

## Environment Variables (Optional)

Set these to avoid typing long paths:

```cmd
set MSVC_LIB=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\lib\x64
set SDK_UM=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64
set SDK_UCRT=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
```

Then link with:
```cmd
lld-link.exe /OUT:hello.exe /SUBSYSTEM:CONSOLE /ENTRY:main hello.obj ^
  /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
  msvcrt.lib kernel32.lib ucrt.lib legacy_stdio_definitions.lib
```

## Complete End-to-End Example

```cmd
:: hello.c
:: #include <stdio.h>
:: int main(void) { printf("Hello, world!\n"); return 0; }

:: Step 1: Compile C to x86-64 assembly
bin\rcc.exe hello.c -o hello.asm --safety=none

:: Step 2: Assemble to COFF object file
bin\jwasm.exe -win64 -Fo hello.obj hello.asm

:: Step 3: Link to Windows executable
bin\lld-link.exe /OUT:hello.exe /SUBSYSTEM:CONSOLE /ENTRY:main hello.obj ^
  /LIBPATH:"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\lib\x64" ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" ^
  msvcrt.lib kernel32.lib ucrt.lib legacy_stdio_definitions.lib

:: Step 4: Run
hello.exe
:: Output: Hello, world!
```

## Standalone Linking (No CRT)

You can link programs without the C runtime (msvcrt, ucrt) by using `/NODEFAULTLIB`
and calling Windows API functions directly. This only requires `kernel32.lib` from
the Windows SDK.

### Why standalone?

- No dependency on MSVC installation (only Windows SDK needed)
- Smaller executables
- Full control over program startup
- Useful for drivers, bootloaders, embedded scenarios, or minimal tools

### What you lose

- No `printf`, `malloc`, `strlen`, etc. (must implement yourself or call Windows API)
- No CRT startup (global constructors, atexit, signal handling)
- No `errno`, `stdio` streams, `setjmp`/`longjmp`

### Standalone link command

```cmd
lld-link.exe /OUT:program.exe /SUBSYSTEM:CONSOLE /ENTRY:main ^
  program.obj /NODEFAULTLIB kernel32.lib ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
```

Only one library path (SDK um) and one library (kernel32.lib) needed.

### Example: hello_standalone.c

See `bin\hello_standalone.c` for a complete working example. The key pattern is
to declare Windows API functions with their correct signatures and call them directly:

```c
// Minimal Windows types
typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;
#define STD_OUTPUT_HANDLE ((DWORD)-11)

// Import from kernel32.dll (resolved via kernel32.lib)
extern HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
extern BOOL __stdcall WriteFile(HANDLE hFile, const void* lpBuffer,
                                DWORD nBytesToWrite, DWORD* lpBytesWritten,
                                void* lpOverlapped);
extern void __stdcall ExitProcess(unsigned int uExitCode);

int main(void) {
    const char* msg = "Hello from standalone RCC!\n";
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(out, msg, 27, &written, 0);
    ExitProcess(0);
    return 0;
}
```

Build and run:
```cmd
bin\rcc.exe hello_standalone.c -o hello_standalone.asm --safety=none
bin\jwasm.exe -win64 -Fo hello_standalone.obj hello_standalone.asm
bin\lld-link.exe /OUT:hello_standalone.exe /SUBSYSTEM:CONSOLE /ENTRY:main ^
  hello_standalone.obj /NODEFAULTLIB kernel32.lib ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
hello_standalone.exe
:: Output: Hello from standalone RCC!
```

### Useful Windows API functions for standalone programs

| Function | Purpose | DLL |
|----------|---------|-----|
| `GetStdHandle` | Get console handle | kernel32 |
| `WriteFile` | Write to file/console/pipe | kernel32 |
| `ReadFile` | Read from file/console/pipe | kernel32 |
| `ExitProcess` | Terminate process | kernel32 |
| `GetProcessHeap` | Get default heap handle | kernel32 |
| `HeapAlloc` | Allocate memory (replaces malloc) | kernel32 |
| `HeapFree` | Free memory (replaces free) | kernel32 |
| `CreateFileA` | Open/create file | kernel32 |
| `CloseHandle` | Close handle | kernel32 |
| `VirtualAlloc` | Allocate virtual memory pages | kernel32 |
| `VirtualFree` | Free virtual memory | kernel32 |
| `GetLastError` | Get error code | kernel32 |

All of these are in kernel32.lib, so no additional libraries are needed.

### Linking levels summary

| Level | Libraries | Lib Paths | What you get |
|-------|-----------|-----------|-------------|
| **Full CRT** | msvcrt + kernel32 + ucrt + legacy_stdio | MSVC + SDK um + SDK ucrt | printf, malloc, full C23 |
| **Standalone** | kernel32 only | SDK um only | Windows API only |

## Troubleshooting

**"undefined symbol: printf"** - Missing `legacy_stdio_definitions.lib`. The UCRT
exports printf as `__stdio_common_vfprintf`; the legacy lib provides the classic
`printf` symbol as a wrapper.

**"undefined symbol: mainCRTStartup"** - You used `/ENTRY:mainCRTStartup` but
did not link msvcrt.lib, or used `/NODEFAULTLIB` without providing the CRT.

**"cannot open input file 'kernel32.lib'"** - Missing `/LIBPATH` for the Windows
SDK um directory. Check that the SDK version path exists.

**"unresolved external symbol __security_check_cookie"** - Add `bufferoverflowU.lib`
from the SDK um directory (for /GS buffer security checks).
