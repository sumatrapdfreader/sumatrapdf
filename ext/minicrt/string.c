#include <windows.h>
#include <limits.h>
#include <stdlib.h>

#include "libctiny.h"

#ifndef _CONST_RETURN
#define _CONST_RETURN
#endif

/***
*wcsstr.c - search for one wide-character string inside another
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcsstr() - search for one wchar_t string inside another
*
*******************************************************************************/

/***
*wchar_t *wcsstr(string1, string2) - search for string2 in string1
*       (wide strings)
*
*Purpose:
*       finds the first occurrence of string2 in string1 (wide strings)
*
*Entry:
*       wchar_t *string1 - string to search in
*       wchar_t *string2 - string to search for
*
*Exit:
*       returns a pointer to the first occurrence of string2 in
*       string1, or NULL if string2 does not occur in string1
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

_CONST_RETURN wchar_t * __cdecl wcsstr(const wchar_t * wcs1,
                                       const wchar_t * wcs2) {
        wchar_t *cp = (wchar_t *) wcs1;
        wchar_t *s1, *s2;

        if (!*wcs2)
            return (wchar_t *)wcs1;

        while (*cp)
        {
                s1 = cp;
                s2 = (wchar_t *) wcs2;

                while (*s1 && *s2 && !(*s1-*s2))
                        s1++, s2++;

                if (!*s2)
                        return(cp);

                cp++;
        }

        return(NULL);
}

/***
*wcsrchr.c - find last occurrence of wchar_t character in wide string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcsrchr() - find the last occurrence of a given character
*       in a string (wide-characters).
*
*******************************************************************************/

/***
*wchar_t *wcsrchr(string, ch) - find last occurrence of ch in wide string
*
*Purpose:
*       Finds the last occurrence of ch in string.  The terminating
*       null character is used as part of the search (wide-characters).
*
*Entry:
*       wchar_t *string - string to search in
*       wchar_t ch - character to search for
*
*Exit:
*       returns a pointer to the last occurrence of ch in the given
*       string
*       returns NULL if ch does not occurr in the string
*
*Exceptions:
*
*******************************************************************************/

_CONST_RETURN wchar_t * __cdecl wcsrchr(const wchar_t * string, wchar_t ch) {
        wchar_t *start = (wchar_t *)string;

        while (*string++)                       /* find end of string */
                ;
                                                /* search towards front */
        while (--string != start && *string != (wchar_t)ch)
                ;

        if (*string == (wchar_t)ch)             /* wchar_t found ? */
                return( (wchar_t *)string );

        return(NULL);
}

/***
*wcschr.c - search a wchar_t string for a given wchar_t character
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcschr() - search a wchar_t string for a wchar_t character
*
*******************************************************************************/

/***
*wchar_t *wcschr(string, c) - search a string for a wchar_t character
*
*Purpose:
*       Searches a wchar_t string for a given wchar_t character,
*       which may be the null character L'\0'.
*
*Entry:
*       wchar_t *string - wchar_t string to search in
*       wchar_t c - wchar_t character to search for
*
*Exit:
*       returns pointer to the first occurence of c in string
*       returns NULL if c does not occur in string
*
*Exceptions:
*
*******************************************************************************/

_CONST_RETURN wchar_t * __cdecl wcschr(const wchar_t * string, wchar_t ch) {
        while (*string && *string != (wchar_t)ch)
                string++;

        if (*string == (wchar_t)ch)
                return((wchar_t *)string);
        return(NULL);
}

/***
*xtoa.c - convert integers/longs to ASCII string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       The module has code to convert integers/longs to ASCII strings.  See
*
*******************************************************************************/

/***
*char *_itoa, *_ltoa, *_ultoa(val, buf, radix) - convert binary int to ASCII
*       string
*
*Purpose:
*       Converts an int to a character string.
*
*Entry:
*       val - number to be converted (int, long or unsigned long)
*       int radix - base to convert into
*       char *buf - ptr to buffer to place result
*
*Exit:
*       fills in space pointed to by buf with string result
*       returns a pointer to this buffer
*
*Exceptions:
*
*******************************************************************************/

/* helper routine that does the main job. */

