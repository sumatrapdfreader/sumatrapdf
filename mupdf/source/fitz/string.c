// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <string.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h> /* for MultiByteToWideChar etc. */
#endif

#include "utfdata.h"

static const int *
fz_ucd_bsearch(int c, const int *t, int n, int ne)
{
	const int *p;
	int m;
	while (n > 1)
	{
		m = n/2;
		p = t + m*ne;
		if (c >= p[0])
		{
			t = p;
			n = n - m;
		}
		else
		{
			n = m;
		}
	}
	if (n && c >= t[0])
		return t;
	return 0;
}

int
fz_tolower(int c)
{
	const int *p;

	/* Make ASCII fast. */
	if (c < 128)
	{
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		return c;
	}

	p = fz_ucd_bsearch(c, ucd_tolower2, nelem(ucd_tolower2) / 3, 3);
	if (p && c >= p[0] && c <= p[1])
		return c + p[2];
	p = fz_ucd_bsearch(c, ucd_tolower1, nelem(ucd_tolower1) / 2, 2);
	if (p && c == p[0])
		return c + p[1];
	return c;
}

int
fz_toupper(int c)
{
	const int *p;
	p = fz_ucd_bsearch(c, ucd_toupper2, nelem(ucd_toupper2) / 3, 3);
	if (p && c >= p[0] && c <= p[1])
		return c + p[2];
	p = fz_ucd_bsearch(c, ucd_toupper1, nelem(ucd_toupper1) / 2, 2);
	if (p && c == p[0])
		return c + p[1];
	return c;
}

size_t
fz_strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? (size_t) (p - s) : n;
}

int
fz_strncasecmp(const char *a, const char *b, size_t n)
{
	while (n > 0)
	{
		int ucs_a, ucs_b, n_a, n_b;
		n_a = fz_chartorunen(&ucs_a, a, n);
		n_b = fz_chartorunen(&ucs_b, b, n);
		/* We believe that for all unicode characters X and Y, s.t.
		 * fz_tolower(X) == fz_tolower(Y), X and Y must utf8 encode to
		 * the same number of bytes. */
		assert(n_a == n_b);
		assert((size_t)n_a <= n);

		// one or both of the strings are short
		if (ucs_a == 0 || ucs_b == 0)
			return ucs_a - ucs_b;

		if (ucs_a != ucs_b)
		{
			ucs_a = fz_tolower(ucs_a);
			ucs_b = fz_tolower(ucs_b);
		}
		if (ucs_a != ucs_b)
			return ucs_a - ucs_b;

		a += n_a;
		b += n_b;
		n -= n_a;
	}
	return 0;
}

int
fz_strcasecmp(const char *a, const char *b)
{
	while (1)
	{
		int ucs_a, ucs_b;
		a += fz_chartorune(&ucs_a, a);
		b += fz_chartorune(&ucs_b, b);
		ucs_a = fz_tolower(ucs_a);
		ucs_b = fz_tolower(ucs_b);
		if (ucs_a == ucs_b)
		{
			if (ucs_a == 0)
				return 0;
		}
		else
			return ucs_a - ucs_b;
	}
}

char *
fz_strsep(char **stringp, const char *delim)
{
	char *ret = *stringp;
	if (!ret) return NULL;
	if ((*stringp = strpbrk(*stringp, delim)) != NULL)
		*((*stringp)++) = '\0';
	return ret;
}

size_t
fz_strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

size_t
fz_strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (*d != '\0' && n-- != 0)
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return dlen + strlen(s);
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return dlen + (s - src);	/* count does not include NUL */
}

void
fz_dirname(char *dir, const char *path, size_t n)
{
	size_t i;

	if (!path || !path[0])
	{
		fz_strlcpy(dir, ".", n);
		return;
	}

	fz_strlcpy(dir, path, n);

	i = strlen(dir);
	for(; dir[i] == '/'; --i) if (!i) { fz_strlcpy(dir, "/", n); return; }
	for(; dir[i] != '/'; --i) if (!i) { fz_strlcpy(dir, ".", n); return; }
	for(; dir[i] == '/'; --i) if (!i) { fz_strlcpy(dir, "/", n); return; }
	dir[i+1] = 0;
}

