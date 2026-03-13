# Building RCC from Source

## Prerequisites

- **Windows 10/11 x64**
- **CMake 3.15+** ([cmake.org](https://cmake.org/download/))
- **Visual Studio 2022** with "Desktop development with C++" workload
  - Or standalone MSVC Build Tools 2022

## Build Steps

```powershell
# Clone the repository
git clone https://github.com/bluelight1324/rcc-compiler.git
cd rcc-compiler

# Configure (generates Visual Studio solution)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build (Release mode)
cmake --build build --config Release
```

The compiler is produced at `build\Release\rcc.exe`.

## Package the Runtime

After building, run the packaging script to update `bin/`:

```powershell
powershell -ExecutionPolicy Bypass -File package_bin.ps1
```

This copies `rcc.exe`, `rcc_threads.lib`, and libc headers into `bin/`.
Note: `jwasm.exe` and `lld-link.exe` are pre-built and already in `bin/`.

## Build Number

Each build auto-increments the build number stored in `build_number.txt`.
The version is defined in `src/version.h`.

## Running Tests

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

To run a subset:
```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -Filter "struct"
```

## Project Structure

```
rcc-compiler/
  src/              # Compiler source (C++)
  libc/             # C23 standard library headers (used by compiled programs)
  bin/              # Pre-built binaries and runtime
  tests/            # Test runner + 102 unit tests
  cmake/            # CMake helper scripts
  CMakeLists.txt    # Build configuration
```
