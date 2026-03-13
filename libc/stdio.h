#ifndef _RCC_STDIO_H
#define _RCC_STDIO_H
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif
typedef void FILE;
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t n, const char* format, ...);
int vprintf(const char* format, void* ap);
int vfprintf(FILE* stream, const char* format, void* ap);
int vsprintf(char* str, const char* format, void* ap);
int vsnprintf(char* str, size_t n, const char* format, void* ap);
int scanf(const char* format, ...);
int fscanf(FILE* stream, const char* format, ...);
int sscanf(const char* str, const char* format, ...);
int putchar(int c);
int getchar(void);
int puts(const char* str);
int fputc(int c, FILE* stream);
int fgetc(FILE* stream);
int fputs(const char* str, FILE* stream);
char* fgets(char* str, int n, FILE* stream);
int ungetc(int c, FILE* stream);
/* C11: "x" open-exclusive flag — fails if file exists. Supported by MSVCRT. */
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);
int fflush(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void perror(const char* str);
/* POSIX / glibc extension: allocates a string of the formatted output.
 * Available in MSVCRT via ucrt. Classified as Owner by RCC safety analysis. */
int asprintf(char** strp, const char* format, ...);
int vasprintf(char** strp, const char* format, void* ap);
/* C11: rename — atomic on some OSes (not guaranteed on Windows) */
int rename(const char* old_name, const char* new_name);
int remove(const char* filename);
/* C99/C11: v-scanf variants */
int vscanf(const char* format, void* ap);
int vfscanf(FILE* stream, const char* format, void* ap);
int vsscanf(const char* str, const char* format, void* ap);
/* C99: error/temp file utilities */
void clearerr(FILE* stream);
FILE* tmpfile(void);
char* tmpnam(char* str);
/* C99: getc/putc aliases (may be macros in real libc) */
int getc(FILE* stream);
int putc(int c, FILE* stream);
/* C99: setvbuf / setbuf */
int setvbuf(FILE* stream, char* buffer, int mode, size_t size);
void setbuf(FILE* stream, char* buffer);
/* C99: wide-character I/O stubs */
int fwide(FILE* stream, int mode);
/* Buffering modes for setvbuf */
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
/* Limits */
#define FILENAME_MAX 260
#define FOPEN_MAX    20
#define L_tmpnam     16
#define TMP_MAX      32767
#define BUFSIZ       512
#endif
