#ifndef _RCC_STRING_H
#define _RCC_STRING_H
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strchr(const char* str, int c);
char* strrchr(const char* str, int c);
char* strstr(const char* haystack, const char* needle);
char* strdup(const char* str);
char* strndup(const char* str, size_t n);
char* strtok(char* str, const char* delimiters);
size_t strspn(const char* str, const char* accept);
size_t strcspn(const char* str, const char* reject);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
void* memmove(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
void* memchr(const void* ptr, int value, size_t num);
char* strerror(int errnum);
size_t strnlen(const char* str, size_t maxlen);
/* POSIX: case-insensitive comparison */
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);
/* C23 §7.1.4: memset_explicit — memset that must not be eliminated by optimisers.
 * Intended for zeroing security-sensitive buffers (e.g. crypto keys).
 * On Windows this maps to SecureZeroMemory from kernel32. */
void* memset_explicit(void* s, int c, size_t n);
/* C23 §7.27.1.2: memccpy — copy at most n bytes from src to dst, stopping after the
 * byte with value c is copied.  Returns pointer past the copied c, or NULL if not found. */
void* memccpy(void* dst, const void* src, int c, size_t n);
#endif
