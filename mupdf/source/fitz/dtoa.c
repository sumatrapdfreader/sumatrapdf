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

typedef unsigned long ulong;

enum { NSIGNIF	= 17 };

/*
 * first few powers of 10, enough for about 1/2 of the
 * total space for doubles.
 */
static double pows10[] =
{
	1e0,	1e1,	1e2,	1e3,	1e4,	1e5,	1e6,	1e7,	1e8,	1e9,
	1e10,	1e11,	1e12,	1e13,	1e14,	1e15,	1e16,	1e17,	1e18,	1e19,
	1e20,	1e21,	1e22,	1e23,	1e24,	1e25,	1e26,	1e27,	1e28,	1e29,
	1e30,	1e31,	1e32,	1e33,	1e34,	1e35,	1e36,	1e37,	1e38,	1e39,
	1e40,	1e41,	1e42,	1e43,	1e44,	1e45,	1e46,	1e47,	1e48,	1e49,
	1e50,	1e51,	1e52,	1e53,	1e54,	1e55,	1e56,	1e57,	1e58,	1e59,
	1e60,	1e61,	1e62,	1e63,	1e64,	1e65,	1e66,	1e67,	1e68,	1e69,
	1e70,	1e71,	1e72,	1e73,	1e74,	1e75,	1e76,	1e77,	1e78,	1e79,
	1e80,	1e81,	1e82,	1e83,	1e84,	1e85,	1e86,	1e87,	1e88,	1e89,
	1e90,	1e91,	1e92,	1e93,	1e94,	1e95,	1e96,	1e97,	1e98,	1e99,
	1e100,	1e101,	1e102,	1e103,	1e104,	1e105,	1e106,	1e107,	1e108,	1e109,
	1e110,	1e111,	1e112,	1e113,	1e114,	1e115,	1e116,	1e117,	1e118,	1e119,
	1e120,	1e121,	1e122,	1e123,	1e124,	1e125,	1e126,	1e127,	1e128,	1e129,
	1e130,	1e131,	1e132,	1e133,	1e134,	1e135,	1e136,	1e137,	1e138,	1e139,
	1e140,	1e141,	1e142,	1e143,	1e144,	1e145,	1e146,	1e147,	1e148,	1e149,
	1e150,	1e151,	1e152,	1e153,	1e154,	1e155,	1e156,	1e157,	1e158,	1e159,
};
#define	npows10 ((int)(sizeof(pows10)/sizeof(pows10[0])))
#define	pow10(x) fmtpow10(x)

