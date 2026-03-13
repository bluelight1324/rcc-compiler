/**
 * RCC - Rust-inspired C Compiler
 *
 * A C compiler with planned Rust-style memory safety analysis.
 * Targets x86-64 Windows (MASM/ml64).
 *
 * Features:
 *   - Full C preprocessor (#define, #include, #if, etc.)
 *   - LALR(1) parser (232 productions)
 *   - Direct AST-to-assembly code generation
 *   - Multi-file compilation and linking
 *   - (Planned) Ownership inference and borrow checking
 *
 * Usage:
 *   rcc <input.c> [-o output.asm] [-c] [-link]
 *   rcc <file1.c> <file2.c> ... -link
 *
 * Options:
 *   -o <file>   Output assembly file
 *   -c          Assemble (produce .obj)
 *   -link       Assemble and link (produce .exe)
 *   -E          Preprocess only
 *   -no-cpp     Skip preprocessing
 *   -I <path>   Add include search path
 *   -D name     Define macro
 *   -D name=val Define macro with value
 *   -std=<std>  Set C standard (c89|c99|c11|c17|c18|c23); default: c17
 *
 * Copyright (c) 2024-2026
 * License: MIT
 */

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "cpp.h"
#include "safety.h"
#include "type_enrichment.h"
#include "peephole.h"
#include "regalloc.h"
#include "ownership.h"
#include "borrow_checker.h"
#include "auto_free_pass.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <sys/stat.h>

#include "version.h"

// P4-B: ANSI colour diagnostics
// Enabled automatically when stderr is a real terminal, or by -fcolor-diagnostics.
// Windows 10+ supports VT100 ANSI sequences in the console.
bool g_colour = false;  // extern in cpp.h — accessible from codegen.cpp

static void enableColourIfTty() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        // Enable ENABLE_VIRTUAL_TERMINAL_PROCESSING (0x0004) for ANSI
        SetConsoleMode(h, mode | 0x0004);
        g_colour = true;
    }
#else
    if (isatty(STDERR_FILENO)) g_colour = true;
#endif
}

// Return ANSI prefix for errors (bold red) or empty if colour disabled
static const char* clr_err()   { return g_colour ? "\033[1;31m" : ""; }
static const char* clr_warn()  { return g_colour ? "\033[1;35m" : ""; }  // bold magenta
static const char* clr_note()  { return g_colour ? "\033[1;36m" : ""; }  // bold cyan
static const char* clr_reset() { return g_colour ? "\033[0m"    : ""; }

// Assembler selection
enum AssemblerType {
    ASM_AUTO,     // Auto-detect available assembler
    ASM_JWASM,    // Force JWasm
    ASM_MASM      // Force MASM
};

static AssemblerType g_assembler = ASM_AUTO;

// Linker selection
enum LinkerType {
    LINK_AUTO,      // Auto-detect available linker
    LINK_LLD,       // Force LLD (LLVM linker)
    LINK_MSVC       // Force MSVC link.exe
};

static LinkerType g_linker = LINK_AUTO;

// Global safety configuration
SafetyLevel g_safety_level = SafetyLevel::Minimal;  // Default: minimal (1-3% overhead)
bool g_safety_enabled = true;                        // Safety analysis enabled by default

// P4-D: Standard level for enforcement warnings (89, 99, 11, 17, 23)
int g_std_level = 17;  // Default: C17

static void printVersion() {
    printf("RCC - Rust-inspired C Compiler v%s (build %d)\n", RCC_VERSION, RCC_BUILD_NUMBER);
    printf("C23 compliant (ISO/IEC 9899:2024) | 98 tests | SQLite 3.45.1 compatible\n");
    printf("Target: x86-64 Windows (MASM/JWasm)\n");
    printf("Features: C23/C17/C11/C99 language, ownership inference, borrow checking, auto-free\n");
}

