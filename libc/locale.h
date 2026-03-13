/* locale.h — C11/C23 §7.11 Localization for RCC (Windows x64)
 *
 * Provides LC_* category constants, struct lconv, setlocale and localeconv.
 * All declarations map to MSVCRT implementations.
 *
 * By default all programs use the "C" locale.
 * Platform: Windows x64
 */
#ifndef _RCC_LOCALE_H
#define _RCC_LOCALE_H

#ifndef NULL
#define NULL ((void*)0)
#endif

/* §7.11.1.1: locale category constants (match MSVCRT values) */
#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5

/* §7.11.2.1: lconv — numeric and monetary formatting conventions */
struct lconv {
    char *decimal_point;       /* non-monetary decimal-point character */
    char *thousands_sep;       /* non-monetary thousands separator */
    char *grouping;            /* non-monetary digit grouping sizes */
    char *int_curr_symbol;     /* international currency symbol */
    char *currency_symbol;     /* local currency symbol */
    char *mon_decimal_point;   /* monetary decimal-point character */
    char *mon_thousands_sep;   /* monetary thousands separator */
    char *mon_grouping;        /* monetary digit grouping sizes */
    char *positive_sign;       /* positive sign */
    char *negative_sign;       /* negative sign */
    char  int_frac_digits;     /* international fractional digit count */
    char  frac_digits;         /* local fractional digit count */
    char  p_cs_precedes;       /* 1 if currency symbol precedes positive value */
    char  p_sep_by_space;      /* 1 if space separates positive currency symbol */
    char  n_cs_precedes;       /* 1 if currency symbol precedes negative value */
    char  n_sep_by_space;      /* 1 if space separates negative currency symbol */
    char  p_sign_posn;         /* positive sign position */
    char  n_sign_posn;         /* negative sign position */
};

/* §7.11.1.1: setlocale — set or query locale.
 * Returns pointer to the current locale name string, or NULL on failure. */
char *setlocale(int category, const char *locale);

/* §7.11.2.1: localeconv — query current numeric formatting.
 * Returns pointer to a static struct lconv for the current locale. */
struct lconv *localeconv(void);

#endif /* _RCC_LOCALE_H */