const char *
fz_basename(const char *path)
{
	const char *name = strrchr(path, '/');
	if (!name)
		name = strrchr(path, '\\');
	if (!name)
		return path;
	return name + 1;
}

#ifdef _WIN32

char *fz_realpath(const char *path, char *buf)
{
	wchar_t wpath[PATH_MAX];
	wchar_t wbuf[PATH_MAX];
	int i;
	if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_MAX))
		return NULL;
	if (!GetFullPathNameW(wpath, PATH_MAX, wbuf, NULL))
		return NULL;
	if (!WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, PATH_MAX, NULL, NULL))
		return NULL;
	for (i=0; buf[i]; ++i)
		if (buf[i] == '\\')
			buf[i] = '/';
	return buf;
}

#else

char *fz_realpath(const char *path, char *buf)
{
	return realpath(path, buf);
}

#endif

static inline int ishex(int a)
{
	return (a >= 'A' && a <= 'F') ||
		(a >= 'a' && a <= 'f') ||
		(a >= '0' && a <= '9');
}

static inline int tohex(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	return 0;
}

#define URIRESERVED ";/?:@&=+$,"
#define URIALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define URIDIGIT "0123456789"
#define URIMARK "-_.!~*'()"
#define URIUNESCAPED URIALPHA URIDIGIT URIMARK
#define HEX "0123456789ABCDEF"

/* Same as fz_decode_uri_component but in-place */
char *
fz_urldecode(char *url)
{
	char *s = url;
	char *p = url;
	while (*s)
	{
		int c = (unsigned char) *s++;
		if (c == '%' && ishex(s[0]) && ishex(s[1]))
		{
			int a = tohex(*s++);
			int b = tohex(*s++);
			*p++ = a << 4 | b;
		}
		else
		{
			*p++ = c;
		}
	}
	*p = 0;
	return url;
}

char *
fz_decode_uri_component(fz_context *ctx, const char *s)
{
	char *uri = fz_malloc(ctx, strlen(s) + 1);
	char *p = uri;
	while (*s)
	{
		int c = (unsigned char) *s++;
		if (c == '%' && ishex(s[0]) && ishex(s[1]))
		{
			int a = tohex(*s++);
			int b = tohex(*s++);
			*p++ = a << 4 | b;
		}
		else
		{
			*p++ = c;
		}
	}
	*p = 0;
	return uri;
}

char *
fz_decode_uri(fz_context *ctx, const char *s)
{
	char *uri = fz_malloc(ctx, strlen(s) + 1);
	char *p = uri;
	while (*s)
	{
		int c = (unsigned char) *s++;
		if (c == '%' && ishex(s[0]) && ishex(s[1]))
		{
			int a = tohex(*s++);
			int b = tohex(*s++);
			c = a << 4 | b;
			if (strchr(URIRESERVED "#", c)) {
				*p++ = '%';
				*p++ = HEX[a];
				*p++ = HEX[b];
			} else {
				*p++ = c;
			}
		}
		else
		{
			*p++ = c;
		}
	}
	*p = 0;
	return uri;
}

static char *
fz_encode_uri_imp(fz_context *ctx, const char *s, const char *unescaped)
{
	char *uri = fz_malloc(ctx, strlen(s) * 3 + 1); /* allocate enough for worst case */
	char *p = uri;
	while (*s)
	{
		int c = (unsigned char) *s++;
		if (strchr(unescaped, c))
		{
			*p++ = c;
		}
		else
		{
			*p++ = '%';
			*p++ = HEX[(c >> 4) & 15];
			*p++ = HEX[(c) & 15];
		}
	}
	*p = 0;
	return uri;
}

char *
fz_encode_uri_component(fz_context *ctx, const char *s)
{
	return fz_encode_uri_imp(ctx, s, URIUNESCAPED);
}

char *
fz_encode_uri_pathname(fz_context *ctx, const char *s)
{
	return fz_encode_uri_imp(ctx, s, URIUNESCAPED "/");
}

char *
fz_encode_uri(fz_context *ctx, const char *s)
{
	return fz_encode_uri_imp(ctx, s, URIUNESCAPED URIRESERVED "#");
}

