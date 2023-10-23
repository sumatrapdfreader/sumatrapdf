// Copyright (C) 2004-2022 Artifex Software, Inc.
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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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
	if (!n--)
		return 0;
	for (; *a && *b && n && (*a == *b || fz_tolower(*a) == fz_tolower(*b)); a++, b++, n--)
		;
	return fz_tolower(*a) - fz_tolower(*b);
}

int
fz_strcasecmp(const char *a, const char *b)
{
	while (fz_tolower(*a) == fz_tolower(*b))
	{
		if (*a++ == 0)
			return 0;
		b++;
	}
	return fz_tolower(*a) - fz_tolower(*b);
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
			int c = a << 4 | b;
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
		s = p = strrchr(fmt, '.');
		if (!p)
			s = p = fmt + strlen(fmt);
	}

	if (z < 1)
		z = 1;
	while (i < z && i < (int)sizeof num)
		num[i++] = '0';
	n = s - fmt;
	if (n + i + strlen(p) >= size)
		fz_throw(ctx, FZ_ERROR_GENERIC, "path name buffer overflow");
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
fz_runetochar(char *str, int rune)
{
	/* Runes are signed, so convert to unsigned for range check. */
	unsigned int c = (unsigned int)rune;

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
	return 0;
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
	ip = -1; jp = 0; k = p = 1;
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
	ip = -1; jp = 0; k = p = 1;
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
