/* The authors of this software are Rob Pike and Ken Thompson.
 * Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY. IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

#include "mupdf/fitz.h"

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef INFINITY
#define INFINITY (DBL_MAX+DBL_MAX)
#endif
#ifndef NAN
#define NAN (INFINITY-INFINITY)
#endif

#ifndef DEFINED_ULONG
#define DEFINED_ULONG
typedef unsigned long ulong;
#endif

enum { NSIGNIF	= 9 };

/*
 * first few powers of 10
 */
static float pows10[] =
{
	1e0f,	1e1f,	1e2f,	1e3f,	1e4f,	1e5f,	1e6f,	1e7f,	1e8f,	1e9f,
	1e10f,	1e11f,	1e12f,	1e13f,	1e14f,	1e15f,	1e16f,	1e17f,	1e18f,	1e19f,
	1e20f,	1e21f,	1e22f,	1e23f,	1e24f,	1e25f,	1e26f,	1e27f,	1e28f,	1e29f,
	1e30f,	1e31f,	1e32f,	1e33f,	1e34f,	1e35f,	1e36f,	1e37f,	1e38f
};
#define	npows10 ((int)(sizeof(pows10)/sizeof(pows10[0])))
#define	pow10(x) fmtpow10(x)

static float
pow10(int n)
{
	float d;
	int neg;

	neg = 0;
	if(n < 0){
		neg = 1;
		n = -n;
	}

	if(n < npows10)
		d = pows10[n];
	else{
		d = pows10[npows10-1];
		for(;;){
			n -= npows10 - 1;
			if(n < npows10){
				d *= pows10[n];
				break;
			}
			d *= pows10[npows10 - 1];
		}
	}
	if(neg)
		return 1./d;
	return d;
}

/*
 * add 1 to the decimal integer string a of length n.
 * if 99999 overflows into 10000, return 1 to tell caller
 * to move the virtual decimal point.
 */
static int
xadd1(char *a, int n)
{
	char *b;
	int c;

	if(n < 0 || n > NSIGNIF)
		return 0;
	for(b = a+n-1; b >= a; b--) {
		c = *b + 1;
		if(c <= '9') {
			*b = c;
			return 0;
		}
		*b = '0';
	}
	/*
	 * need to overflow adding digit.
	 * shift number down and insert 1 at beginning.
	 * decimal is known to be 0s or we wouldn't
	 * have gotten this far. (e.g., 99999+1 => 00000)
	 */
	a[0] = '1';
	return 1;
}

/*
 * subtract 1 from the decimal integer string a.
 * if 10000 underflows into 09999, make it 99999
 * and return 1 to tell caller to move the virtual
 * decimal point. this way, xsub1 is inverse of xadd1.
 */
static int
xsub1(char *a, int n)
{
	char *b;
	int c;

	if(n < 0 || n > NSIGNIF)
		return 0;
	for(b = a+n-1; b >= a; b--) {
		c = *b - 1;
		if(c >= '0') {
			if(c == '0' && b == a) {
				/*
				 * just zeroed the top digit; shift everyone up.
				 * decimal is known to be 9s or we wouldn't
				 * have gotten this far. (e.g., 10000-1 => 09999)
				 */
				*b = '9';
				return 1;
			}
			*b = c;
			return 0;
		}
		*b = '9';
	}
	/*
	 * can't get here. the number a is always normalized
	 * so that it has a nonzero first digit.
	 */
	return 0;
}

/*
 * format exponent like sprintf(p, "e%+d", e)
 */
static void
fmtexp(char *p, int e)
{
	char se[9];
	int i;

	*p++ = 'e';
	if(e < 0) {
		*p++ = '-';
		e = -e;
	} else
		*p++ = '+';
	i = 0;
	while(e) {
		se[i++] = e % 10 + '0';
		e /= 10;
	}
	while(i < 1)
		se[i++] = '0';
	while(i > 0)
		*p++ = se[--i];
	*p++ = '\0';
}

/*
 * compute decimal integer m, exp such that:
 *	f = m*10^exp
 *	m is as short as possible with losing exactness
 * assumes special cases (NaN, +Inf, -Inf) have been handled.
 */
void
fz_ftoa(float f, char *s, int *exp, int *neg, int *ns)
{
	int c, d, e2, e, ee, i, ndigit, oerrno;
	char tmp[NSIGNIF+10];
	float g;

	oerrno = errno; /* in case strtod smashes errno */

	/*
	 * make f non-negative.
	 */
	*neg = 0;
	if(f < 0) {
		f = -f;
		*neg = 1;
	}

	/*
	 * must handle zero specially.
	 */
	if(f == 0){
		*exp = 0;
		s[0] = '0';
		s[1] = '\0';
		*ns = 1;
		return;
	}

	/*
	 * find g,e such that f = g*10^e.
	 * guess 10-exponent using 2-exponent, then fine tune.
	 */
	frexpf(f, &e2);
	e = (int)(e2 * .301029995664f);
	g = f * pow10(-e);
	while(g < 1) {
		e--;
		g = f * pow10(-e);
	}
	while(g >= 10) {
		e++;
		g = f * pow10(-e);
	}

	/*
	 * convert NSIGNIF digits as a first approximation.
	 */
	for(i=0; i<NSIGNIF; i++) {
		d = (int)g;
		s[i] = d+'0';
		g = (g-d) * 10;
	}
	s[i] = 0;

	/*
	 * adjust e because s is 314159... not 3.14159...
	 */
	e -= NSIGNIF-1;
	fmtexp(s+NSIGNIF, e);

	/*
	 * adjust conversion until strtod(s) == f exactly.
	 */
	for(i=0; i<10; i++) {
		g = fz_strtod(s, NULL);
		if(f > g) {
			if(xadd1(s, NSIGNIF)) {
				/* gained a digit */
				e--;
				fmtexp(s+NSIGNIF, e);
			}
			continue;
		}
		if(f < g) {
			if(xsub1(s, NSIGNIF)) {
				/* lost a digit */
				e++;
				fmtexp(s+NSIGNIF, e);
			}
			continue;
		}
		break;
	}

	/*
	 * play with the decimal to try to simplify.
	 */

	/*
	 * bump last few digits up to 9 if we can
	 */
	for(i=NSIGNIF-1; i>=NSIGNIF-3; i--) {
		c = s[i];
		if(c != '9') {
			s[i] = '9';
			g = fz_strtod(s, NULL);
			if(g != f) {
				s[i] = c;
				break;
			}
		}
	}

	/*
	 * add 1 in hopes of turning 9s to 0s
	 */
	if(s[NSIGNIF-1] == '9') {
		strcpy(tmp, s);
		ee = e;
		if(xadd1(tmp, NSIGNIF)) {
			ee--;
			fmtexp(tmp+NSIGNIF, ee);
		}
		g = fz_strtod(tmp, NULL);
		if(g == f) {
			strcpy(s, tmp);
			e = ee;
		}
	}

	/*
	 * bump last few digits down to 0 as we can.
	 */
	for(i=NSIGNIF-1; i>=NSIGNIF-3; i--) {
		c = s[i];
		if(c != '0') {
			s[i] = '0';
			g = fz_strtod(s, NULL);
			if(g != f) {
				s[i] = c;
				break;
			}
		}
	}

	/*
	 * remove trailing zeros.
	 */
	ndigit = NSIGNIF;
	while(ndigit > 1 && s[ndigit-1] == '0'){
		e++;
		--ndigit;
	}
	s[ndigit] = 0;
	*exp = e;
	*ns = ndigit;
	errno = oerrno;
}