void
fz_format_output_path(fz_context *ctx, char *path, size_t size, const char *fmt, int page)
{
	const char *s, *p;
	char num[40];
	int i, n;
	int z = 0;

	for (i = 0; page; page /= 10)
		num[i++] = '0' + page % 10;
	num[i] = 0;

	s = p = strchr(fmt, '%');
	if (p)
	{
		++p;
		while (*p >= '0' && *p <= '9')
			z = z * 10 + (*p++ - '0');
	}
	if (p && *p == 'd')
	{
		++p;
	}
	else
	{
		const char *psep = strrchr(fmt, '/');
		s = p = strrchr(fmt, '.');
		/* Ensure we only match a . in the last path segment. */
		if (psep != NULL && p < psep)
			p = NULL;
		if (!p)
			s = p = fmt + strlen(fmt);
	}

	if (z < 1)
		z = 1;
	while (i < z && i < (int)sizeof num)
		num[i++] = '0';
	n = s - fmt;
	if (n + i + strlen(p) >= size)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "path name buffer overflow");
	memcpy(path, fmt, n);
	while (i > 0)
		path[n++] = num[--i];
	fz_strlcpy(path + n, p, size - n);
}

#define SEP(x) ((x)=='/' || (x) == 0)

char *
fz_cleanname(char *name)
{
	char *p, *q, *dotdot;
	int rooted;

	rooted = name[0] == '/';

	/*
	 * invariants:
	 *		p points at beginning of path element we're considering.
	 *		q points just past the last path element we wrote (no slash).
	 *		dotdot points just past the point where .. cannot backtrack
	 *				any further (no slash).
	 */
	p = q = dotdot = name + rooted;
	while (*p)
	{
		if(p[0] == '/') /* null element */
			p++;
		else if (p[0] == '.' && SEP(p[1]))
			p += 1; /* don't count the separator in case it is nul */
		else if (p[0] == '.' && p[1] == '.' && SEP(p[2]))
		{
			p += 2;
			if (q > dotdot) /* can backtrack */
			{
				while(--q > dotdot && *q != '/')
					;
			}
			else if (!rooted) /* /.. is / but ./../ is .. */
			{
				if (q != name)
					*q++ = '/';
				*q++ = '.';
				*q++ = '.';
				dotdot = q;
			}
		}
		else /* real path element */
		{
			if (q != name+rooted)
				*q++ = '/';
			while ((*q = *p) != '/' && *q != 0)
				p++, q++;
		}
	}

	if (q == name) /* empty string is really "." */
		*q++ = '.';
	*q = '\0';
	return name;
}

char *
fz_cleanname_strdup(fz_context *ctx, const char *name)
{
	size_t len = strlen(name);
	char *newname = fz_malloc(ctx, fz_maxz(2, len + 1));
	memcpy(newname, name, len + 1);
	newname[len] = '\0';
	return fz_cleanname(newname);
}

enum
{
	UTFmax = 4, /* maximum bytes per rune */
	Runesync = 0x80, /* cannot represent part of a UTF sequence (<) */
	Runeself = 0x80, /* rune and UTF sequences are the same (<) */
	Runeerror = 0xFFFD, /* decoding error in UTF */
	Runemax = 0x10FFFF, /* maximum rune value */
};

enum
{
	Bit1 = 7,
	Bitx = 6,
	Bit2 = 5,
	Bit3 = 4,
	Bit4 = 3,
	Bit5 = 2,

	T1 = ((1<<(Bit1+1))-1) ^ 0xFF, /* 0000 0000 */
	Tx = ((1<<(Bitx+1))-1) ^ 0xFF, /* 1000 0000 */
	T2 = ((1<<(Bit2+1))-1) ^ 0xFF, /* 1100 0000 */
	T3 = ((1<<(Bit3+1))-1) ^ 0xFF, /* 1110 0000 */
	T4 = ((1<<(Bit4+1))-1) ^ 0xFF, /* 1111 0000 */
	T5 = ((1<<(Bit5+1))-1) ^ 0xFF, /* 1111 1000 */

	Rune1 = (1<<(Bit1+0*Bitx))-1, /* 0000 0000 0111 1111 */
	Rune2 = (1<<(Bit2+1*Bitx))-1, /* 0000 0111 1111 1111 */
	Rune3 = (1<<(Bit3+2*Bitx))-1, /* 1111 1111 1111 1111 */
	Rune4 = (1<<(Bit4+3*Bitx))-1, /* 0001 1111 1111 1111 1111 1111 */