static double
pow10(int n)
{
	double d;
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
fz_dtoa(double f, char *s, int *exp, int *neg, int *ns)
{
	int c, d, e2, e, ee, i, ndigit, oerrno;
	char tmp[NSIGNIF+10];
	double g;

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
	frexp(f, &e2);
	e = (int)(e2 * .301029995664);
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

static ulong
umuldiv(ulong a, ulong b, ulong c)
{
	double d;

	d = ((double)a * (double)b) / (double)c;
	if(d >= 4294967295.)
		d = 4294967295.;
	return (ulong)d;
}

/*
 * This routine will convert to arbitrary precision
 * floating point entirely in multi-precision fixed.
 * The answer is the closest floating point number to
 * the given decimal number. Exactly half way are
 * rounded ala ieee rules.
 * Method is to scale input decimal between .500 and .999...
 * with external power of 2, then binary search for the
 * closest mantissa to this decimal number.
 * Nmant is is the required precision. (53 for ieee dp)
 * Nbits is the max number of bits/word. (must be <= 28)
 * Prec is calculated - the number of words of fixed mantissa.
 */
enum
{
	Nbits	= 28,				/* bits safely represented in a ulong */
	Nmant	= 53,				/* bits of precision required */
	Prec	= (Nmant+Nbits+1)/Nbits,	/* words of Nbits each to represent mantissa */
	Sigbit	= 1<<(Prec*Nbits-Nmant),	/* first significant bit of Prec-th word */
	Ndig	= 1500,
	One	= (ulong)(1<<Nbits),
	Half	= (ulong)(One>>1),
	Maxe	= 310,

	Fsign	= 1<<0,		/* found - */
	Fesign	= 1<<1,		/* found e- */
	Fdpoint	= 1<<2,		/* found . */

	S0	= 0,		/* _		_S0	+S1	#S2	.S3 */
	S1,			/* _+		#S2	.S3 */
	S2,			/* _+#		#S2	.S4	eS5 */
	S3,			/* _+.		#S4 */
	S4,			/* _+#.#	#S4	eS5 */
	S5,			/* _+#.#e	+S6	#S7 */
	S6,			/* _+#.#e+	#S7 */
	S7			/* _+#.#e+#	#S7 */
};

static	int	xcmp(char*, char*);
static	int	fpcmp(char*, ulong*);
static	void	frnorm(ulong*);
static	void	divascii(char*, int*, int*, int*);
static	void	mulascii(char*, int*, int*, int*);

typedef	struct	Tab	Tab;
struct	Tab
{
	int	bp;
	int	siz;
	char*	cmp;
};

double
fz_strtod(const char *as, char **aas)
{
	int na, ex, dp, bp, c, i, flag, state;
	ulong low[Prec], hig[Prec], mid[Prec];
	double d;
	char *s, a[Ndig];

	flag = 0;	/* Fsign, Fesign, Fdpoint */
	na = 0;		/* number of digits of a[] */
	dp = 0;		/* na of decimal point */
	ex = 0;		/* exonent */

	state = S0;
	for(s=(char*)as;; s++) {
		c = *s;
		if(c >= '0' && c <= '9') {
			switch(state) {
			case S0:
			case S1:
			case S2:
				state = S2;
				break;
			case S3:
			case S4:
				state = S4;
				break;

			case S5:
			case S6:
			case S7:
				state = S7;
				ex = ex*10 + (c-'0');
				continue;
			}
			if(na == 0 && c == '0') {
				dp--;
				continue;
			}
			if(na < Ndig-50)
				a[na++] = c;
			continue;
		}
		switch(c) {
		case '\t':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
			if(state == S0)
				continue;
			break;
		case '-':
			if(state == S0)
				flag |= Fsign;
			else
				flag |= Fesign;
		case '+':
			if(state == S0)
				state = S1;
			else
			if(state == S5)
				state = S6;
			else
				break;	/* syntax */
			continue;
		case '.':
			flag |= Fdpoint;
			dp = na;
			if(state == S0 || state == S1) {
				state = S3;
				continue;
			}
			if(state == S2) {
				state = S4;
				continue;
			}
			break;
		case 'e':
		case 'E':
			if(state == S2 || state == S4) {
				state = S5;
				continue;
			}
			break;
		}
		break;
	}

	/*
	 * clean up return char-pointer
	 */
	switch(state) {
	case S0:
		if(xcmp(s, "nan") == 0) {
			if(aas != NULL)
				*aas = s+3;
			goto retnan;
		}
	case S1:
		if(xcmp(s, "infinity") == 0) {
			if(aas != NULL)
				*aas = s+8;
			goto retinf;
		}
		if(xcmp(s, "inf") == 0) {
			if(aas != NULL)
				*aas = s+3;
			goto retinf;
		}
	case S3:
		if(aas != NULL)
			*aas = (char*)as;
		goto ret0;	/* no digits found */
	case S6:
		s--;		/* back over +- */
	case S5:
		s--;		/* back over e */
		break;
	}
	if(aas != NULL)
		*aas = s;

	if(flag & Fdpoint)
	while(na > 0 && a[na-1] == '0')
		na--;
	if(na == 0)
		goto ret0;	/* zero */
	a[na] = 0;
	if(!(flag & Fdpoint))
		dp = na;
	if(flag & Fesign)
		ex = -ex;
	dp += ex;
	if(dp < -Maxe){
		errno = ERANGE;
		goto ret0;	/* underflow by exp */
	} else
	if(dp > +Maxe)
		goto retinf;	/* overflow by exp */

	/*
	 * normalize the decimal ascii number
	 * to range .[5-9][0-9]* e0
	 */
	bp = 0;		/* binary exponent */
	while(dp > 0)
		divascii(a, &na, &dp, &bp);
	while(dp < 0 || a[0] < '5')
		mulascii(a, &na, &dp, &bp);

	/* close approx by naive conversion */
	mid[0] = 0;
	mid[1] = 1;
	for(i=0; (c=a[i]) != '\0'; i++) {
		mid[0] = mid[0]*10 + (c-'0');
		mid[1] = mid[1]*10;
		if(i >= 8)
			break;
	}
	low[0] = umuldiv(mid[0], One, mid[1]);
	hig[0] = umuldiv(mid[0]+1, One, mid[1]);
	for(i=1; i<Prec; i++) {
		low[i] = 0;
		hig[i] = One-1;
	}

	/* binary search for closest mantissa */
	for(;;) {
		/* mid = (hig + low) / 2 */
		c = 0;
		for(i=0; i<Prec; i++) {
			mid[i] = hig[i] + low[i];
			if(c)
				mid[i] += One;
			c = mid[i] & 1;
			mid[i] >>= 1;
		}
		frnorm(mid);

		/* compare */
		c = fpcmp(a, mid);
		if(c > 0) {
			c = 1;
			for(i=0; i<Prec; i++)
				if(low[i] != mid[i]) {
					c = 0;
					low[i] = mid[i];
				}
			if(c)
				break;	/* between mid and hig */
			continue;
		}
		if(c < 0) {
			for(i=0; i<Prec; i++)
				hig[i] = mid[i];
			continue;
		}

		/* only hard part is if even/odd roundings wants to go up */
		c = mid[Prec-1] & (Sigbit-1);
		if(c == Sigbit/2 && (mid[Prec-1]&Sigbit) == 0)
			mid[Prec-1] -= c;
		break;	/* exactly mid */
	}

	/* normal rounding applies */
	c = mid[Prec-1] & (Sigbit-1);
	mid[Prec-1] -= c;
	if(c >= Sigbit/2) {
		mid[Prec-1] += Sigbit;
		frnorm(mid);
	}
	goto out;

ret0:
	return 0;

retnan:
	return NAN;

retinf:
	/*
	 * Unix strtod requires these. Plan 9 would return Inf(0) or Inf(-1). */
	errno = ERANGE;
	if(flag & Fsign)
		return -HUGE_VAL;
	return HUGE_VAL;

out:
	d = 0;
	for(i=0; i<Prec; i++)
		d = d*One + mid[i];
	if(flag & Fsign)
		d = -d;
	d = ldexp(d, bp - Prec*Nbits);
	if(d == 0){	/* underflow */
		errno = ERANGE;
	}
	return d;
}

static void
frnorm(ulong *f)
{
	int i, c;

	c = 0;
	for(i=Prec-1; i>0; i--) {
		f[i] += c;
		c = f[i] >> Nbits;
		f[i] &= One-1;
	}
	f[0] += c;
}

static int
fpcmp(char *a, ulong* f)
{
	ulong tf[Prec];
	int i, d, c;

	for(i=0; i<Prec; i++)
		tf[i] = f[i];

	for(;;) {
		/* tf *= 10 */
		for(i=0; i<Prec; i++)
			tf[i] = tf[i]*10;
		frnorm(tf);
		d = (tf[0] >> Nbits) + '0';
		tf[0] &= One-1;

		/* compare next digit */
		c = *a;
		if(c == 0) {
			if('0' < d)
				return -1;
			if(tf[0] != 0)
				goto cont;
			for(i=1; i<Prec; i++)
				if(tf[i] != 0)
					goto cont;
			return 0;
		}
		if(c > d)
			return +1;
		if(c < d)
			return -1;
		a++;
	cont:;
	}
}

static void
divby(char *a, int *na, int b)
{
	int n, c;
	char *p;

	p = a;
	n = 0;
	while(n>>b == 0) {
		c = *a++;
		if(c == 0) {
			while(n) {
				c = n*10;
				if(c>>b)
					break;
				n = c;
			}
			goto xx;
		}
		n = n*10 + c-'0';
		(*na)--;
	}
	for(;;) {
		c = n>>b;
		n -= c<<b;
		*p++ = c + '0';
		c = *a++;
		if(c == 0)
			break;
		n = n*10 + c-'0';
	}
	(*na)++;
xx:
	while(n) {
		n = n*10;
		c = n>>b;
		n -= c<<b;
		*p++ = c + '0';
		(*na)++;
	}
	*p = 0;
}

static	Tab	tab1[] =
{
	{ 1, 0, "" },
	{ 3, 1, "7" },
	{ 6, 2, "63" },
	{ 9, 3, "511" },
	{ 13, 4, "8191" },
	{ 16, 5, "65535" },
	{ 19, 6, "524287" },
	{ 23, 7, "8388607" },
	{ 26, 8, "67108863" },
	{ 27, 9, "134217727" },
};

static void
divascii(char *a, int *na, int *dp, int *bp)
{
	int b, d;
	Tab *t;

	d = *dp;
	if(d >= (int)(nelem(tab1)))
		d = (int)(nelem(tab1))-1;
	t = tab1 + d;
	b = t->bp;
	if(memcmp(a, t->cmp, t->siz) > 0)
		d--;
	*dp -= d;
	*bp += b;
	divby(a, na, b);
}

static void
mulby(char *a, char *p, char *q, int b)
{
	int n, c;

	n = 0;
	*p = 0;
	for(;;) {
		q--;
		if(q < a)
			break;
		c = *q - '0';
		c = (c<<b) + n;
		n = c/10;
		c -= n*10;
		p--;
		*p = c + '0';
	}
	while(n) {
		c = n;
		n = c/10;
		c -= n*10;
		p--;
		*p = c + '0';
	}
}

static	Tab	tab2[] =
{
	{ 1, 1, "" },				/* dp = 0-0 */
	{ 3, 3, "125" },
	{ 6, 5, "15625" },
	{ 9, 7, "1953125" },
	{ 13, 10, "1220703125" },
	{ 16, 12, "152587890625" },
	{ 19, 14, "19073486328125" },
	{ 23, 17, "11920928955078125" },
	{ 26, 19, "1490116119384765625" },
	{ 27, 19, "7450580596923828125" },		/* dp 8-9 */
};

static void
mulascii(char *a, int *na, int *dp, int *bp)
{
	char *p;
	int d, b;
	Tab *t;

	d = -*dp;
	if(d >= (int)(nelem(tab2)))
		d = (int)(nelem(tab2))-1;
	t = tab2 + d;
	b = t->bp;
	if(memcmp(a, t->cmp, t->siz) < 0)
		d--;
	p = a + *na;
	*bp -= b;
	*dp += d;
	*na += d;
	mulby(a, p+d, p, b);
}

static int
xcmp(char *a, char *b)
{
	int c1, c2;

	while((c1 = *b++) != '\0') {
		c2 = *a++;
		if(c2 >= 'A' && c2 <= 'Z')
			c2 = c2 - 'A' + 'a';
		if(c1 != c2)
			return 1;
	}
	return 0;
}
