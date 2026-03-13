/* RCC stub <windows.h> — minimal Win32 type/API declarations for compilation.
 * Provides enough for sqlite3.c and similar Windows-targeted C code to parse.
 * Actual symbol resolution happens at link time against the real Windows libraries.
 */
#pragma once
#ifndef _WINDOWS_H
#define _WINDOWS_H

/* ── Basic integer types ─────────────────────────────────────────────────── */
typedef unsigned char      BYTE;
typedef unsigned char      UCHAR;
typedef signed char        SCHAR;
typedef unsigned short     WORD;
typedef unsigned short     USHORT;
typedef unsigned int       DWORD;   /* 4 bytes on Windows x64 (LLP64 model) */
typedef unsigned int       ULONG;   /* 4 bytes on Windows x64 */
typedef int                LONG;    /* 4 bytes on Windows x64 */
typedef int                INT;
typedef unsigned int       UINT;
typedef int                BOOL;
typedef unsigned short     WCHAR;
typedef long long          LONGLONG;
typedef unsigned long long ULONGLONG;
typedef unsigned long long ULONG_PTR;   /* pointer-sized unsigned — 8 bytes on x64 */
typedef long long          LONG_PTR;    /* pointer-sized signed  — 8 bytes on x64 */
typedef unsigned long long SIZE_T;      /* size_t equivalent      — 8 bytes on x64 */
typedef unsigned long long DWORD64;
typedef unsigned long long ULONG64;
typedef unsigned long long DWORD_PTR;  /* pointer-sized DWORD    — 8 bytes on x64 */

/* ── Pointer types ───────────────────────────────────────────────────────── */
typedef void*              PVOID;
typedef void*              LPVOID;
typedef const void*        LPCVOID;
typedef char*              LPSTR;
typedef const char*        LPCSTR;
typedef WCHAR*             LPWSTR;
typedef const WCHAR*       LPCWSTR;
typedef DWORD*             LPDWORD;
typedef LONG*              LPLONG;
typedef LONG*              PLONG;
typedef BYTE*              LPBYTE;
typedef BOOL*              LPBOOL;
typedef void*              HANDLE;
typedef HANDLE             HMODULE;
typedef HANDLE             HFILE;
typedef HANDLE             HLOCAL;
typedef HANDLE*            LPHANDLE;
typedef int (*FARPROC)(void);

/* ── Calling convention (already no-op in RCC) ───────────────────────────── */
#ifndef WINAPI
#define WINAPI
#endif
#define CALLBACK
#define PASCAL
#define APIENTRY WINAPI
#define VOID void

/* ── Boolean / sentinel values ───────────────────────────────────────────── */
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)(-1))
#define NULL_HANDLE          ((HANDLE)0)