	Maskx = (1<<Bitx)-1,	/* 0011 1111 */
	Testx = Maskx ^ 0xFF,	/* 1100 0000 */

	Bad = Runeerror,
};

int
fz_chartorune(int *rune, const char *str)
{
	int c, c1, c2, c3;
	int l;

	/* overlong null character */
	if((unsigned char)str[0] == 0xc0 && (unsigned char)str[1] == 0x80) {
		*rune = 0;
		return 2;
	}

	/*
	 * one character sequence
	 *	00000-0007F => T1
	 */
	c = *(const unsigned char*)str;
	if(c < Tx) {
		*rune = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	0080-07FF => T2 Tx
	 */
	c1 = *(const unsigned char*)(str+1) ^ Tx;
	if(c1 & Testx)
		goto bad;
	if(c < T3) {
		if(c < T2)
			goto bad;
		l = ((c << Bitx) | c1) & Rune2;
		if(l <= Rune1)
			goto bad;
		*rune = l;
		return 2;
	}

	/*
	 * three character sequence
	 *	0800-FFFF => T3 Tx Tx
	 */
	c2 = *(const unsigned char*)(str+2) ^ Tx;
	if(c2 & Testx)
		goto bad;
	if(c < T4) {
		l = ((((c << Bitx) | c1) << Bitx) | c2) & Rune3;
		if(l <= Rune2)
			goto bad;
		*rune = l;
		return 3;
	}

	/*
	 * four character sequence (21-bit value)
	 *	10000-1FFFFF => T4 Tx Tx Tx
	 */
	c3 = *(const unsigned char*)(str+3) ^ Tx;
	if (c3 & Testx)
		goto bad;
	if (c < T5) {
		l = ((((((c << Bitx) | c1) << Bitx) | c2) << Bitx) | c3) & Rune4;
		if (l <= Rune3)
			goto bad;
		*rune = l;
		return 4;
	}
	/*
	 * Support for 5-byte or longer UTF-8 would go here, but
	 * since we don't have that, we'll just fall through to bad.
	 */

	/*
	 * bad decoding
	 */
bad:
	*rune = Bad;
	return 1;
}

int
fz_chartorunen(int *rune, const char *str, size_t n)
{
	int c, c1, c2, c3;
	int l;

	if (n < 1)
		goto bad;

	/*
	 * one character sequence
	 *	00000-0007F => T1
	 */
	c = *(const unsigned char*)str;
	if(c < Tx) {
		*rune = c;
		return 1;
	}

	if (n < 2)
		goto bad;

	/* overlong null character */
	if((unsigned char)str[0] == 0xc0 && (unsigned char)str[1] == 0x80) {
		*rune = 0;
		return 2;
	}

	/*
	 * two character sequence
	 *	0080-07FF => T2 Tx
	 */
	c1 = *(const unsigned char*)(str+1) ^ Tx;
	if(c1 & Testx)
		goto bad;
	if(c < T3) {
		if(c < T2)
			goto bad;
		l = ((c << Bitx) | c1) & Rune2;
		if(l <= Rune1)
			goto bad;
		*rune = l;
		return 2;
	}

	if (n < 3)
		goto bad;

	/*
	 * three character sequence
	 *	0800-FFFF => T3 Tx Tx
	 */
	c2 = *(const unsigned char*)(str+2) ^ Tx;
	if(c2 & Testx)
		goto bad;
	if(c < T4) {
		l = ((((c << Bitx) | c1) << Bitx) | c2) & Rune3;
		if(l <= Rune2)
			goto bad;
		*rune = l;
		return 3;
	}

	if (n < 4)
		goto bad;

	/*
	 * four character sequence (21-bit value)
	 *	10000-1FFFFF => T4 Tx Tx Tx
	 */
	c3 = *(const unsigned char*)(str+3) ^ Tx;
	if (c3 & Testx)
		goto bad;
	if (c < T5) {
		l = ((((((c << Bitx) | c1) << Bitx) | c2) << Bitx) | c3) & Rune4;
		if (l <= Rune3)
			goto bad;
		*rune = l;
		return 4;
	}
	/*
	 * Support for 5-byte or longer UTF-8 would go here, but
	 * since we don't have that, we'll just fall through to bad.
	 */

	/*
	 * bad decoding
	 */
bad:
	*rune = Bad;
	return 1;
}

int
fz_runetochar(char *str, int rune)
{
	/* Runes are signed, so convert to unsigned for range check. */
	unsigned int c = (unsigned int)rune;

	/* overlong null character */
	if (c == 0) {
		((unsigned char *)str)[0] = 0xc0;
		((unsigned char *)str)[1] = 0x80;
		return 2;
	}

	/*
	 * one character sequence
	 *	00000-0007F => 00-7F
	 */
	if(c <= Rune1) {
		str[0] = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	0080-07FF => T2 Tx
	 */
	if(c <= Rune2) {
		str[0] = T2 | (c >> 1*Bitx);
		str[1] = Tx | (c & Maskx);
		return 2;
	}

	/*
	 * If the Rune is out of range, convert it to the error rune.
	 * Do this test here because the error rune encodes to three bytes.
	 * Doing it earlier would duplicate work, since an out of range
	 * Rune wouldn't have fit in one or two bytes.
	 */
	if (c > Runemax)
		c = Runeerror;

	/*
	 * three character sequence
	 *	0800-FFFF => T3 Tx Tx
	 */
	if (c <= Rune3) {
		str[0] = T3 | (c >> 2*Bitx);
		str[1] = Tx | ((c >> 1*Bitx) & Maskx);
		str[2] = Tx | (c & Maskx);
		return 3;
	}

	/*
	 * four character sequence (21-bit value)
	 *	10000-1FFFFF => T4 Tx Tx Tx
	 */
	str[0] = T4 | (c >> 3*Bitx);
	str[1] = Tx | ((c >> 2*Bitx) & Maskx);
	str[2] = Tx | ((c >> 1*Bitx) & Maskx);
	str[3] = Tx | (c & Maskx);
	return 4;
}

int
fz_runelen(int c)
{
	char str[10];
	return fz_runetochar(str, c);
}

int
fz_runeidx(const char *s, const char *p)
{
	int rune;
	int i = 0;
	while (s < p) {
		if (*(unsigned char *)s < Runeself)
			++s;
		else
			s += fz_chartorune(&rune, s);
		++i;
	}
	return i;
}

const char *
fz_runeptr(const char *s, int i)
{
	int rune;
	while (i-- > 0) {
		rune = *(unsigned char*)s;
		if (rune < Runeself) {
			if (rune == 0)
				return NULL;
			++s;
		} else
			s += fz_chartorune(&rune, s);
	}
	return s;
}

int
fz_utflen(const char *s)
{
	int c, n, rune;
	n = 0;
	for(;;) {
		c = *(const unsigned char*)s;
		if(c < Runeself) {
			if(c == 0)
				return n;
			s++;
		} else
			s += fz_chartorune(&rune, s);
		n++;
	}
}

float fz_atof(const char *s)
{
	float result;

	if (s == NULL)
		return 0;

	errno = 0;
	result = fz_strtof(s, NULL);
	if ((errno == ERANGE && result == 0) || isnan(result))
		/* Return 1.0 on  underflow, as it's a small known value that won't cause a divide by 0.  */
		return 1;
	result = fz_clamp(result, -FLT_MAX, FLT_MAX);
	return result;
}

int fz_atoi(const char *s)
{
	if (s == NULL)
		return 0;
	return atoi(s);
}

int64_t fz_atoi64(const char *s)
{
	if (s == NULL)
		return 0;
	return atoll(s);
}

size_t fz_atoz(const char *s)
{
	int64_t i;

	if (s == NULL)
		return 0;
	i = atoll(s);
	if (i < 0 || (int64_t)(size_t)i != i)
		return 0;
	return (size_t)i;
}

int fz_is_page_range(fz_context *ctx, const char *s)
{
	/* TODO: check the actual syntax... */
	while (*s)
	{
		if ((*s < '0' || *s > '9') && *s != 'N' && *s != '-' && *s != ',')
			return 0;
		s++;
	}
	return 1;
}

const char *fz_parse_page_range(fz_context *ctx, const char *s, int *a, int *b, int n)
{
	const char *orig = s;

	if (!s || !s[0])
		return NULL;

	if (s[0] == ',')
		s += 1;

	if (s[0] == 'N')
	{
		*a = n;
		s += 1;
	}
	else
		*a = strtol(s, (char**)&s, 10);

	if (s[0] == '-')
	{
		if (s[1] == 'N')
		{
			*b = n;
			s += 2;
		}
		else
			*b = strtol(s+1, (char**)&s, 10);
	}
	else
		*b = *a;

	if (*a < 0) *a = n + 1 + *a;
	if (*b < 0) *b = n + 1 + *b;

	*a = fz_clampi(*a, 1, n);
	*b = fz_clampi(*b, 1, n);

	if (s == orig)
	{
		fz_warn(ctx, "skipping invalid page range");
		return NULL;
	}

	return s;
}

/* memmem from musl */

#define MAX(a,b) ((a)>(b)?(a):(b))

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

static char *twobyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint16_t nw = n[0]<<8 | n[1], hw = h[0]<<8 | h[1];
	for (h++, k--; k; k--, hw = hw<<8 | *++h)
		if (hw == nw) return (char *)h-1;
	return 0;
}

static char *threebyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint32_t nw = n[0]<<24 | n[1]<<16 | n[2]<<8;
	uint32_t hw = h[0]<<24 | h[1]<<16 | h[2]<<8;
	for (h+=2, k-=2; k; k--, hw = (hw|*++h)<<8)
		if (hw == nw) return (char *)h-2;
	return 0;
}