static void printUsage() {
    fprintf(stderr, "Usage: rcc <input.c> [-o output.asm] [-c] [-link]\n");
    fprintf(stderr, "       rcc <file1.c> <file2.c> ... -link  (multi-file)\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -o <file>   Output assembly file\n");
    fprintf(stderr, "  -c          Assemble (produce .obj)\n");
    fprintf(stderr, "  -link       Assemble and link (produce .exe)\n");
    fprintf(stderr, "  -E          Preprocess only\n");
    fprintf(stderr, "  -no-cpp     Skip preprocessing\n");
    fprintf(stderr, "  -I <path>   Add include search path\n");
    fprintf(stderr, "  -D name     Define macro\n");
    fprintf(stderr, "  -D name=val Define macro with value\n");
    fprintf(stderr, "  -std=<std>  Set C standard (c89|c99|c11|c17|c18|c23); default: c17\n");
    fprintf(stderr, "              c89/c90: undefine __STDC_VERSION__\n");
    fprintf(stderr, "              c99:     __STDC_VERSION__ = 199901L\n");
    fprintf(stderr, "              c11:     __STDC_VERSION__ = 201112L\n");
    fprintf(stderr, "              c17/c18: __STDC_VERSION__ = 201710L (default)\n");
    fprintf(stderr, "              c23/c2x: __STDC_VERSION__ = 202311L\n");
    fprintf(stderr, "  -O0         Disable peephole optimizer (faster compile, larger code)\n");
    fprintf(stderr, "  -O / -O1    Enable peephole optimizer (default)\n");
    fprintf(stderr, "  -O2 / -O3   Peephole + defines __OPTIMIZE__=2\n");
    fprintf(stderr, "  -MJ <file>  Append compile_commands.json entry to <file> (clangd/IDE)\n");
    fprintf(stderr, "  -g          Emit source-location comments in assembly output (debug)\n");
    fprintf(stderr, "\nMemory Safety options:\n");
    fprintf(stderr, "  --safety=<level>   Set safety level (none|minimal|medium|full)\n");
    fprintf(stderr, "                     none:    Static analysis only, 0%% overhead\n");
    fprintf(stderr, "                     minimal: Warnings only, 1-3%% overhead (default)\n");
    fprintf(stderr, "                     medium:  Auto-free + borrow tracking, ~2%% overhead\n");
    fprintf(stderr, "                     full:    Runtime guards + instrumentation, ~15%% overhead\n");
    fprintf(stderr, "  --no-safety        Disable all safety analysis\n");
    fprintf(stderr, "\nAssembler options:\n");
    fprintf(stderr, "  --assembler=<asm>  Select assembler (auto|jwasm|masm)\n");
    fprintf(stderr, "  --asm=<asm>        Short form of --assembler\n");
    fprintf(stderr, "                     auto: Auto-detect (default)\n");
    fprintf(stderr, "                     jwasm: Use JWasm assembler (bundled)\n");
    fprintf(stderr, "                     masm: Use MASM (ml64, requires VS)\n");
    fprintf(stderr, "\nLinker options:\n");
    fprintf(stderr, "  --linker=<lnk>     Select linker (auto|lld-link|link)\n");
    fprintf(stderr, "                     auto: Auto-detect (default)\n");
    fprintf(stderr, "                     lld-link: Use LLD linker (LLVM)\n");
    fprintf(stderr, "                     link: Use MSVC linker (requires VS)\n");
    fprintf(stderr, "\nGeneral:\n");
    fprintf(stderr, "  --version   Show version information\n");
    fprintf(stderr, "  --help      Show this help message\n");
    fprintf(stderr, "\nEnvironment:\n");
    fprintf(stderr, "  RCC_ASSEMBLER  Override assembler (jwasm|masm)\n");
    fprintf(stderr, "  RCC_LINKER     Override linker (lld-link|link)\n");
    fprintf(stderr, "  RCC_SAFETY     Override safety level (none|minimal|medium|full)\n");
}