/* ── SECURITY_ATTRIBUTES ─────────────────────────────────────────────────── */
typedef struct _SECURITY_ATTRIBUTES {
    DWORD  nLength;
    LPVOID lpSecurityDescriptor;
    BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

/* ── OVERLAPPED ──────────────────────────────────────────────────────────── */
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    DWORD     Offset;
    DWORD     OffsetHigh;
    HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;

/* ── CRITICAL_SECTION ────────────────────────────────────────────────────── */
typedef struct _RTL_CRITICAL_SECTION {
    PVOID  DebugInfo;
    LONG   LockCount;
    LONG   RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

/* ── LARGE_INTEGER ───────────────────────────────────────────────────────── */
typedef struct _LARGE_INTEGER {
    DWORD LowPart;
    LONG  HighPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _ULARGE_INTEGER {
    DWORD LowPart;
    DWORD HighPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

/* ── FILETIME / SYSTEMTIME ───────────────────────────────────────────────── */
typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay;
    WORD wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

/* ── WIN32_FILE_ATTRIBUTE_DATA ───────────────────────────────────────────── */
typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;

typedef int GET_FILEEX_INFO_LEVELS;
#define GetFileExInfoStandard 0

/* ── WIN32_FIND_DATA ─────────────────────────────────────────────────────── */
#define MAX_PATH 260

typedef struct _WIN32_FIND_DATAA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD    nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    char     cFileName[MAX_PATH];
    char     cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef struct _WIN32_FIND_DATAW {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD    nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    WCHAR    cFileName[MAX_PATH];
    WCHAR    cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

/* ── SYSTEM_INFO ─────────────────────────────────────────────────────────── */
typedef struct _SYSTEM_INFO {
    DWORD  dwOemId;
    DWORD  dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD  dwNumberOfProcessors;
    DWORD  dwProcessorType;
    DWORD  dwAllocationGranularity;
    WORD   wProcessorLevel;
    WORD   wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

/* ── Function pointer type for sqlite3 aSyscall table ───────────────────── */
typedef void (*SYSCALL)(void);

/* ── Constants ───────────────────────────────────────────────────────────── */
#define GENERIC_READ              0x80000000UL
#define GENERIC_WRITE             0x40000000UL
#define GENERIC_EXECUTE           0x20000000UL
#define GENERIC_ALL               0x10000000UL
#define FILE_SHARE_READ           0x00000001UL
#define FILE_SHARE_WRITE          0x00000002UL
#define FILE_SHARE_DELETE         0x00000004UL
#define CREATE_NEW                1
#define CREATE_ALWAYS             2
#define OPEN_EXISTING             3
#define OPEN_ALWAYS               4
#define TRUNCATE_EXISTING         5
#define FILE_BEGIN                0
#define FILE_CURRENT              1
#define FILE_END                  2
#define INVALID_SET_FILE_POINTER  ((DWORD)0xFFFFFFFF)
#define INVALID_FILE_SIZE         ((DWORD)0xFFFFFFFF)
#define FILE_ATTRIBUTE_NORMAL     0x00000080
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010
#define FILE_ATTRIBUTE_READONLY   0x00000001
#define FILE_ATTRIBUTE_HIDDEN     0x00000002
#define FILE_ATTRIBUTE_SYSTEM     0x00000004
#define FILE_FLAG_RANDOM_ACCESS   0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_NO_BUFFERING    0x20000000
#define FILE_FLAG_OVERLAPPED      0x40000000
#define FILE_FLAG_DELETE_ON_CLOSE 0x04000000
#define FILE_FLAG_WRITE_THROUGH   0x80000000
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_WRITE_THROUGH    0x00000008
#define LOCKFILE_EXCLUSIVE_LOCK   0x00000002
#define LOCKFILE_FAIL_IMMEDIATELY 0x00000001
#define PAGE_READONLY             0x02
#define PAGE_READWRITE            0x04
#define PAGE_WRITECOPY            0x08
#define SEC_COMMIT                0x08000000
#define FILE_MAP_READ             0x0004
#define FILE_MAP_WRITE            0x0002
#define FILE_MAP_ALL_ACCESS       0x000f001f
#define INFINITE                  0xFFFFFFFF
#define WAIT_OBJECT_0             0
#define WAIT_TIMEOUT              258
#define WAIT_FAILED               0xFFFFFFFF
#define HEAP_ZERO_MEMORY          0x00000008
#define MEM_COMMIT                0x00001000
#define MEM_RESERVE               0x00002000
#define MEM_RELEASE               0x00008000
#define PAGE_NOACCESS             0x01
#define CP_ACP                    0
#define CP_UTF8                   65001
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200

/* Error codes */
#define ERROR_SUCCESS             0
#define ERROR_ACCESS_DENIED       5
#define ERROR_INVALID_HANDLE      6
#define ERROR_NOT_ENOUGH_MEMORY   8
#define ERROR_INVALID_DRIVE       15
#define ERROR_NO_MORE_FILES       18
#define ERROR_SHARING_VIOLATION   32
#define ERROR_LOCK_VIOLATION      33
#define ERROR_HANDLE_DISK_FULL    39
#define ERROR_FILE_NOT_FOUND      2
#define ERROR_PATH_NOT_FOUND      3
#define ERROR_DISK_FULL           112
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_ALREADY_EXISTS      183
#define ERROR_LOCK_FAILED         167
#define ERROR_IO_PENDING          997
#define HRESULT_FROM_WIN32(x)     ((long)(x))

/* ── Critical section functions ─────────────────────────────────────────── */
void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags);
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);
void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

/* ── Thread / synchronization functions ─────────────────────────────────── */
DWORD  GetCurrentThreadId(void);
HANDLE GetCurrentThread(void);
DWORD  GetCurrentProcessId(void);
HANDLE GetCurrentProcess(void);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                    LPVOID lpStartAddress, LPVOID lpParameter,
                    DWORD dwCreationFlags, LPDWORD lpThreadId);
BOOL   CloseHandle(HANDLE hObject);
DWORD  WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD  WaitForMultipleObjects(DWORD nCount, HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
VOID   Sleep(DWORD dwMilliseconds);
DWORD  SleepEx(DWORD dwMilliseconds, BOOL bAlertable);
BOOL   TerminateThread(HANDLE hThread, DWORD dwExitCode);
BOOL   GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);

/* ── Interlocked functions ───────────────────────────────────────────────── */
LONG InterlockedCompareExchange(LONG* Destination, LONG Exchange, LONG Comparand);
LONG InterlockedExchange(LONG* Target, LONG Value);
LONG InterlockedIncrement(LONG* Addend);
LONG InterlockedDecrement(LONG* Addend);
LONG InterlockedExchangeAdd(LONG* Addend, LONG Value);

/* ── Memory barrier ─────────────────────────────────────────────────────── */
#define _ReadWriteBarrier() ((void)0)
#define MemoryBarrier()     ((void)0)

/* ── Mutex / event objects ───────────────────────────────────────────────── */
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName);
HANDLE OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
HANDLE OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
BOOL   ReleaseMutex(HANDLE hMutex);
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
BOOL   SetEvent(HANDLE hEvent);
BOOL   ResetEvent(HANDLE hEvent);

/* ── File I/O functions ──────────────────────────────────────────────────── */
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL   ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL   WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                 LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
BOOL   FlushFileBuffers(HANDLE hFile);
BOOL   SetEndOfFile(HANDLE hFile);
DWORD  SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod);
BOOL   SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);
BOOL   GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);
DWORD  GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL   GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
BOOL   GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
DWORD  GetFileAttributesA(LPCSTR lpFileName);
DWORD  GetFileAttributesW(LPCWSTR lpFileName);
BOOL   SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes);
BOOL   SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes);
BOOL   DeleteFileA(LPCSTR lpFileName);
BOOL   DeleteFileW(LPCWSTR lpFileName);
BOOL   MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
BOOL   MoveFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName);
BOOL   MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);
BOOL   MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags);
BOOL   GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
BOOL   CancelIo(HANDLE hFile);
BOOL   LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh);
BOOL   LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped);
BOOL   UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh);
BOOL   UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped);

