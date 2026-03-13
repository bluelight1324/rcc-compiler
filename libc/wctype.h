/* wctype.h — C99/C11 §7.30 wide character classification for RCC
 *
 * Provides wide character classification and mapping functions.
 * Implementations are provided by the Windows C runtime (MSVCRT).
 *
 * Platform: Windows x64
 */
#ifndef _RCC_WCTYPE_H
#define _RCC_WCTYPE_H

/* wint_t: wide character result type */
#ifndef _RCC_WINT_T
#define _RCC_WINT_T
typedef unsigned short wint_t;
#endif

/* wctype_t: type for wide character class (from iswctype/wctype) */
typedef unsigned int wctype_t;

/* wctrans_t: type for wide character mapping (from towctrans/wctrans) */
typedef unsigned int wctrans_t;

#define WEOF  ((wint_t)0xFFFF)

/* §7.30.2 — Wide character classification functions */
int iswalpha (wint_t c);
int iswdigit (wint_t c);
int iswspace (wint_t c);
int iswupper (wint_t c);
int iswlower (wint_t c);
int iswpunct (wint_t c);
int iswprint (wint_t c);
int iswcntrl (wint_t c);
int iswalnum (wint_t c);
int iswxdigit(wint_t c);
int iswblank (wint_t c);
int iswgraph (wint_t c);

/* §7.30.3 — Wide character mapping functions */
wint_t towupper(wint_t c);
wint_t towlower(wint_t c);

#endif /* _RCC_WCTYPE_H */