static char* readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open '%s'\n", path); return nullptr; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);
    return buf;
}

static bool file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

// Global to store assembler path
static char g_assembler_path[MAX_PATH] = "";

static const char* detect_assembler() {
    // 1. Check explicit command-line selection
    if (g_assembler == ASM_JWASM) {
        // Find jwasm path - always use full path from exe directory
#ifdef _WIN32
        char exe_path[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        if (len > 0) {
            char* last_sep = strrchr(exe_path, '\\');
            if (!last_sep) last_sep = strrchr(exe_path, '/');
            if (last_sep) {
                *last_sep = 0;
                snprintf(g_assembler_path, sizeof(g_assembler_path), "%s\\jwasm.exe", exe_path);
                if (file_exists(g_assembler_path)) {
                    return "jwasm";
                }
            }
        }
        // Fallback to PATH
        strcpy(g_assembler_path, "jwasm");
#else
        strcpy(g_assembler_path, "jwasm");
#endif
        return "jwasm";
    }
    if (g_assembler == ASM_MASM) {
        strcpy(g_assembler_path, "ml64");
        return "masm";
    }

    // 2. ASM_AUTO - auto-detect available assembler

    // Check environment variable first
    const char* env = getenv("RCC_ASSEMBLER");
    if (env) {
        if (strcmp(env, "jwasm") == 0) {
            g_assembler = ASM_JWASM;
            return detect_assembler();
        }
        if (strcmp(env, "masm") == 0) {
            g_assembler = ASM_MASM;
            return detect_assembler();
        }
        fprintf(stderr, "Warning: Unknown RCC_ASSEMBLER value '%s', auto-detecting\n", env);
    }

    // Check for jwasm.exe in RCC executable directory
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len > 0) {
        char* last_sep = strrchr(exe_path, '\\');
        if (!last_sep) last_sep = strrchr(exe_path, '/');
        if (last_sep) {
            *last_sep = 0;
            char jwasm_path[MAX_PATH];
            snprintf(jwasm_path, sizeof(jwasm_path), "%s\\jwasm.exe", exe_path);
            if (file_exists(jwasm_path)) {
                strcpy(g_assembler_path, jwasm_path);
                return "jwasm";
            }
        }
    }
#endif

    // Check for ml64 in PATH
#ifdef _WIN32
    if (system("where ml64 >nul 2>&1") == 0) {
        strcpy(g_assembler_path, "ml64");
        return "masm";
    }
#endif

    // Not found
    fprintf(stderr, "Error: No assembler found!\n\n");
    fprintf(stderr, "Solutions:\n");
    fprintf(stderr, "  1. Place jwasm.exe in RCC directory, or\n");
    fprintf(stderr, "  2. Install Visual Studio Build Tools (for ml64), or\n");
    fprintf(stderr, "  3. Download RCC bundle with JWasm included\n\n");
    return nullptr;
}

// Global to store linker path
static char g_linker_path[MAX_PATH] = "";

