# RCC - Rust-inspired C Compiler

A **C23-compliant** C compiler for **x86-64 Windows** that brings Rust-inspired safety features to C: ownership inference, borrow checking, and automatic memory management.

RCC compiles standard C code (C99/C11/C17/C23) to x86-64 MASM assembly, with optional memory safety analysis that catches use-after-free, double-free, and dangling pointer bugs at compile time.

## Features

### Language Support
- **Full C23** (ISO/IEC 9899:2024) with backward compatibility for C99, C11, C17
- Preprocessor with `#include`, `#define`, `#if`/`#elif`/`#else`, `#pragma`, variadic macros
- Complete type system: structs, unions, enums, typedefs, function pointers, bitfields
- All C operators, control flow, switch/case with fallthrough
- Variable-length arrays (VLA), designated initializers, compound literals
- `_Generic`, `_Static_assert`, `_Alignof`, `_Alignas`
- Binary literals (`0b1010`), digit separators, `#embed`, `typeof`

### Memory Safety (Rust-inspired)
- **Ownership inference** - tracks which variable owns each allocation
- **Borrow checking** - detects aliasing violations and use-after-free at compile time
- **Auto-free** - automatically inserts `free()` calls at scope exit for owned pointers
- Safety analysis runs as a compile-time pass (zero runtime overhead)
- Use `--safety=none` to disable for legacy code or false positives
- See [SAFETY.md](SAFETY.md) for full details

### Code Generation
- Targets **x86-64 Windows** (Microsoft x64 ABI)
- Outputs MASM-syntax assembly (compatible with ml64.exe and JWasm)
- Register allocation with peephole optimization (7 transform passes)
- Struct pass-by-value and return (up to any size)
- Full floating-point support (XMM registers, SSE2)
- Correct Windows shadow space and stack alignment

### Proven Compatibility
- **98 regression tests** passing
- **cJSON 1.7.17** - full library compiles and passes 17/17 tests
- **SQLite 3.45.1** - full amalgamation compiles, `sqlite3_exec("SELECT 1;")` works
- Complete C23 standard library headers (32 headers)

## Quick Start

### Pre-built Binaries

The `bin/` directory contains everything you need:

```cmd
:: Compile and link a C program
bin\rcc.exe hello.c -o hello.asm --safety=none
bin\jwasm.exe -win64 -Fo hello.obj hello.asm
bin\lld-link.exe /OUT:hello.exe /SUBSYSTEM:CONSOLE /ENTRY:main hello.obj ^
  /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
  msvcrt.lib kernel32.lib ucrt.lib legacy_stdio_definitions.lib
hello.exe
```

See [bin/README.md](bin/README.md) for details and [LINKING_GUIDE.md](LINKING_GUIDE.md) for library paths.

### Standalone (No CRT)

Link against only `kernel32.lib` for freestanding programs:

```cmd
bin\lld-link.exe /OUT:prog.exe /SUBSYSTEM:CONSOLE /ENTRY:main prog.obj ^
  /NODEFAULTLIB kernel32.lib /LIBPATH:"%SDK_UM%"
```

See [bin/hello_standalone.c](bin/hello_standalone.c) for a working example.

### Building from Source

Requirements:
- **CMake 3.15+**
- **Visual Studio 2022** (or MSVC Build Tools) with C++ workload

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

See [BUILDING.md](BUILDING.md) for full build instructions.

### Running Tests

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

## Compiler Options

| Option | Description |
|--------|-------------|
| `-o file.asm` | Output assembly file |
| `-c` | Compile and assemble (produce .obj) |
| `-link` | Compile, assemble, and link (produce .exe) |
| `--safety=none` | Disable ownership/borrow checking |
| `--version` | Show version and build info |
| `-E` | Preprocess only |
| `-D NAME=VALUE` | Define preprocessor macro |
| `-I path` | Add include search path |

## Architecture

```
Source (.c) --> Lexer --> Preprocessor --> Parser (AST)
                                            |
                                     Safety Analysis
                                    (ownership + borrow)
                                            |
                                        CodeGen
                                    (x86-64 assembly)
                                            |
                                     Peephole Optimizer
                                    (7 transform passes)
                                            |
                                      Output (.asm)
```

### Key Source Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, CLI argument parsing |
| `src/lexer.cpp` | Tokenizer |
| `src/cpp.cpp` | C preprocessor |
| `src/parser.cpp` | LALR parser, builds AST |
| `src/codegen.cpp` | x86-64 code generation |
| `src/regalloc.cpp` | Register allocation + peephole optimizer |
| `src/ownership.cpp` | Ownership inference |
| `src/borrow_checker.cpp` | Borrow checking analysis |
| `src/auto_free_pass.cpp` | Auto-free insertion pass |
| `src/auto_memory.cpp` | Memory tracking |
| `src/version.h` | Version definitions |

## Bundled Tools

| Tool | Version | Purpose |
|------|---------|---------|
| `rcc.exe` | v6.0.0 | RCC compiler |
| `jwasm.exe` | v2.21 | JWasm assembler (MASM-compatible) |
| `lld-link.exe` | LLVM 22.1.1 | LLVM linker (MSVC link.exe drop-in) |

## Documentation

| Document | Description |
|----------|-------------|
| [BUILDING.md](BUILDING.md) | Building from source, running tests |
| [LINKING_GUIDE.md](LINKING_GUIDE.md) | How to link with lld-link, library paths |
| [SAFETY.md](SAFETY.md) | How the Rust-inspired memory safety system works |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute, areas where help is needed |
| [bin/README.md](bin/README.md) | Pre-built binary usage |

## Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines
and areas where help is most needed (Linux port, optimizer, debug info, and more).

## Requirements

- **Windows 10/11 x64**
- **Windows SDK** (for kernel32.lib, ucrt.lib)
- **MSVC runtime libraries** (for msvcrt.lib) - from Visual Studio or Build Tools
- To build from source: CMake 3.15+ and Visual Studio 2022

## License

MIT License - see [LICENSE](LICENSE).