static char *fourbyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint32_t nw = n[0]<<24 | n[1]<<16 | n[2]<<8 | n[3];
	uint32_t hw = h[0]<<24 | h[1]<<16 | h[2]<<8 | h[3];
	for (h+=3, k-=3; k; k--, hw = hw<<8 | *++h)
		if (hw == nw) return (char *)h-3;
	return 0;
}

static char *twoway_memmem(const unsigned char *h, const unsigned char *z, const unsigned char *n, size_t l)
{
	size_t i, ip, jp, k, p, ms, p0, mem, mem0;
	size_t byteset[32 / sizeof(size_t)] = { 0 };
	size_t shift[256];

	/* Computing length of needle and fill shift table */
	for (i=0; i<l; i++)
		BITOP(byteset, n[i], |=), shift[n[i]] = i+1;

	/* Compute maximal suffix */
	ip = (size_t)-1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] > n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	ms = ip;
	p0 = p;

	/* And with the opposite comparison */
	ip = (size_t)-1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] < n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	if (ip+1 > ms+1) ms = ip;
	else p = p0;

	/* Periodic needle? */
	if (memcmp(n, n+p, ms+1)) {
		mem0 = 0;
		p = MAX(ms, l-ms-1) + 1;
	} else mem0 = l-p;
	mem = 0;

	/* Search loop */
	for (;;) {
		/* If remainder of haystack is shorter than needle, done */
		if ((size_t)(z-h) < l) return 0;

		/* Check last byte first; advance by shift on mismatch */
		if (BITOP(byteset, h[l-1], &)) {
			k = l-shift[h[l-1]];
			if (k) {
				if (mem0 && mem && k < p) k = l-p;
				h += k;
				mem = 0;
				continue;
			}
		} else {
			h += l;
			mem = 0;
			continue;
		}

		/* Compare right half */
		for (k=MAX(ms+1,mem); k<l && n[k] == h[k]; k++);
		if (k < l) {
			h += k-ms;
			mem = 0;
			continue;
		}
		/* Compare left half */
		for (k=ms+1; k>mem && n[k-1] == h[k-1]; k--);
		if (k <= mem) return (char *)h;
		h += p;
		mem = mem0;
	}
}

