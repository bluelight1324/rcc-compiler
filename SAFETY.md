# RCC Memory Safety System

RCC brings Rust-inspired memory safety to standard C code. The safety system runs
entirely at compile time with zero runtime overhead.

## Overview

RCC's safety system has three components that work together:

1. **Ownership Inference** - determines which variable "owns" each heap allocation
2. **Borrow Checking** - ensures references don't outlive the data they point to
3. **Auto-Free** - automatically inserts `free()` calls when owned pointers go out of scope

These are compile-time analyses. They do not add runtime checks, garbage collection,
or reference counting. The generated assembly is identical to hand-written C with
manual memory management.

## How It Works

### Ownership Inference

When you call `malloc()`, `calloc()`, or `realloc()`, RCC tracks which local
variable receives the pointer. That variable becomes the **owner**.

```c
void example(void) {
    int* p = malloc(sizeof(int) * 10);  // p owns this allocation
    *p = 42;
    // RCC auto-inserts: free(p);
}   // p goes out of scope, owned memory is freed
```

Ownership can be **transferred** by assigning to another variable or returning
from a function:

```c
int* create(void) {
    int* p = malloc(sizeof(int));
    *p = 7;
    return p;  // ownership transferred to caller
}

void caller(void) {
    int* x = create();  // x now owns the allocation
    printf("%d\n", *x);
    // RCC auto-inserts: free(x);
}
```

### Borrow Checking

RCC tracks when multiple pointers reference the same data and flags potential
aliasing violations:

```c
void dangerous(void) {
    int* a = malloc(sizeof(int));
    int* b = a;     // b borrows from a
    free(a);        // a is freed
    *b = 10;        // WARNING: use-after-free - b borrows freed memory
}
```

The borrow checker detects:
- **Use-after-free** - accessing memory through a pointer after it has been freed
- **Double-free** - freeing the same allocation twice
- **Dangling returns** - returning a pointer to a local variable
- **Simultaneous mutable aliases** - two pointers that could write to the same memory

### Auto-Free

For owned pointers, RCC automatically inserts `free()` calls at every scope exit
point (end of block, return statements, break/continue):

```c
void process(int flag) {
    char* buf = malloc(256);
    if (flag) {
        // RCC inserts: free(buf);
        return;  // early return - buf is freed
    }
    use(buf);
    // RCC inserts: free(buf);
}   // normal exit - buf is freed
```

This eliminates the most common source of memory leaks in C code.

## Comparison with Rust

| Feature | Rust | RCC |
|---------|------|-----|
| Ownership tracking | Enforced (compile error) | Inferred (warnings) |
| Borrow checking | Strict (compile error) | Advisory (warnings) |
| Auto-free | Drop trait | Scope-based free insertion |
| Lifetime annotations | Required (`'a`) | Not needed |
| Runtime cost | Zero | Zero |
| Existing code | Must be rewritten | Works with standard C |

Key difference: Rust **rejects** code that violates safety rules. RCC **warns**
about potential issues but still compiles the code. This lets you incrementally
adopt safety checking in existing C codebases.

## Enabling and Disabling

Safety analysis is **on by default**. To disable it:

```cmd
rcc.exe myfile.c -o myfile.asm --safety=none
```

Common reasons to disable:
- Large codebases with many false positives (e.g., SQLite)
- Code that uses complex pointer patterns the analyzer doesn't understand
- Build scripts where compilation speed matters more than safety warnings

## Warning Categories

| Code | Category | Description |
|------|----------|-------------|
| W1 | use-after-free | Accessing memory after it has been freed |
| W2 | double-free | Freeing an allocation that was already freed |
| W3 | dangling-return | Returning a pointer to a local/stack variable |
| W4 | alias-violation | Multiple mutable pointers to the same data |
| W5 | leak | Owned pointer goes out of scope without being freed |

## Limitations

The safety system is conservative - it may produce false positives for:

- Pointer arithmetic and array indexing patterns
- Callbacks and function pointer indirection
- Complex data structures (linked lists, trees, graphs)
- Union-based type punning
- Global/static pointer storage

When false positives are excessive, use `--safety=none` for that compilation unit.

## Implementation Details

The safety analysis is implemented in four source files:

| File | Purpose |
|------|---------|
| `src/ownership.cpp` | Ownership inference - tracks allocations and transfers |
| `src/borrow_checker.cpp` | Borrow checking - detects aliasing and lifetime issues |
| `src/auto_free_pass.cpp` | Auto-free insertion - adds free() at scope exits |
| `src/auto_memory.cpp` | Memory tracking infrastructure |

The analysis runs after parsing (on the AST) and before code generation, so it
has full type information available but does not affect the generated code beyond
inserting free calls.