static void __cdecl xtoa(unsigned long val, char *buf, unsigned radix, int is_neg) {
        char *p;                /* pointer to traverse string */
        char *firstdig;         /* pointer to first digit */
        char temp;              /* temp char */
        unsigned digval;        /* value of digit */

        p = buf;

        if (is_neg) {
            /* negative, so output '-' and negate */
            *p++ = '-';
            val = (unsigned long)(-(long)val);
        }

        firstdig = p;           /* save pointer to first digit */

        do {
            digval = (unsigned) (val % radix);
            val /= radix;       /* get next digit */

            /* convert to ascii and store */
            if (digval > 9)
                *p++ = (char) (digval - 10 + 'a');  /* a letter */
            else
                *p++ = (char) (digval + '0');       /* a digit */
        } while (val > 0);

        /* We now have the digit of the number in the buffer, but in reverse
           order.  Thus we reverse them now. */

        *p-- = '\0';            /* terminate string; p points to last digit */

        do {
            temp = *p;
            *p = *firstdig;
            *firstdig = temp;   /* swap *p and *firstdig */
            --p;
            ++firstdig;         /* advance to next two digits */
        } while (firstdig < p); /* repeat until halfway */
}

/* Actual functions just call conversion helper with neg flag set correctly,
   and return pointer to buffer. */

 char * __cdecl _itoa(int val, char *buf, int radix) {
    if (radix == 10 && val < 0)
        xtoa((unsigned long)val, buf, radix, 1);
    else
        xtoa((unsigned long)(unsigned int)val, buf, radix, 0);
    return buf;
}

/***
*strlen.c - contains strlen() routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       strlen returns the length of a null-terminated string,
*       not including the null byte itself.
*
*******************************************************************************/

#ifdef _MSC_VER
#pragma function(strlen)
#endif  /* _MSC_VER */

/***
*strlen - return the length of a null-terminated string
*
*Purpose:
*       Finds the length in bytes of the given string, not including
*       the final null character.
*
*Entry:
*       const char * str - string whose length is to be computed
*
*Exit:
*       length of the string "str", exclusive of the final null byte
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl strlen(const char * str) {
        const char *eos = str;

        while (*eos++) ;

        return( (int)(eos - str - 1) );
}

size_t __cdecl strnlen(const char *str, size_t maxsize) {
  return strlen(str);
}

/***
*wcsncpy.c - copy at most n characters of wide-character string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcsncpy() - copy at most n characters of wchar_t string
*
*******************************************************************************/

/***
*wchar_t *wcsncpy(dest, source, count) - copy at most n wide characters
*
*Purpose:
*       Copies count characters from the source string to the
*       destination.  If count is less than the length of source,
*       NO NULL CHARACTER is put onto the end of the copied string.
*       If count is greater than the length of sources, dest is padded
*       with null characters to length count (wide-characters).
*
*
*Entry:
*       wchar_t *dest - pointer to destination
*       wchar_t *source - source string for copy
*       size_t count - max number of characters to copy
*
*Exit:
*       returns dest
*
*Exceptions:
*
*******************************************************************************/

wchar_t * __cdecl wcsncpy(wchar_t * dest, const wchar_t * source, size_t count) {
        wchar_t *start = dest;

        while (count && (*dest++ = *source++))    /* copy string */
                count--;

        if (count)                              /* pad out with zeroes */
                while (--count)
                        *dest++ = L'\0';

        return(start);
}

/***
*wcscmp.c - routine to compare two wchar_t strings (for equal, less, or greater)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Compares two wide-character strings, determining their lexical order.
*
*******************************************************************************/

/***
*wcscmp - compare two wchar_t strings,
*        returning less than, equal to, or greater than
*
*Purpose:
*       wcscmp compares two wide-character strings and returns an integer
*       to indicate whether the first is less than the second, the two are
*       equal, or whether the first is greater than the second.
*
*       Comparison is done wchar_t by wchar_t on an UNSIGNED basis, which is to
*       say that Null wchar_t(0) is less than any other character.
*
*Entry:
*       const wchar_t * src - string for left-hand side of comparison
*       const wchar_t * dst - string for right-hand side of comparison
*
*Exit:
*       returns -1 if src <  dst
*       returns  0 if src == dst
*       returns +1 if src >  dst
*
*Exceptions:
*
*******************************************************************************/