void *fz_memmem(const void *h0, size_t k, const void *n0, size_t l)
{
	const unsigned char *h = h0, *n = n0;

	/* Return immediately on empty needle */
	if (!l) return (void *)h;

	/* Return immediately when needle is longer than haystack */
	if (k<l) return 0;

	/* Use faster algorithms for short needles */
	h = memchr(h0, *n, k);
	if (!h || l==1) return (void *)h;
	k -= h - (const unsigned char *)h0;
	if (k<l) return 0;
	if (l==2) return twobyte_memmem(h, k, n);
	if (l==3) return threebyte_memmem(h, k, n);
	if (l==4) return fourbyte_memmem(h, k, n);

	return twoway_memmem(h, h+k, n, l);
}

char *
fz_utf8_from_wchar(fz_context *ctx, const wchar_t *s)
{
	const wchar_t *src = s;
	char *d;
	char *dst;
	int len = 1;

	while (*src)
	{
		len += fz_runelen(*src++);
	}

	d = Memento_label(fz_malloc(ctx, len), "utf8_from_wchar");
	dst = d;
	src = s;
	while (*src)
	{
		dst += fz_runetochar(dst, *src++);
	}
	*dst = 0;

	return d;
}

wchar_t *
fz_wchar_from_utf8(fz_context *ctx, const char *path)
{
	size_t z = 0;
	const char *p = path;
	wchar_t *wpath, *w;

	if (!path)
		return NULL;

	while (*p)
	{
		int c;
		p += fz_chartorune(&c, p);
		z++;
		if (c >= 0x10000)
			z++;
	}

	w = wpath = fz_malloc(ctx, 2*(z+1));
	while (*path)
	{
		int c;
		path += fz_chartorune(&c, path);
		if (c >= 0x10000)
		{
			c -= 0x10000;
			*w++ = 0xd800 + (c>>10);
			*w++ = 0xdc00 + (c&1023);
		}
		else
			*w++ = c;
	}
	*w = 0;

	return wpath;
}

