/* RCC stub <process.h> — MSVC CRT thread/process declarations.
 * Provides _beginthreadex/_endthreadex used by sqlite3.c Windows threading code.
 */
#pragma once
#ifndef _PROCESS_H
#define _PROCESS_H

#include <stddef.h>

/* _beginthreadex — start a thread using the CRT (recommended over CreateThread) */
unsigned long _beginthreadex(
    void*    security,        /* LPSECURITY_ATTRIBUTES */
    unsigned stack_size,
    unsigned (*start_address)(void*),
    void*    arglist,
    unsigned initflag,
    unsigned* thrdaddr
);

/* _endthreadex — exit the current CRT thread */
void _endthreadex(unsigned retval);

/* _beginthread / _endthread (legacy, simpler forms) */
unsigned long _beginthread(void (*start_address)(void*), unsigned stack_size, void* arglist);
void _endthread(void);

/* Process control */
void _exit(int status);
int  _getpid(void);

#endif /* _PROCESS_H */