int __cdecl wcscmp(const wchar_t * src, const wchar_t * dst) {
        int ret = 0 ;

        while (! (ret = (int)(*src - *dst)) && *dst)
                ++src, ++dst;

        if (ret < 0)
                ret = -1 ;
        else if (ret > 0)
                ret = 1 ;

        return( ret );
}

/***
*wcslen.c - contains wcslen() routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       wcslen returns the length of a null-terminated wide-character string,
*       not including the null wchar_t itself.
*
*******************************************************************************/

/***
*wcslen - return the length of a null-terminated wide-character string
*
*Purpose:
*       Finds the length in wchar_t's of the given string, not including
*       the final null wchar_t (wide-characters).
*
*Entry:
*       const wchar_t * wcs - string whose length is to be computed
*
*Exit:
*       length of the string "wcs", exclusive of the final null wchar_t
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl wcslen(
        const wchar_t * wcs
        ) {
        const wchar_t *eos = wcs;

        while (*eos++) ;

        return( (size_t)(eos - wcs - 1) );
}

/***
*strstr.c - search for one string inside another
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines strstr() - search for one string inside another
*
*******************************************************************************/

/***
*char *strstr(string1, string2) - search for string2 in string1
*
*Purpose:
*       finds the first occurrence of string2 in string1
*
*Entry:
*       char *string1 - string to search in
*       char *string2 - string to search for
*
*Exit:
*       returns a pointer to the first occurrence of string2 in
*       string1, or NULL if string2 does not occur in string1
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

_CONST_RETURN char * __cdecl strstr(
        const char * str1,
        const char * str2
        ) {
        char *cp = (char *) str1;
        char *s1, *s2;

        if (!*str2)
            return((char *)str1);

        while (*cp)
        {
                s1 = cp;
                s2 = (char *) str2;

                while (*s1 && *s2 && !(*s1-*s2))
                        s1++, s2++;

                if (!*s2)
                        return(cp);

                cp++;
        }

        return(NULL);

}

/***
*strcmp.c - routine to compare two strings (for equal, less, or greater)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Compares two string, determining their lexical order.
*
*******************************************************************************/

#ifdef _MSC_VER
#pragma function(strcmp)
#endif  /* _MSC_VER */

/***
*strcmp - compare two strings, returning less than, equal to, or greater than
*
*Purpose:
*       STRCMP compares two strings and returns an integer
*       to indicate whether the first is less than the second, the two are
*       equal, or whether the first is greater than the second.
*
*       Comparison is done byte by byte on an UNSIGNED basis, which is to
*       say that Null (0) is less than any other character (1-255).
*
*Entry:
*       const char * src - string for left-hand side of comparison
*       const char * dst - string for right-hand side of comparison
*
*Exit:
*       returns -1 if src <  dst
*       returns  0 if src == dst
*       returns +1 if src >  dst
*
*Exceptions:
*
*******************************************************************************/

int __cdecl strcmp(
        const char * src,
        const char * dst
        ) {
        int ret = 0 ;

        while (! (ret = *(unsigned char *)src - *(unsigned char *)dst) && *dst)
                ++src, ++dst;

        if (ret < 0)
                ret = -1 ;
        else if (ret > 0)
                ret = 1 ;

        return( ret );
}

#ifndef _MBSCAT
#ifdef _MSC_VER
#pragma function(strcpy)
#endif  /* _MSC_VER */
#endif  /* _MBSCAT */

/***
*char *strcpy(dst, src) - copy one string over another
*
*Purpose:
*       Copies the string src into the spot specified by
*       dest; assumes enough room.
*
*Entry:
*       char * dst - string over which "src" is to be copied
*       const char * src - string to be copied over "dst"
*
*Exit:
*       The address of "dst"
*
*Exceptions:
*******************************************************************************/

char * __cdecl strcpy(char * dst, const char * src) {
        char * cp = dst;

        while (*cp++ = *src++)
                ;               /* Copy src over dst */

        return( dst );
}

