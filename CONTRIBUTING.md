# Contributing to RCC

Thank you for your interest in contributing to RCC! This project is open source
and we welcome contributions from the community.

## Ways to Contribute

### Report Bugs
- Open an [issue](https://github.com/bluelight1324/rcc-compiler/issues) with:
  - A minimal C source file that reproduces the problem
  - The exact command line you used
  - Expected vs actual output or behavior
  - Your Windows version and Visual Studio version

### Fix Bugs
- Check the [issues](https://github.com/bluelight1324/rcc-compiler/issues) page for open bugs
- Comment on an issue to let others know you are working on it
- Submit a pull request with your fix and a test case

### Add Test Cases
- We have 98 passing tests but always need more coverage
- Tests go in `tests/unit/` as `.c` files
- Each test should print `PASS` on success and `FAIL` on failure to stdout
- See existing tests for the pattern

### Improve Code Generation
- Better register allocation
- More peephole optimizations (see `src/regalloc.cpp`)
- Floating-point optimization
- Better struct layout and access patterns

### Extend Language Support
- Improve C23 feature coverage
- Better error messages and recovery
- Additional preprocessor features

### Improve Safety Analysis
- Reduce false positives in ownership inference
- Better tracking through function calls
- Inter-procedural analysis

### Documentation
- Improve the README, guides, and inline comments
- Write tutorials or blog posts about RCC
- Document compiler internals

## Development Setup

1. Clone the repo:
```bash
git clone https://github.com/bluelight1324/rcc-compiler.git
cd rcc-compiler
```

2. Build (requires Visual Studio 2022):
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

3. Run tests:
```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

## Pull Request Guidelines

1. **One feature or fix per PR** - keep changes focused
2. **Add a test** - if you fix a bug, add a test that would have caught it
3. **Don't break existing tests** - run the full test suite before submitting
4. **Keep it simple** - prefer clear, straightforward code over clever tricks
5. **Match the existing style** - look at surrounding code for conventions

## Architecture Overview

```
Source (.c)
  |
  v
Lexer (src/lexer.cpp) -----> Token stream
  |
  v
Preprocessor (src/cpp.cpp) -> Expanded token stream
  |
  v
Parser (src/parser.cpp) ----> AST (Abstract Syntax Tree)
  |
  v
Safety Analysis ------------> Warnings (optional)
  (src/ownership.cpp)
  (src/borrow_checker.cpp)
  (src/auto_free_pass.cpp)
  |
  v
CodeGen (src/codegen.cpp) --> x86-64 assembly
  |
  v
Peephole (src/regalloc.cpp) -> Optimized assembly
  |
  v
Output (.asm)
```

Key things to know:
- The AST is defined in `src/ast.h` (node kinds, type system)
- Code generation targets MASM syntax for the x86-64 Microsoft ABI
- The peephole optimizer makes 7 passes over the assembly output
- Safety analysis works on the AST, not the generated code

## Areas Where Help is Most Needed

### High Priority
- **Linux/macOS port** - currently Windows-only; would need ELF/Mach-O output and System V ABI
- **Better error messages** - line numbers, column numbers, source context in errors
- **Optimizer improvements** - constant folding, dead code elimination, common subexpression elimination

### Medium Priority
- **Debug info** - generate DWARF or CodeView debug information
- **Static analysis** - null pointer checks, array bounds, integer overflow
- **C++ subset** - classes, references, overloading (ambitious but valuable)

### Nice to Have
- **LSP server** - language server protocol for IDE integration
- **Cross-compilation** - target ARM64 or other architectures
- **WebAssembly backend** - compile C to WASM

## Code of Conduct

Be respectful and constructive. We are here to build a great compiler together.
Focus on the code, not the person. Everyone was a beginner once.

## Questions?

Open a [discussion](https://github.com/bluelight1324/rcc-compiler/issues) or
reach out in the issues. No question is too basic.
