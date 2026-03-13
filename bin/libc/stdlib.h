#ifndef _RCC_STDLIB_H
#define _RCC_STDLIB_H
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif
void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
void exit(int status);
void abort(void);
/* C11: aligned_alloc — Windows does not export this function directly.
 * The MSVCRT equivalent is _aligned_malloc but with REVERSED argument order:
 *   Standard: aligned_alloc(alignment, size)
 *   Windows:  _aligned_malloc(size, alignment)
 * This macro wrapper swaps the arguments to match the C11 signature. */
void* _aligned_malloc(size_t size, size_t alignment);
#define aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
int atoi(const char* str);
long atol(const char* str);
double atof(const char* str);
long strtol(const char* str, char** endptr, int base);
long long strtoll(const char* str, char** endptr, int base);
unsigned long long strtoull(const char* str, char** endptr, int base);
double strtod(const char* str, char** endptr);
float strtof(const char* str, char** endptr);
long double strtold(const char* str, char** endptr);
int rand(void);
void srand(unsigned int seed);
int abs(int n);
long labs(long n);
void qsort(void* base, size_t num, size_t size, int (*compar)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t num, size_t size, int (*compar)(const void*, const void*));
char* getenv(const char* name);
/* C11: quick program termination (handlers registered with at_quick_exit run,
 * but not atexit handlers or static destructors). Passed through to MSVCRT. */
void quick_exit(int status);
int  at_quick_exit(void (*func)(void));
/* C99/C11: _Exit — terminate immediately with no cleanup */
void _Exit(int status);
/* C23 §7.24.3.6: memalignment(p) — largest power-of-2 alignment of pointer p.
 * Returns 0 if p is a null pointer.  Implementation: count trailing zero bits. */
#ifndef memalignment
static __inline unsigned long long memalignment(const void *p) {
    unsigned long long a = (unsigned long long)(p);
    if (!a) return 0ULL;
    unsigned long long align = 1ULL;
    while (!(a & align)) align <<= 1;
    return align;
}
#endif

/* C23 §7.24.1.5: strfromd/strfromf/strfroml — convert floating-point to string.
 * Equivalent to snprintf(s, n, fmt, fp) where fmt is a %a/%e/%f/%g conversion.
 * Returns the number of characters that would be written (excluding NUL),
 * or a negative value on encoding error. */
#ifndef _RCC_SNPRINTF_DECL
#define _RCC_SNPRINTF_DECL
extern int snprintf(char*, size_t, const char*, ...);
#endif
static __inline int strfromd(char *s, size_t n, const char *fmt, double fp) {
    return snprintf(s, n, fmt, fp);
}
static __inline int strfromf(char *s, size_t n, const char *fmt, float fp) {
    return snprintf(s, n, fmt, (double)fp);
}
static __inline int strfroml(char *s, size_t n, const char *fmt, long double fp) {
    return snprintf(s, n, fmt, (double)fp);
}

/* C23 §7.24.3.3: free_sized / free_aligned_sized — deallocation with size hints.
 * On current platforms (including Windows UCRT) the size/alignment hints are
 * ignored; the functions delegate to free() and _aligned_free() respectively.
 * Future implementations may use the hints to speed up pool-based allocators. */
void _aligned_free(void *ptr);
static __inline void free_sized(void *p, size_t size) {
    (void)size; free(p);
}
static __inline void free_aligned_sized(void *p, size_t alignment, size_t size) {
    (void)alignment; (void)size; _aligned_free(p);
}

#endif