errno_t __cdecl strcpy_s(char * dst, size_t num_bytes, const char * src) {
#pragma warning(push)
#pragma warning(disable : 4996)
// 4996: 'function' was declared deprecated

  strcpy(dst, src);
#pragma warning(pop)

  return 0;
}

/***
*strncmp.c - compare first n characters of two strings
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines strncmp() - compare first n characters of two strings
*       for lexical order.
*
*******************************************************************************/

/***
*int strncmp(first, last, count) - compare first count chars of strings
*
*Purpose:
*       Compares two strings for lexical order.  The comparison stops
*       after: (1) a difference between the strings is found, (2) the end
*       of the strings is reached, or (3) count characters have been
*       compared.
*
*Entry:
*       char *first, *last - strings to compare
*       unsigned count - maximum number of characters to compare
*
*Exit:
*       returns <0 if first < last
*       returns  0 if first == last
*       returns >0 if first > last
*
*Exceptions:
*
*******************************************************************************/

int __cdecl strncmp(
        const char * first,
        const char * last,
        size_t count
        ) {
        if (!count)
                return(0);

        while (--count && *first && *first == *last)
        {
                first++;
                last++;
        }

        return( *(unsigned char *)first - *(unsigned char *)last );
}

/***
*strchr.c - search a string for a given character
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines strchr() - search a string for a character
*
*******************************************************************************/

/***
*char *strchr(string, c) - search a string for a character
*
*Purpose:
*       Searches a string for a given character, which may be the
*       null character '\0'.
*
*Entry:
*       char *string - string to search in
*       char c - character to search for
*
*Exit:
*       returns pointer to the first occurence of c in string
*       returns NULL if c does not occur in string
*
*Exceptions:
*
*******************************************************************************/

_CONST_RETURN char * __cdecl strchr(
        const char * string,
        int ch
        ) {
        while (*string && *string != (char)ch)
                string++;

        if (*string == (char)ch)
                return((char *)string);
        return(NULL);
}

/***
*wcsncmp.c - compare first n characters of two wide-character strings
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcsncmp() - compare first n characters of two wchar_t strings
*       for lexical order.
*
*******************************************************************************/

/***
*int wcsncmp(first, last, count) - compare first count chars of wchar_t strings
*
*Purpose:
*       Compares two strings for lexical order.  The comparison stops
*       after: (1) a difference between the strings is found, (2) the end
*       of the strings is reached, or (3) count characters have been
*       compared (wide-character strings).
*
*Entry:
*       wchar_t *first, *last - strings to compare
*       size_t count - maximum number of characters to compare
*
*Exit:
*       returns <0 if first < last
*       returns  0 if first == last
*       returns >0 if first > last
*
*Exceptions:
*
*******************************************************************************/

int __cdecl wcsncmp(
        const wchar_t * first,
        const wchar_t * last,
        size_t count
        ) {
        if (!count)
                return(0);

        while (--count && *first && *first == *last)
        {
                first++;
                last++;
        }

        return((int)(*first - *last));
}

/***
*wcsspn.c - find length of initial substring of chars from a control string
*       (wide-character strings)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcsspn() - finds the length of the initial substring of
*       a string consisting entirely of characters from a control string
*       (wide-character strings).
*
*******************************************************************************/

/***
*int wcsspn(string, control) - find init substring of control chars
*
*Purpose:
*       Finds the index of the first character in string that does belong
*       to the set of characters specified by control.  This is
*       equivalent to the length of the initial substring of string that
*       consists entirely of characters from control.  The L'\0' character
*       that terminates control is not considered in the matching process
*       (wide-character strings).
*
*Entry:
*       wchar_t *string - string to search
*       wchar_t *control - string containing characters not to search for
*
*Exit:
*       returns index of first wchar_t in string not in control
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl wcsspn(
        const wchar_t * string,
        const wchar_t * control
        ) {
        wchar_t *str = (wchar_t *) string;
        wchar_t *ctl;

        /* 1st char not in control string stops search */
        while (*str) {
            for (ctl = (wchar_t *)control; *ctl != *str; ctl++) {
                if (*ctl == (wchar_t)0) {
                    /*
                     * reached end of control string without finding a match
                     */
                    return (size_t)(str - string);
                }
            }
            str++;
        }
        /*
         * The whole string consisted of characters from control
         */
        return (size_t)(str - string);
}

