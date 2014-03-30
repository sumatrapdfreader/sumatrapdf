#include "mupdf/fitz.h"

struct fmtbuf
{
	char *p;
	int s;
	int n;
};

static void fmtputc(struct fmtbuf *out, int c)
{
	if (out->n < out->s)
		out->p[out->n] = c;
	++(out->n);
}

/*
 * Compute decimal integer m, exp such that:
 *	f = m*10^exp
 *	m is as short as possible with losing exactness
 * assumes special cases (NaN, +Inf, -Inf) have been handled.
 */
static void fz_dtoa(float f, char *digits, int *exp, int *neg, int *ndigits)
{
	char buf[20], *s, *p, *e;
	int n;

	/* TODO: binary search */
	for (n = 1; n < 9; ++n) {
		sprintf(buf, "%+.*e", n, f);
		if (strtof(buf, NULL) == f)
			break;
	}

	*neg = (buf[0] == '-');

	p = buf + 3;
	e = strchr(p, 'e');
	*exp = atoi(e + 1) - (e - p);

	if (e[-1] == '0') {
		--e;
		++(*exp);
	}

	s = digits;
	*s++ = buf[1];
	while (p < e)
		*s++ = *p++;
	*s = 0;

	*ndigits = s - digits;
}

/*
 * Convert float to shortest possible string that won't lose precision, except:
 * NaN to 0, +Inf to FLT_MAX, -Inf to -FLT_MAX.
 */
static void fmtfloat(struct fmtbuf *out, float f)
{
	char digits[40], *s = digits;
	int exp, neg, ndigits, point;

	if (isnan(f)) f = 0;
	if (isinf(f)) f = f < 0 ? -FLT_MAX : FLT_MAX;

	fz_dtoa(f, digits, &exp, &neg, &ndigits);
	point = exp + ndigits;

	if (neg)
		fmtputc(out, '-');

	if (point <= 0)
	{
		fmtputc(out, '.');
		while (point++ < 0)
			fmtputc(out, '0');
		while (ndigits-- > 0)
			fmtputc(out, *s++);
	}

	else
	{
		while (ndigits-- > 0)
		{
			fmtputc(out, *s++);
			if (--point == 0 && ndigits > 0)
				fmtputc(out, '.');
		}
		while (point-- > 0)
			fmtputc(out, '0');
	}
}

static void fmtint(struct fmtbuf *out, int value, int z, int base)
{
	static const char *digits = "0123456789abcdef";
	char buf[40];
	unsigned int a;
	int i;

	if (value < 0)
	{
		fmtputc(out, '-');
		a = -value;
	}
	else
		a = value;

	i = 0;
	while (a) {
		buf[i++] = digits[a % base];
		a /= base;
	}
	while (i < z)
		buf[i++] = '0';
	while (i > 0)
		fmtputc(out, buf[--i]);
}

static void fmtquote(struct fmtbuf *out, const char *s, int sq, int eq)
{
	int c;
	fmtputc(out, sq);
	while ((c = *s++) != 0) {
		switch (c) {
		default:
			if (c < 32 || c > 127) {
				fmtputc(out, '\\');
				fmtputc(out, '0' + ((c >> 6) & 7));
				fmtputc(out, '0' + ((c >> 3) & 7));
				fmtputc(out, '0' + ((c) & 7));
			} else {
				if (c == sq || c == eq)
					fmtputc(out, '\\');
				fmtputc(out, c);
			}
			break;
		case '\\': fmtputc(out, '\\'); fmtputc(out, '\\'); break;
		case '\b': fmtputc(out, '\\'); fmtputc(out, 'b'); break;
		case '\f': fmtputc(out, '\\'); fmtputc(out, 'f'); break;
		case '\n': fmtputc(out, '\\'); fmtputc(out, 'n'); break;
		case '\r': fmtputc(out, '\\'); fmtputc(out, 'r'); break;
		case '\t': fmtputc(out, '\\'); fmtputc(out, 't'); break;
		}
	}
	fmtputc(out, eq);
}

