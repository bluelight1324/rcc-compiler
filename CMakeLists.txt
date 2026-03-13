cmake_minimum_required(VERSION 3.16)
project(RCC VERSION 6.0.0 LANGUAGES C CXX)

# RCC - Rust-inspired C Compiler
# A C compiler with Rust-style memory safety analysis AND automatic memory management

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build number — read from build_number.txt, auto-incremented on each build
set(BUILD_NUMBER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/build_number.txt")
if(EXISTS "${BUILD_NUMBER_FILE}")
    file(READ "${BUILD_NUMBER_FILE}" RCC_BUILD_NUMBER)
    string(STRIP "${RCC_BUILD_NUMBER}" RCC_BUILD_NUMBER)
else()
    set(RCC_BUILD_NUMBER 1)
endif()
message(STATUS "Build number: ${RCC_BUILD_NUMBER}")

# Build options
option(RCC_ENABLE_SAFETY "Enable Rust-style memory safety analysis" ON)
option(RCC_ENABLE_RUNTIME_CHECKS "Enable runtime safety checks" ON)
option(RCC_BUILD_TESTS "Build test suite" ON)

# Main compiler executable
add_executable(rcc
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/cpp.cpp
    src/synout0.cpp
    src/peephole.cpp
    src/regalloc.cpp
)

target_include_directories(rcc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Pass build number to compiler
target_compile_definitions(rcc PRIVATE RCC_BUILD_NUMBER=${RCC_BUILD_NUMBER})

# Copy libc/ headers next to the executable after every build so rcc.exe
# can find them at runtime via its auto-detect path (exe_dir/libc/).
add_custom_command(TARGET rcc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/libc"
        "$<TARGET_FILE_DIR:rcc>/libc"
    COMMENT "Syncing RCC libc headers to build output"
)

# Auto-increment build number after successful build
add_custom_command(TARGET rcc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/increment_build.cmake"
    COMMENT "Incrementing build number (current: ${RCC_BUILD_NUMBER})"
)

# Safety analysis module (Version 4.0)
if(RCC_ENABLE_SAFETY)
    target_compile_definitions(rcc PRIVATE RCC_SAFETY_ENABLED)
    target_sources(rcc PRIVATE
        src/type_enrichment.cpp
        src/ownership.cpp
        src/borrow_checker.cpp
        src/auto_memory.cpp
        src/auto_free_pass.cpp
    )
    message(STATUS "Memory safety analysis: ENABLED")
    message(STATUS "  • v4.1 Automatic memory management: INCLUDED")
endif()

# Runtime safety library (future)
if(RCC_ENABLE_RUNTIME_CHECKS)
    # add_library(rcc_runtime STATIC
    #     runtime/rcc_safety_rt.c
    # )
endif()

# C11 <threads.h> Win32 runtime library
# Users who call thrd_*/mtx_*/cnd_*/tss_*/call_once must link rcc_threads.lib.
# See docs/THREADS_USAGE.md for the full compile + link workflow.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/threads_win32.c")
    add_library(rcc_threads STATIC runtime/threads_win32.c)
    set_source_files_properties(runtime/threads_win32.c PROPERTIES LANGUAGE C)
    target_include_directories(rcc_threads PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}   # allows #include "libc/threads.h"
    )
    if(MSVC)
        target_compile_options(rcc_threads PRIVATE /W3 /O2)
    endif()
    install(TARGETS rcc_threads ARCHIVE DESTINATION lib)
    message(STATUS "  • rcc_threads (C11 <threads.h> Win32 runtime): INCLUDED")
endif()

# Tests
if(RCC_BUILD_TESTS)
    enable_testing()
    # Future: add test targets
endif()

# Installation
install(TARGETS rcc DESTINATION bin)
install(DIRECTORY libc/ DESTINATION include/rcc)

# Print configuration summary
message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "RCC - Rust-inspired C Compiler v${PROJECT_VERSION} (build ${RCC_BUILD_NUMBER})")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "Features:")
message(STATUS "  • Automatic ownership inference")
message(STATUS "  • Automatic lifetime analysis")
message(STATUS "  • Borrow checking from usage patterns")
message(STATUS "  • Scope-based automatic free (NEW in v4.1)")
message(STATUS "  • Runtime memory guards (NEW in v4.1)")
message(STATUS "  • Configurable safety levels (--safety=none/minimal/medium/full)")
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  Safety analysis: ${RCC_ENABLE_SAFETY}")
message(STATUS "  Runtime checks:  ${RCC_ENABLE_RUNTIME_CHECKS}")
message(STATUS "  Build tests:     ${RCC_BUILD_TESTS}")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "")