static const char* detect_linker() {
    // 1. Check explicit command-line selection
    if (g_linker == LINK_LLD) {
        // Find lld-link path
#ifdef _WIN32
        char exe_path[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        if (len > 0) {
            char* last_sep = strrchr(exe_path, '\\');
            if (!last_sep) last_sep = strrchr(exe_path, '/');
            if (last_sep) {
                *last_sep = 0;
                snprintf(g_linker_path, sizeof(g_linker_path), "%s\\lld-link.exe", exe_path);
                if (file_exists(g_linker_path)) {
                    return "lld-link";
                }
            }
        }
        // Fallback to PATH
        strcpy(g_linker_path, "lld-link");
#else
        strcpy(g_linker_path, "lld-link");
#endif
        return "lld-link";
    }
    if (g_linker == LINK_MSVC) {
        strcpy(g_linker_path, "link");
        return "link";
    }

    // 2. LINK_AUTO - auto-detect available linker

    // Check environment variable first
    const char* env = getenv("RCC_LINKER");
    if (env) {
        if (strcmp(env, "lld-link") == 0 || strcmp(env, "lld") == 0) {
            g_linker = LINK_LLD;
            return detect_linker();
        }
        if (strcmp(env, "link") == 0 || strcmp(env, "msvc") == 0) {
            g_linker = LINK_MSVC;
            return detect_linker();
        }
        fprintf(stderr, "Warning: Unknown RCC_LINKER value '%s', auto-detecting\n", env);
    }

    // Check for lld-link.exe in RCC executable directory
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len > 0) {
        char* last_sep = strrchr(exe_path, '\\');
        if (!last_sep) last_sep = strrchr(exe_path, '/');
        if (last_sep) {
            *last_sep = 0;
            char lld_path[MAX_PATH];
            snprintf(lld_path, sizeof(lld_path), "%s\\lld-link.exe", exe_path);
            if (file_exists(lld_path)) {
                strcpy(g_linker_path, lld_path);
                return "lld-link";
            }
        }
    }
#endif

    // Check for lld-link in PATH
#ifdef _WIN32
    if (system("where lld-link >nul 2>&1") == 0) {
        strcpy(g_linker_path, "lld-link");
        return "lld-link";
    }
#endif

    // Fallback: Check for MSVC link in PATH
#ifdef _WIN32
    if (system("where link >nul 2>&1") == 0) {
        strcpy(g_linker_path, "link");
        return "link";
    }
#endif

    // Not found
    fprintf(stderr, "Error: No linker found!\n\n");
    fprintf(stderr, "Solutions:\n");
    fprintf(stderr, "  1. Download lld-link.exe from LLVM and place in RCC directory, or\n");
    fprintf(stderr, "  2. Install Visual Studio Build Tools (for link.exe), or\n");
    fprintf(stderr, "  3. Download RCC bundle with LLD included\n\n");
    fprintf(stderr, "LLD download: https://github.com/llvm/llvm-project/releases\n");
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    // P4-B: Enable ANSI colour if stderr is a terminal
    enableColourIfTty();

    // Check for --version or --help first
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printVersion();
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage();
            return 0;
        }
    }

    std::vector<const char*> infiles;
    const char* outfile = nullptr;
    bool assemble = false;
    bool do_link = false;
    bool preprocess_only = false;
    bool no_preprocess = false;
    const char* std_mode = "c17";  // Default: C17
    int opt_level = 1;             // Default: -O1 (peephole on)
    const char* mj_file = nullptr; // -MJ: write compile_commands.json entry
    bool debug_info = false;       // -g: emit source-location comments in ASM
    Preprocessor cpp;

    // Auto-detect libc/ directory relative to executable
