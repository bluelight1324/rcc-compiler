#ifndef _ERRNO_H
#define _ERRNO_H

extern int* _errno(void);
#define errno (*_errno())

#define EDOM 33
#define ERANGE 34
#define EILSEQ 42

#endif