/***
*wcscspn.c - find length of initial substring of wide characters
*        not in a control string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines wcscspn()- finds the length of the initial substring of
*       a string consisting entirely of characters not in a control string
*       (wide-character strings).
*
*******************************************************************************/

/***
*size_t wcscspn(string, control) - search for init substring w/o control wchars
*
*Purpose:
*       returns the index of the first character in string that belongs
*       to the set of characters specified by control.  This is equivalent
*       to the length of the length of the initial substring of string
*       composed entirely of characters not in control.  Null chars not
*       considered (wide-character strings).
*
*Entry:
*       wchar_t *string - string to search
*       wchar_t *control - set of characters not allowed in init substring
*
*Exit:
*       returns the index of the first wchar_t in string
*       that is in the set of characters specified by control.
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl wcscspn(
        const wchar_t * string,
        const wchar_t * control
        ) {
        wchar_t *str = (wchar_t *) string;
        wchar_t *wcset;

        /* 1st char in control string stops search */
        while (*str) {
            for (wcset = (wchar_t *)control; *wcset; wcset++) {
                if (*wcset == *str) {
                    return (size_t)(str - string);
                }
            }
            str++;
        }
        return (size_t)(str - string);
}

/***
*wchar_t *wcscpy(dst, src) - copy one wchar_t string over another
*
*Purpose:
*       Copies the wchar_t string src into the spot specified by
*       dest; assumes enough room.
*
*Entry:
*       wchar_t * dst - wchar_t string over which "src" is to be copied
*       const wchar_t * src - wchar_t string to be copied over "dst"
*
*Exit:
*       The address of "dst"
*
*Exceptions:
*******************************************************************************/

wchar_t * __cdecl wcscpy(wchar_t * dst, const wchar_t * src)
{
        wchar_t * cp = dst;

        while( *cp++ = *src++ )
                ;               /* Copy src over dst */

        return( dst );
}

/***
*strtol, strtoul(nptr,endptr,ibase) - Convert ascii string to long un/signed
*       int.
*
*Purpose:
*       Convert an ascii string to a long 32-bit value.  The base
*       used for the caculations is supplied by the caller.  The base
*       must be in the range 0, 2-36.  If a base of 0 is supplied, the
*       ascii string must be examined to determine the base of the
*       number:
*               (a) First char = '0', second char = 'x' or 'X',
*                   use base 16.
*               (b) First char = '0', use base 8
*               (c) First char in range '1' - '9', use base 10.
*
*       If the 'endptr' value is non-NULL, then strtol/strtoul places
*       a pointer to the terminating character in this value.
*       See ANSI standard for details
*
*Entry:
*       nptr == NEAR/FAR pointer to the start of string.
*       endptr == NEAR/FAR pointer to the end of the string.
*       ibase == integer base to use for the calculations.
*
*       string format: [whitespace] [sign] [0] [x] [digits/letters]
*
*Exit:
*       Good return:
*               result
*
*       Overflow return:
*               strtol -- LONG_MAX or LONG_MIN
*               strtoul -- ULONG_MAX
*               strtol/strtoul -- errno == ERANGE
*
*       No digits or bad base return:
*               0
*               endptr = nptr*
*
*Exceptions:
*       None.
*******************************************************************************/

/* flag values */
#define FL_UNSIGNED   1       /* strtoul called */
#define FL_NEG        2       /* negative sign found */
#define FL_OVERFLOW   4       /* overflow occured */
#define FL_READDIGIT  8       /* we've read at least one correct digit */

// __ascii_isdigit returns a non-zero value if c is a decimal digit (0 – 9).
int __ascii_isdigit(int c)
{
  return (c >= '0' && c <= '9');
}

// __ascii_isalpha returns a nonzero value if c is within
// the ranges A – Z or a – z.
int __ascii_isalpha(int c)
{
  return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

// __ascii_toupper converts lowercase character to uppercase.
int __ascii_toupper(int c)
{
  if (c >= 'a' && c <= 'z') return (c - ('a' - 'A'));
  return c;
}

int isspace(int c)
{
  static unsigned char spaces[256] =
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  // 0-9
    1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0,  // 30-39
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40-49
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 60-69
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 150-159
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 240-249
    0, 0, 0, 0, 0, 1,              // 250-255
  };

  return spaces[(unsigned char)c] == 1;
}