const char *
fz_strstr(const char *haystack, const char *needle)
{
	size_t matchlen = 0;
	char d;

	if (haystack == NULL || needle == NULL)
		return NULL;

	while ((d = needle[matchlen]) != 0)
	{
		char c = *haystack++;
		if (c == 0)
			return NULL;
		if (c == d)
			matchlen++;
		else
		{
			haystack -= matchlen;
			matchlen = 0;
		}
	}

	return haystack - matchlen;
}

const char *
fz_strstrcase(const char *haystack, const char *needle)
{
	size_t matchlen = 0;
	size_t firstlen;

	if (haystack == NULL || needle == NULL)
		return NULL;

	while (1)
	{
		int c, d;
		int nc, nd;

		nd = fz_chartorune(&d, &needle[matchlen]);
		if (d == 0)
			break;
		nc = fz_chartorune(&c, haystack);
		if (matchlen == 0)
			firstlen = nc;
		haystack += nc;
		matchlen += nd;
		if (c == 0)
			return NULL;
		if (c != d)
			haystack -= matchlen - firstlen, matchlen = 0;
	}

	return haystack - matchlen;
}

static inline int my_isdigit(int c) {
	return c >= '0' && c <= '9';
}

int
fz_strverscmp(const char *l0, const char *r0)
{
	// This strverscmp implementation is borrowed from musl.
	// Copyright © 2005-2020 Rich Felker, et al.
	// Standard MIT license.
	const unsigned char *l = (const void *)l0;
	const unsigned char *r = (const void *)r0;
	size_t i, dp, j;
	int z = 1;

	/* Find maximal matching prefix and track its maximal digit
	 * suffix and whether those digits are all zeros. */
	for (dp=i=0; l[i]==r[i]; i++) {
		int c = l[i];
		if (!c) return 0;
		if (!my_isdigit(c)) dp=i+1, z=1;
		else if (c!='0') z=0;
	}

	if (l[dp]!='0' && r[dp]!='0') {
		/* If we're not looking at a digit sequence that began
		 * with a zero, longest digit string is greater. */
		for (j=i; my_isdigit(l[j]); j++)
			if (!my_isdigit(r[j])) return 1;
		if (my_isdigit(r[j])) return -1;
	} else if (z && dp<i && (my_isdigit(l[i]) || my_isdigit(r[i]))) {
		/* Otherwise, if common prefix of digit sequence is
		 * all zeros, digits order less than non-digits. */
		return (unsigned char)(l[i]-'0') - (unsigned char)(r[i]-'0');
	}

	return l[i] - r[i];
}