/* ── File mapping / memory-mapped files ─────────────────────────────────── */
HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                          DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow,
                          LPCSTR lpName);
HANDLE CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                          DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow,
                          LPCWSTR lpName);
LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                     DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
LPVOID MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                       DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap, LPVOID lpBaseAddress);
BOOL   UnmapViewOfFile(LPCVOID lpBaseAddress);
BOOL   FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush);

/* ── Directory / path functions ─────────────────────────────────────────── */
BOOL  CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL  CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL  RemoveDirectoryA(LPCSTR lpPathName);
BOOL  RemoveDirectoryW(LPCWSTR lpPathName);
DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer);
DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer);
DWORD GetTempPathExA(DWORD nBufferLength, LPSTR lpBuffer);
DWORD GetTempPathExW(DWORD nBufferLength, LPWSTR lpBuffer);
UINT  GetTempFileNameA(LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName);
UINT  GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPWSTR lpTempFileName);
DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);
DWORD GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart);
HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
BOOL   FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL   FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);
BOOL   FindClose(HANDLE hFindFile);

/* ── String / encoding functions ─────────────────────────────────────────── */
int   MultiByteToWideChar(DWORD CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
int   WideCharToMultiByte(DWORD CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);
DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, void* Arguments);
DWORD FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, void* Arguments);

/* ── System information ──────────────────────────────────────────────────── */
void  GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
void  GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
void  GetLocalTime(LPSYSTEMTIME lpSystemTime);
void  GetSystemTime(LPSYSTEMTIME lpSystemTime);
DWORD GetTickCount(void);
BOOL  QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
BOOL  QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);

/* ── Error handling ──────────────────────────────────────────────────────── */
DWORD GetLastError(void);
void  SetLastError(DWORD dwErrCode);

/* ── Heap / virtual memory ───────────────────────────────────────────────── */
HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
BOOL   HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);
LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
BOOL   VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
LPVOID LocalAlloc(UINT uFlags, SIZE_T uBytes);
HANDLE LocalFree(HANDLE hMem);

/* ── Module / DLL functions ──────────────────────────────────────────────── */
HMODULE LoadLibraryA(LPCSTR lpLibFileName);
HMODULE LoadLibraryW(LPCWSTR lpLibFileName);
BOOL    FreeLibrary(HMODULE hLibModule);
LPVOID  GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE GetModuleHandleA(LPCSTR lpModuleName);
HMODULE GetModuleHandleW(LPCWSTR lpModuleName);

/* ── Console / output ────────────────────────────────────────────────────── */
BOOL  WriteConsoleA(HANDLE hConsoleOutput, LPCVOID lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved);
BOOL  WriteConsoleW(HANDLE hConsoleOutput, LPCVOID lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved);

/* ── Process functions ───────────────────────────────────────────────────── */
BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);
void ExitProcess(UINT uExitCode);

/* ── OSVERSIONINFO (legacy OS detection) ────────────────────────────────── */
#define VER_PLATFORM_WIN32_NT 2
typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    char  szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;

/* ── MSVC version macro (if not defined, set to VS2022 equivalent) ────────── */
#ifndef MSVC_VERSION
#define MSVC_VERSION 1939
#endif

#endif /* _WINDOWS_H */
