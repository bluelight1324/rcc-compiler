// hello_standalone.c - Freestanding program, no CRT needed
// Only links against kernel32.lib (Windows API, always available)
//
// Build:
//   rcc.exe hello_standalone.c -o hello_standalone.asm --safety=none
//   jwasm.exe -win64 -Fo hello_standalone.obj hello_standalone.asm
//   lld-link.exe /OUT:hello_standalone.exe /SUBSYSTEM:CONSOLE /ENTRY:main
//     hello_standalone.obj /NODEFAULTLIB kernel32.lib
//     /LIBPATH:"<Windows SDK um/x64 path>"

// Minimal Windows types (no headers needed)
typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;

#define STD_OUTPUT_HANDLE ((DWORD)-11)

extern HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
extern BOOL __stdcall WriteFile(HANDLE hFile, const void* lpBuffer,
                                DWORD nNumberOfBytesToWrite,
                                DWORD* lpNumberOfBytesWritten,
                                void* lpOverlapped);
extern void __stdcall ExitProcess(unsigned int uExitCode);

// String length (no libc available)
static int slen(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

int main(void) {
    const char* msg = "Hello from standalone RCC! No CRT needed.\n";
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteFile(out, msg, (DWORD)slen(msg), &written, 0);
    ExitProcess(0);
    return 0;
}