int
fz_vsnprintf(char *buffer, int space, const char *fmt, va_list args)
{
	struct fmtbuf out;
	fz_matrix *m;
	fz_rect *r;
	fz_point *p;
	int c, i, n, z;
	double f;
	char *s;

	out.p = buffer;
	out.s = space;
	out.n = 0;

	while ((c = *fmt++) != 0)
	{
		if (c == '%') {
			c = *fmt++;
			if (c == 0)
				break;
			z = 1;
			if (c == '0' && fmt[0] && fmt[1]) {
				z = *fmt++ - '0';
				c = *fmt++;
			}
			switch (c) {
			default:
				fmtputc(&out, '%');
				fmtputc(&out, c);
				break;
			case '%':
				fmtputc(&out, '%');
				break;
			case 'M':
				m = va_arg(args, fz_matrix*);
				fmtfloat(&out, m->a); fmtputc(&out, ' ');
				fmtfloat(&out, m->b); fmtputc(&out, ' ');
				fmtfloat(&out, m->c); fmtputc(&out, ' ');
				fmtfloat(&out, m->d); fmtputc(&out, ' ');
				fmtfloat(&out, m->e); fmtputc(&out, ' ');
				fmtfloat(&out, m->f);
				break;
			case 'R':
				r = va_arg(args, fz_rect*);
				fmtfloat(&out, r->x0); fmtputc(&out, ' ');
				fmtfloat(&out, r->y0); fmtputc(&out, ' ');
				fmtfloat(&out, r->x1); fmtputc(&out, ' ');
				fmtfloat(&out, r->y1);
				break;
			case 'P':
				p = va_arg(args, fz_point*);
				fmtfloat(&out, p->x); fmtputc(&out, ' ');
				fmtfloat(&out, p->y);
				break;
			case 'C':
				c = va_arg(args, int);
				if (c < 128)
					fmtputc(&out, c);
				else {
					char buf[10];
					n = fz_runetochar(buf, c);
					for (i=0; i < n; ++i)
						fmtputc(&out, buf[i]);
				}
				break;
			case 'c':
				c = va_arg(args, int);
				fmtputc(&out, c);
				break;
			case 'f':
			case 'g':
				f = va_arg(args, double);
				fmtfloat(&out, f);
				break;
			case 'x':
				i = va_arg(args, int);
				fmtint(&out, i, z, 16);
				break;
			case 'd':
				i = va_arg(args, int);
				fmtint(&out, i, z, 10);
				break;
			case 'o':
				i = va_arg(args, int);
				fmtint(&out, i, z, 8);
				break;
			case 's':
				s = va_arg(args, char*);
				if (!s)
					s = "(null)";
				while ((c = *s++) != 0)
					fmtputc(&out, c);
				break;
			case 'q':
				s = va_arg(args, char*);
				if (!s) s = "";
				fmtquote(&out, s, '"', '"');
				break;
			case '(':
				s = va_arg(args, char*);
				if (!s) s = "";
				fmtquote(&out, s, '(', ')');
				break;
			}
		} else {
			fmtputc(&out, c);
		}
	}

	fmtputc(&out, 0);
	return out.n - 1;
}

int
fz_vfprintf(fz_context *ctx, FILE *file, const char *fmt, va_list old_args)
{
	char buffer[256];
	int l;
	va_list args;
	char *b = buffer;

	/* First try using our fixed size buffer */
	va_copy(args, old_args);
	l = fz_vsnprintf(buffer, sizeof buffer, fmt, args);
	va_copy_end(args);

	/* If that failed, allocate the right size buffer dynamically */
	if (l >= sizeof buffer)
	{
		b = fz_malloc(ctx, l + 1);
		va_copy(args, old_args);
		fz_vsnprintf(buffer, l + 1, fmt, args);
		va_copy_end(args);
	}

	l = fwrite(b, 1, l, file);

	if (b != buffer)
		fz_free(ctx, b);

	return l;
}