static unsigned long __cdecl strtoxl (
        const char *nptr,
        const char **endptr,
        int ibase,
        int flags
        )
{
        const char *p;
        char c;
        unsigned long number;
        unsigned digval;
        unsigned long maxval;

        p = nptr;                       /* p is our scanning pointer */
        number = 0;                     /* start with zero */

        c = *p++;                       /* read char */
        while ( isspace((int)(unsigned char)c) )
                c = *p++;               /* skip whitespace */

        if (c == '-') {
                flags |= FL_NEG;        /* remember minus sign */
                c = *p++;
        }
        else if (c == '+')
                c = *p++;               /* skip sign */

        if (ibase < 0 || ibase == 1 || ibase > 36) {
                /* bad base! */
                if (endptr)
                        /* store beginning of string in endptr */
                        *endptr = nptr;
                return 0L;              /* return 0 */
        }
        else if (ibase == 0) {
                /* determine base free-lance, based on first two chars of
                   string */
                if (c != '0')
                        ibase = 10;
                else if (*p == 'x' || *p == 'X')
                        ibase = 16;
                else
                        ibase = 8;
        }

        if (ibase == 16) {
                /* we might have 0x in front of number; remove if there */
                if (c == '0' && (*p == 'x' || *p == 'X')) {
                        ++p;
                        c = *p++;       /* advance past prefix */
                }
        }

        /* if our number exceeds this, we will overflow on multiply */
        maxval = ULONG_MAX / ibase;


        for (;;) {      /* exit in middle of loop */
                /* convert c to value */
                if ( __ascii_isdigit((int)(unsigned char)c) )
                        digval = c - '0';
                else if ( __ascii_isalpha((int)(unsigned char)c) )
                        digval = __ascii_toupper(c) - 'A' + 10;
                else
                        break;
                if (digval >= (unsigned)ibase)
                        break;          /* exit loop if bad digit found */

                /* record the fact we have read one digit */
                flags |= FL_READDIGIT;

                /* we now need to compute number = number * base + digval,
                   but we need to know if overflow occured.  This requires
                   a tricky pre-check. */

                if (number < maxval || (number == maxval &&
                (unsigned long)digval <= ULONG_MAX % ibase)) {
                        /* we won't overflow, go ahead and multiply */
                        number = number * ibase + digval;
                }
                else {
                        /* we would have overflowed -- set the overflow flag */
                        flags |= FL_OVERFLOW;
                }

                c = *p++;               /* read next digit */
        }

        --p;                            /* point to place that stopped scan */

        if (!(flags & FL_READDIGIT)) {
                /* no number there; return 0 and point to beginning of
                   string */
                if (endptr)
                        /* store beginning of string in endptr later on */
                        p = nptr;
                number = 0L;            /* return 0 */
        }
        else if ( (flags & FL_OVERFLOW) ||
                  ( !(flags & FL_UNSIGNED) &&
                    ( ( (flags & FL_NEG) && (number > -LONG_MIN) ) ||
                      ( !(flags & FL_NEG) && (number > LONG_MAX) ) ) ) )
        {
                /* overflow or signed overflow occurred */
                // errno = ERANGE;
                if ( flags & FL_UNSIGNED )
                        number = ULONG_MAX;
                else if ( flags & FL_NEG )
                        number = (unsigned long)(-LONG_MIN);
                else
                        number = LONG_MAX;
        }

        if (endptr != NULL)
                /* store pointer to char that stopped the scan */
                *endptr = p;

        if (flags & FL_NEG)
                /* negate result if there was a neg sign */
                number = (unsigned long)(-(long)number);

        return number;                  /* done. */
}

long __cdecl strtol (
        const char *nptr,
        char **endptr,
        int ibase
        )
{
        return (long) strtoxl(nptr, (const char**)endptr, ibase, 0);
}

unsigned long __cdecl strtoul (
        const char *nptr,
        char **endptr,
        int ibase
        )
{
        return strtoxl(nptr, (const char**)endptr, ibase, FL_UNSIGNED);
}