#ifdef _WIN32
    {
        char exe_path[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        if (len > 0) {
            char* last_sep = strrchr(exe_path, '\\');
            if (!last_sep) last_sep = strrchr(exe_path, '/');
            if (last_sep) {
                *last_sep = 0;
                std::string libc_path = std::string(exe_path) + "\\libc";
                cpp.addIncludePath(libc_path);
            }
        }
    }
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outfile = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0) {
            assemble = true;
        } else if (strcmp(argv[i], "-link") == 0) {
            do_link = true;
        } else if (strcmp(argv[i], "-E") == 0) {
            preprocess_only = true;
        } else if (strcmp(argv[i], "-no-cpp") == 0) {
            no_preprocess = true;
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            cpp.addIncludePath(argv[++i]);
        } else if (strncmp(argv[i], "-I", 2) == 0 && argv[i][2]) {
            cpp.addIncludePath(argv[i] + 2);
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            char* eq = strchr(argv[++i], '=');
            if (eq) { *eq = 0; cpp.defineMacro(argv[i], eq + 1); *eq = '='; }
            else cpp.defineMacro(argv[i]);
        } else if (strncmp(argv[i], "-D", 2) == 0 && argv[i][2]) {
            char* arg = argv[i] + 2;
            char* eq = strchr(arg, '=');
            if (eq) { std::string n(arg, eq - arg); cpp.defineMacro(n, eq + 1); }
            else cpp.defineMacro(arg);
        } else if (strcmp(argv[i], "--assembler=jwasm") == 0 || strcmp(argv[i], "--asm=jwasm") == 0) {
            g_assembler = ASM_JWASM;
        } else if (strcmp(argv[i], "--assembler=masm") == 0 || strcmp(argv[i], "--asm=masm") == 0) {
            g_assembler = ASM_MASM;
        } else if (strcmp(argv[i], "--assembler=auto") == 0 || strcmp(argv[i], "--asm=auto") == 0) {
            g_assembler = ASM_AUTO;
        } else if (strcmp(argv[i], "--linker=lld-link") == 0 || strcmp(argv[i], "--linker=lld") == 0) {
            g_linker = LINK_LLD;
        } else if (strcmp(argv[i], "--linker=link") == 0 || strcmp(argv[i], "--linker=msvc") == 0) {
            g_linker = LINK_MSVC;
        } else if (strcmp(argv[i], "--linker=auto") == 0) {
            g_linker = LINK_AUTO;
        } else if (strcmp(argv[i], "--safety=none") == 0) {
            g_safety_level = SafetyLevel::None;
            g_safety_enabled = false;
        } else if (strcmp(argv[i], "--safety=minimal") == 0) {
            g_safety_level = SafetyLevel::Minimal;
        } else if (strcmp(argv[i], "--safety=medium") == 0) {
            g_safety_level = SafetyLevel::Medium;
        } else if (strcmp(argv[i], "--safety=full") == 0) {
            g_safety_level = SafetyLevel::Full;
        } else if (strcmp(argv[i], "--no-safety") == 0) {
            g_safety_enabled = false;
        } else if (strncmp(argv[i], "-std=", 5) == 0) {
            std_mode = argv[i] + 5;
        } else if (strcmp(argv[i], "-O0") == 0) {
            opt_level = 0;
        } else if (strcmp(argv[i], "-O1") == 0 || strcmp(argv[i], "-O") == 0) {
            opt_level = 1;
        } else if (strcmp(argv[i], "-O2") == 0 || strcmp(argv[i], "-O3") == 0) {
            opt_level = 2;
        } else if (strcmp(argv[i], "-MJ") == 0 && i + 1 < argc) {
            mj_file = argv[++i];
        } else if (strncmp(argv[i], "-MJ", 3) == 0 && argv[i][3]) {
            mj_file = argv[i] + 3;
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "-g1") == 0 ||
                   strcmp(argv[i], "-g2") == 0 || strcmp(argv[i], "-g3") == 0) {
            debug_info = true;
        } else if (strcmp(argv[i], "-fcolor-diagnostics") == 0 ||
                   strcmp(argv[i], "-fcolour-diagnostics") == 0) {
            g_colour = true;
        } else if (strcmp(argv[i], "-fno-color-diagnostics") == 0 ||
                   strcmp(argv[i], "-fno-colour-diagnostics") == 0) {
            g_colour = false;
        } else if (argv[i][0] != '-') {
            infiles.push_back(argv[i]);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    // Apply C standard version to __STDC_VERSION__ macro and g_std_level
    {
        const char* stdc_ver = nullptr;
        if (strcmp(std_mode, "c89") == 0 || strcmp(std_mode, "c90") == 0) {
            cpp.undefineMacro("__STDC_VERSION__");  // C89 does not define this macro
            g_std_level = 89;
        } else if (strcmp(std_mode, "c99") == 0) {
            stdc_ver = "199901L";
            g_std_level = 99;
        } else if (strcmp(std_mode, "c11") == 0) {
            stdc_ver = "201112L";
            g_std_level = 11;
        } else if (strcmp(std_mode, "c17") == 0 || strcmp(std_mode, "c18") == 0) {
            stdc_ver = "201710L";  // already the default; explicit for clarity
            g_std_level = 17;
        } else if (strcmp(std_mode, "c23") == 0 || strcmp(std_mode, "c2x") == 0) {
            stdc_ver = "202311L";
            g_std_level = 23;
        } else {
            fprintf(stderr, "Warning: unknown -std= value '%s'; defaulting to c17\n", std_mode);
            stdc_ver = "201710L";
            g_std_level = 17;
        }
        if (stdc_ver) cpp.defineMacro("__STDC_VERSION__", stdc_ver);
    }

    // Apply optimization macros (__OPTIMIZE__ for compatibility with autoconf checks)
    if (opt_level >= 2)
        cpp.defineMacro("__OPTIMIZE__", "2");
    else if (opt_level >= 1)
        cpp.defineMacro("__OPTIMIZE__", "1");

    if (infiles.empty()) {
        fprintf(stderr, "No input files\n");
        return 1;
    }

    const char* infile = infiles[0];
    bool link = do_link;

    // Default output: input.asm
    char default_out[256];
    if (!outfile) {
        strncpy(default_out, infile, sizeof(default_out) - 5);
        char* dot = strrchr(default_out, '.');
        if (dot) strcpy(dot, ".asm");
        else strcat(default_out, ".asm");
        outfile = default_out;
    }

    char* src = readFile(infile);
    if (!src) return 1;

    // Preprocess
    if (!no_preprocess) {
        std::string preprocessed = cpp.preprocess(src, infile);
        free(src);
        src = _strdup(preprocessed.c_str());
    }
    if (preprocess_only) {
        printf("%s", src);
        free(src);
        return 0;
    }

    // Reset attribute registry for each compilation unit
    AttrRegistry::clear();

    // Lex + Parse
    Lexer lex(src, infile);
    Parser parser(lex);
    ASTPtr ast = parser.parse();

    if (!ast) {
        fprintf(stderr, "Parse failed\n");
        free(src);
        return 1;
    }

    // ═══════════════════════════════════════════════════════════════════════════════
    // MEMORY SAFETY ANALYSIS (Version 4.0)
    // ═══════════════════════════════════════════════════════════════════════════════

    SafetyContext safety_ctx;
    safety_ctx.setSafetyLevel(g_safety_level);

    if (g_safety_enabled) {
        printf("Running memory safety analysis (level: ");
        switch (g_safety_level) {
            case SafetyLevel::None: printf("none"); break;
            case SafetyLevel::Minimal: printf("minimal"); break;
            case SafetyLevel::Medium: printf("medium"); break;
            case SafetyLevel::Full: printf("full"); break;
        }
        printf(")...\n");

        // Pass 1: Type Enrichment - Classify pointers (Owner/Borrowed/Raw/Temp)
        TypeEnrichment type_enrichment(safety_ctx);
        type_enrichment.analyze(ast.get());

        // Pass 2: Ownership Analysis - Track ownership transfers, detect use-after-free
        OwnershipAnalysis ownership(safety_ctx);
        ownership.analyze(ast.get());

        // Pass 3: Borrow Checker - Enforce borrow rules, detect aliasing violations
        BorrowChecker borrow_checker(safety_ctx);
        borrow_checker.analyze(ast.get());

        // Report diagnostics
        if (safety_ctx.getErrorCount() > 0 || safety_ctx.getWarningCount() > 0) {
            fprintf(stderr, "\n%sMemory Safety Analysis Results:%s\n",
                    clr_note(), clr_reset());
            fprintf(stderr, "────────────────────────────────\n");
            safety_ctx.reportDiagnostics(stderr);

            if (safety_ctx.getErrorCount() > 0) {
                fprintf(stderr, "\n%serror:%s Compilation aborted due to safety errors.\n",
                        clr_err(), clr_reset());
                free(src);
                return 1;
            }
        } else {
            printf("✓ No memory safety issues detected\n");
        }

        // Pass 4: Automatic Memory Management (v4.1) - Insert auto-free cleanup
        if (safety_ctx.shouldInsertAutoFree()) {
            printf("Inserting automatic memory cleanup...\n");
            AutoFreePass auto_free(safety_ctx);
            auto_free.transform(ast.get());
            printf("✓ Auto-free transformation complete\n");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════════
    // CODE GENERATION
    // ═══════════════════════════════════════════════════════════════════════════════

    // Generate x86-64 MASM
    FILE* out = fopen(outfile, "wb"); // binary: avoid CRLF translation on Windows
    if (!out) {
        fprintf(stderr, "Cannot open output '%s'\n", outfile);
        free(src);
        return 1;
    }

    CodeGen codegen(out, infile);
    if (debug_info) codegen.setDebugInfo(true);
    codegen.generate(ast.get());
    fclose(out);

    // Phase E: Peephole optimizer — read back ASM, optimize, rewrite (disabled at -O0)
    // Use binary mode for both read and write to avoid CRLF double-translation on Windows:
    // text-mode ftell returns physical size (with \r bytes) but text-mode fread strips \r,
    // causing asm_text to have trailing \0 bytes that corrupt the output when written back.
    if (opt_level >= 1) {
        FILE* pf = fopen(outfile, "rb");
        if (pf) {
            fseek(pf, 0, SEEK_END);
            long psz = ftell(pf);
            fseek(pf, 0, SEEK_SET);
            std::string asm_text(psz, '\0');
            size_t n_read = fread(&asm_text[0], 1, psz, pf);
            asm_text.resize(n_read); // trim to actual bytes read (no trailing NULs)
            fclose(pf);

            int n_opts = peepholeOptimize(asm_text, opt_level);
            if (opt_level >= 2) regAllocPass(asm_text);  // P3.3: store-then-reload elimination
            if (n_opts > 0) {
                FILE* pw = fopen(outfile, "wb");
                if (pw) {
                    fwrite(asm_text.data(), 1, asm_text.size(), pw);
                    fclose(pw);
                }
            }
            (void)n_opts; // suppress unused warning if not printing
        }
    }

    printf("Generated: %s\n", outfile);

    // -MJ: append a compile_commands.json entry (GCC/Clang compatible)
    if (mj_file) {
        FILE* mj = fopen(mj_file, "a");
        if (mj) {
            // Build the command string from argv
            std::string cmd;
            for (int i = 0; i < argc; i++) {
                if (i > 0) cmd += ' ';
                cmd += argv[i];
            }
            // Get current directory
            char cwd[512] = ".";
#ifdef _WIN32
            GetCurrentDirectoryA(sizeof(cwd), cwd);
            // Convert backslashes to forward slashes for JSON
            for (char* p = cwd; *p; p++) if (*p == '\\') *p = '/';
#endif
            fprintf(mj, "{ \"directory\": \"%s\", \"command\": \"%s\", \"file\": \"%s\" }\n",
                    cwd, cmd.c_str(), infile);
            fclose(mj);
        }
    }

    // Handle additional input files for multi-file compilation
    std::vector<std::string> all_obj_files;
    if (assemble || do_link) {
        // Detect assembler
        const char* assembler = detect_assembler();
        if (!assembler) {
            free(src);
            return 1;
        }

        // Assemble first file
        char cmd[2048];
        char objfile[512];
        strncpy(objfile, outfile, sizeof(objfile) - 5);
        char* dot = strrchr(objfile, '.');
        if (dot) strcpy(dot, ".obj");
        else strcat(objfile, ".obj");

        if (strcmp(assembler, "jwasm") == 0) {
            snprintf(cmd, sizeof(cmd), "%s -win64 -c -Fo\"%s\" \"%s\"", g_assembler_path, objfile, outfile);
        } else {
            snprintf(cmd, sizeof(cmd), "%s /c /Fo\"%s\" \"%s\"", g_assembler_path, objfile, outfile);
        }
        printf("Assembling (%s): %s\n", assembler, cmd);
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "Assembly failed for %s\n", outfile);
            free(src);
            return 1;
        }
        all_obj_files.push_back(objfile);

        // Compile and assemble additional files
        for (size_t fi = 1; fi < infiles.size(); fi++) {
            char* extra_src = readFile(infiles[fi]);
            if (!extra_src) return 1;

            // Generate .asm name
            char extra_asm[512], extra_obj[512];
            strncpy(extra_asm, infiles[fi], sizeof(extra_asm) - 5);
            dot = strrchr(extra_asm, '.');
            if (dot) strcpy(dot, ".asm");
            else strcat(extra_asm, ".asm");

            strncpy(extra_obj, infiles[fi], sizeof(extra_obj) - 5);
            dot = strrchr(extra_obj, '.');
            if (dot) strcpy(dot, ".obj");
            else strcat(extra_obj, ".obj");

            if (!no_preprocess) {
                std::string pp = cpp.preprocess(extra_src, infiles[fi]);
                free(extra_src);
                extra_src = _strdup(pp.c_str());
            }

            Lexer extra_lex(extra_src, infiles[fi]);
            Parser extra_parser(extra_lex);
            ASTPtr extra_ast = extra_parser.parse();
            if (!extra_ast) {
                fprintf(stderr, "Parse failed for %s\n", infiles[fi]);
                free(extra_src);
                return 1;
            }

            FILE* extra_out = fopen(extra_asm, "w");
            if (!extra_out) { fprintf(stderr, "Cannot open %s\n", extra_asm); free(extra_src); return 1; }
            CodeGen extra_codegen(extra_out);
            extra_codegen.generate(extra_ast.get());
            fclose(extra_out);
            printf("Generated: %s\n", extra_asm);

            if (strcmp(assembler, "jwasm") == 0) {
                snprintf(cmd, sizeof(cmd), "%s -win64 -c -Fo\"%s\" \"%s\"", g_assembler_path, extra_obj, extra_asm);
            } else {
                snprintf(cmd, sizeof(cmd), "%s /c /Fo\"%s\" \"%s\"", g_assembler_path, extra_obj, extra_asm);
            }
            printf("Assembling (%s): %s\n", assembler, cmd);
            rc = system(cmd);
            if (rc != 0) { fprintf(stderr, "Assembly failed for %s\n", extra_asm); free(extra_src); return 1; }
            all_obj_files.push_back(extra_obj);
            free(extra_src);
        }

        if (do_link) {
            // Detect linker
            const char* linker = detect_linker();
            if (!linker) {
                free(src);
                return 1;
            }

            // Build object file list
            std::string obj_list;
            for (auto& o : all_obj_files) {
                obj_list += "\"" + o + "\" ";
            }

            char exefile[512];
            strncpy(exefile, infiles[0], sizeof(exefile) - 5);
            dot = strrchr(exefile, '.');
            if (dot) strcpy(dot, ".exe");
            else strcat(exefile, ".exe");

            // Use detected linker (lld-link or link)
            snprintf(cmd, sizeof(cmd),
                "%s /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:\"%s\" %s"
                "msvcrt.lib ucrt.lib legacy_stdio_definitions.lib kernel32.lib",
                g_linker_path, exefile, obj_list.c_str());
            printf("Linking (%s): %s\n", linker, cmd);
            rc = system(cmd);
            if (rc != 0) { fprintf(stderr, "Link failed\n"); free(src); return 1; }
            printf("Executable: %s\n", exefile);
        }
    }

    free(src);
    return 0;
}
