#include "fitz.h"
#include "mupdf.h"

/* this mess is seokgyo's doing */

enum
{
	MAXN = FZ_MAXCOLORS,
	MAXM = FZ_MAXCOLORS,
};

typedef struct psobj_s psobj;

enum
{
	SAMPLE = 0,
	EXPONENTIAL = 2,
	STITCHING = 3,
	POSTSCRIPT = 4
};

struct pdf_function_s
{
	int refs;
	int type;				/* 0=sample 2=exponential 3=stitching 4=postscript */
	int m;					/* number of input values */
	int n;					/* number of output values */
	float domain[MAXM][2];	/* even index : min value, odd index : max value */
	float range[MAXN][2];	/* even index : min value, odd index : max value */
	int hasrange;

	union
	{
		struct {
			unsigned short bps;
			int size[MAXM];
			float encode[MAXM][2];
			float decode[MAXN][2];
			int *samples;
		} sa;

		struct {
			float n;
			float c0[MAXN];
			float c1[MAXN];
		} e;

		struct {
			int k;
			pdf_function **funcs; /* k */
			float *bounds; /* k - 1 */
			float *encode; /* k * 2 */
		} st;

		struct {
			psobj *code;
			int cap;
		} p;
	} u;
};

/*
 * PostScript calculator
 */

#define RADIAN 57.2957795

#define LERP(x, xmin, xmax, ymin, ymax) \
	(ymin) + ((x) - (xmin)) * ((ymax) - (ymin)) / ((xmax) - (xmin))

enum { PSBOOL, PSINT, PSREAL, PSOPERATOR, PSBLOCK };

enum
{
	PSOABS, PSOADD, PSOAND, PSOATAN, PSOBITSHIFT, PSOCEILING,
	PSOCOPY, PSOCOS, PSOCVI, PSOCVR, PSODIV, PSODUP, PSOEQ,
	PSOEXCH, PSOEXP, PSOFALSE, PSOFLOOR, PSOGE, PSOGT, PSOIDIV,
	PSOINDEX, PSOLE, PSOLN, PSOLOG, PSOLT, PSOMOD, PSOMUL,
	PSONE, PSONEG, PSONOT, PSOOR, PSOPOP, PSOROLL, PSOROUND,
	PSOSIN, PSOSQRT, PSOSUB, PSOTRUE, PSOTRUNCATE, PSOXOR,
	PSOIF, PSOIFELSE, PSORETURN
};

static char *psopnames[] =
{
	"abs", "add", "and", "atan", "bitshift", "ceiling", "copy",
	"cos", "cvi", "cvr", "div", "dup", "eq", "exch", "exp",
	"false", "floor", "ge", "gt", "idiv", "index", "le", "ln",
	"log", "lt", "mod", "mul", "ne", "neg", "not", "or", "pop",
	"roll", "round", "sin", "sqrt", "sub", "true", "truncate",
	"xor", "if", "ifelse", "return"
};

struct psobj_s
{
	int type;
	union
	{
		int b;				/* boolean (stack only) */
		int i;				/* integer (stack and code) */
		float f;			/* real (stack and code) */
		int op;				/* operator (code only) */
		int block;			/* if/ifelse block pointer (code only) */
	} u;
};

enum { PSSTACKSIZE = 100 };

#define fz_stackoverflow fz_throw("stack overflow in calculator function")
#define fz_stackunderflow fz_throw("stack underflow in calculator function")
#define fz_stacktypemismatch fz_throw("type mismatch in calculator function")

typedef struct psstack_s psstack;

struct psstack_s
{
	psobj stack[PSSTACKSIZE];
	int sp;
};

static void
psinitstack(psstack *st)
{
	memset(st->stack, 0, sizeof(st->stack));
	st->sp = PSSTACKSIZE;
}

static int
pscheckoverflow(psstack *st, int n)
{
	return st->sp >= n;
}

static int
pscheckunderflow(psstack *st)
{
	return st->sp != PSSTACKSIZE;
}

static int
pschecktype(psstack *st, unsigned short type)
{
	return st->stack[st->sp].type == type;
}

static fz_error
pspushbool(psstack *st, int booln)
{
	if (!pscheckoverflow(st, 1))
		return fz_stackoverflow;
	st->stack[--st->sp].type = PSBOOL;
	st->stack[st->sp].u.b = booln;
	return fz_okay;
}

static fz_error
pspushint(psstack *st, int intg)
{
	if (!pscheckoverflow(st, 1))
		return fz_stackoverflow;
	st->stack[--st->sp].type = PSINT;
	st->stack[st->sp].u.i = intg;
	return fz_okay;
}

static fz_error
pspushreal(psstack *st, float real)
{
	if (!pscheckoverflow(st, 1))
		return fz_stackoverflow;
	st->stack[--st->sp].type = PSREAL;
	st->stack[st->sp].u.f = real;
	return fz_okay;
}

static fz_error
pspopbool(psstack *st, int *booln)
{
	if (!pscheckunderflow(st))
		return fz_stackunderflow;
	if (!pschecktype(st, PSBOOL))
		return fz_stacktypemismatch;
	*booln = st->stack[st->sp++].u.b;
	return fz_okay;
}

static fz_error
pspopint(psstack *st, int *intg)
{
	if (!pscheckunderflow(st))
		return fz_stackunderflow;
	if (!pschecktype(st, PSINT))
		return fz_stacktypemismatch;
	*intg = st->stack[st->sp++].u.i;
	return fz_okay;
}

static fz_error
pspopnum(psstack *st, float *real)
{
	if (!pscheckunderflow(st))
		return fz_stackunderflow;
	if (!pschecktype(st, PSINT) && !pschecktype(st, PSREAL))
		return fz_stacktypemismatch;
	*real = (st->stack[st->sp].type == PSINT) ?
	st->stack[st->sp].u.i : st->stack[st->sp].u.f;
	++st->sp;
	return fz_okay;
}

static int
pstopisint(psstack *st)
{
	return st->sp < PSSTACKSIZE && st->stack[st->sp].type == PSINT;
}

static int
pstoptwoareints(psstack *st)
{
	return st->sp < PSSTACKSIZE - 1 &&
		st->stack[st->sp].type == PSINT &&
		st->stack[st->sp + 1].type == PSINT;
}

static int
pstopisreal(psstack *st)
{
	return st->sp < PSSTACKSIZE && st->stack[st->sp].type == PSREAL;
}

static int
pstoptwoarenums(psstack *st)
{
	return st->sp < PSSTACKSIZE - 1 &&
		(st->stack[st->sp].type == PSINT || st->stack[st->sp].type == PSREAL) &&
		(st->stack[st->sp + 1].type == PSINT || st->stack[st->sp + 1].type == PSREAL);
}

static fz_error
pscopy(psstack *st, int n)
{
	int i;

	if (!pscheckoverflow(st, n))
		return fz_stackoverflow;

	for (i = 0; i < n; i++)
	{
		st->stack[st->sp - n + i] = st->stack[st->sp + i];
	}
	st->sp -= n;

	return fz_okay;
}

static void
psroll(psstack *st, int n, int j)
{
	psobj obj;
	int i, k;

	if (j >= 0)
	{
		j %= n;
	}
	else
	{
		j = -j % n;
		if (j != 0)
		{
			j = n - j;
		}
	}
	if (n <= 0 || j == 0)
	{
		return;
	}
	for (i = 0; i < j; ++i)
	{
		/* FIXME check for underflow? */
		obj = st->stack[st->sp];
		for (k = st->sp; k < st->sp + n - 1; ++k)
		{
			st->stack[k] = st->stack[k + 1];
		}
		st->stack[st->sp + n - 1] = obj;
	}
}

static fz_error
psindex(psstack *st, int i)
{
	if (!pscheckoverflow(st, 1))
		return fz_stackoverflow;
	--st->sp;
	st->stack[st->sp] = st->stack[st->sp + 1 + i];
	return fz_okay;
}

static fz_error
pspop(psstack *st)
{
	if (!pscheckoverflow(st, 1))
		return fz_stackoverflow;
	++st->sp;
	return fz_okay;
}

static void
resizecode(pdf_function *func, int newsize)
{
	if (newsize >= func->u.p.cap)
	{
		func->u.p.cap = func->u.p.cap + 64;
		func->u.p.code = fz_realloc(func->u.p.code, func->u.p.cap * sizeof(psobj));
	}
}

static fz_error
parsecode(pdf_function *func, fz_stream *stream, int *codeptr)
{
	fz_error error;
	char buf[64];
	int len;
	pdf_token_e tok;
	int opptr, elseptr;
	int a, b, mid, cmp;

	memset(buf, 0, sizeof(buf));

	while (1)
	{
		error = pdf_lex(&tok, stream, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "calculator function lexical error");

		switch(tok)
		{
		case PDF_TEOF:
			return fz_throw("truncated calculator function");

		case PDF_TINT:
			resizecode(func, *codeptr);
			func->u.p.code[*codeptr].type = PSINT;
			func->u.p.code[*codeptr].u.i = atoi(buf);
			++*codeptr;
			break;

		case PDF_TREAL:
			resizecode(func, *codeptr);
			func->u.p.code[*codeptr].type = PSREAL;
			func->u.p.code[*codeptr].u.f = atof(buf);
			++*codeptr;
			break;

		case PDF_TOBRACE:
			opptr = *codeptr;
			*codeptr += 3;

			resizecode(func, opptr + 2);

			error = parsecode(func, stream, codeptr);
			if (error)
				return fz_rethrow(error, "error in 'if' branch");

			error = pdf_lex(&tok, stream, buf, sizeof buf, &len);
			if (error)
				return fz_rethrow(error, "calculator function syntax error");

			if (tok == PDF_TOBRACE)
			{
				elseptr = *codeptr;
				error = parsecode(func, stream, codeptr);
				if (error)
					return fz_rethrow(error, "error in 'else' branch");

				error = pdf_lex(&tok, stream, buf, sizeof buf, &len);
				if (error)
					return fz_rethrow(error, "calculator function syntax error");
			}
			else
			{
				elseptr = -1;
			}

			if (tok == PDF_TKEYWORD)
			{
				if (!strcmp(buf, "if"))
				{
					if (elseptr >= 0)
						return fz_throw("too many branches for 'if'");
					func->u.p.code[opptr].type = PSOPERATOR;
					func->u.p.code[opptr].u.op = PSOIF;
					func->u.p.code[opptr+2].type = PSBLOCK;
					func->u.p.code[opptr+2].u.block = *codeptr;
				}
				else if (!strcmp(buf, "ifelse"))
				{
					if (elseptr < 0)
						return fz_throw("not enough branches for 'ifelse'");
					func->u.p.code[opptr].type = PSOPERATOR;
					func->u.p.code[opptr].u.op = PSOIFELSE;
					func->u.p.code[opptr+1].type = PSBLOCK;
					func->u.p.code[opptr+1].u.block = elseptr;
					func->u.p.code[opptr+2].type = PSBLOCK;
					func->u.p.code[opptr+2].u.block = *codeptr;
				}
				else
				{
					return fz_throw("unknown keyword in 'if-else' context: '%s'", buf);
				}
			}
			else
			{
				return fz_throw("missing keyword in 'if-else' context");
			}
			break;

		case PDF_TCBRACE:
			resizecode(func, *codeptr);
			func->u.p.code[*codeptr].type = PSOPERATOR;
			func->u.p.code[*codeptr].u.op = PSORETURN;
			++*codeptr;
			return fz_okay;

		case PDF_TKEYWORD:
			cmp = -1;
			a = -1;
			b = sizeof(psopnames) / sizeof(psopnames[0]);
			while (b - a > 1)
			{
				mid = (a + b) / 2;
				cmp = strcmp(buf, psopnames[mid]);
				if (cmp > 0)
					a = mid;
				else if (cmp < 0)
					b = mid;
				else
					a = b = mid;
			}
			if (cmp != 0)
				return fz_throw("unknown operator: '%s'", buf);

			resizecode(func, *codeptr);
			func->u.p.code[*codeptr].type = PSOPERATOR;
			func->u.p.code[*codeptr].u.op = a;
			++*codeptr;
			break;

		default:
			return fz_throw("calculator function syntax error");
		}
	}
}

static fz_error
loadpostscriptfunc(pdf_function *func, pdf_xref *xref, fz_obj *dict, int oid, int gen)
{
	fz_error error;
	fz_stream *stream;
	int codeptr;

	pdf_logrsrc("load postscript function (%d %d R)\n", oid, gen);

	error = pdf_openstream(&stream, xref, oid, gen);
	if (error)
		return fz_rethrow(error, "cannot open calculator function stream");

	if (fz_readbyte(stream) != '{')
	{
		fz_dropstream(stream);
		return fz_throw("stream is not a calculator function");
	}

	func->u.p.code = nil;
	func->u.p.cap = 0;

	codeptr = 0;
	error = parsecode(func, stream, &codeptr);
	if (error)
	{
		fz_dropstream(stream);
		return fz_rethrow(error, "cannot parse calculator function");
	}

	fz_dropstream(stream);
	return fz_okay;
}

#define SAFE_RETHROW            if (error) return fz_rethrow(error, "runtime error in calculator function")
#define SAFE_PUSHINT(st, a)	{ error = pspushint(st, a); SAFE_RETHROW; }
#define SAFE_PUSHREAL(st, a)	{ error = pspushreal(st, a); SAFE_RETHROW; }
#define SAFE_PUSHBOOL(st, a)	{ error = pspushbool(st, a); SAFE_RETHROW; }
#define SAFE_POPINT(st, a)	{ error = pspopint(st, a); SAFE_RETHROW; }
#define SAFE_POPNUM(st, a)	{ error = pspopnum(st, a); SAFE_RETHROW; }
#define SAFE_POPBOOL(st, a)	{ error = pspopbool(st, a); SAFE_RETHROW; }
#define SAFE_POP(st)		{ error = pspop(st); SAFE_RETHROW; }
#define SAFE_INDEX(st, i)	{ error = psindex(st, i); SAFE_RETHROW; }
#define SAFE_COPY(st, n)	{ error = pscopy(st, n); SAFE_RETHROW; }

static fz_error
evalpostscriptfunc(pdf_function *func, psstack *st, int codeptr)
{
	fz_error error;
	int i1, i2;
	float r1, r2;
	int b1, b2;

	while (1)
	{
		switch (func->u.p.code[codeptr].type)
		{
		case PSINT:
			SAFE_PUSHINT(st, func->u.p.code[codeptr++].u.i);
			break;

		case PSREAL:
			SAFE_PUSHREAL(st, func->u.p.code[codeptr++].u.f);
			break;

		case PSOPERATOR:
			switch (func->u.p.code[codeptr++].u.op)
			{
			case PSOABS:
				if (pstopisint(st)) {
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, abs(i1));
				}
				else {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, fabs(r1));
				}
				break;

			case PSOADD:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, i1 + i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, r1 + r2);
				}
				break;

			case PSOAND:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, i1 & i2);
				}
				else {
					SAFE_POPBOOL(st, &b2);
					SAFE_POPBOOL(st, &b1);
					SAFE_PUSHBOOL(st, b1 && b2);
				}
				break;

			case PSOATAN:
				SAFE_POPNUM(st, &r2);
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, atan2(r1, r2)*RADIAN);
				break;

			case PSOBITSHIFT:
				SAFE_POPINT(st, &i2);
				SAFE_POPINT(st, &i1);
				if (i2 > 0) {
					SAFE_PUSHINT(st, i1 << i2);
				}
				else if (i2 < 0) {
					SAFE_PUSHINT(st, (int)((unsigned int)i1 >> i2));
				}
				else {
					SAFE_PUSHINT(st, i1);
				}
				break;

			case PSOCEILING:
				if (!pstopisint(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, ceil(r1));
				}
				break;

			case PSOCOPY:
				SAFE_POPINT(st, &i1);
				SAFE_COPY(st, i1);
				break;

			case PSOCOS:
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, cos(r1/RADIAN));
				break;

			case PSOCVI:
				if (!pstopisint(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHINT(st, (int)r1);
				}
				break;

			case PSOCVR:
				if (!pstopisreal(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, r1);
				}
				break;

			case PSODIV:
				SAFE_POPNUM(st, &r2);
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, r1 / r2);
				break;

			case PSODUP:
				SAFE_COPY(st, 1);
				break;

			case PSOEQ:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHBOOL(st, i1 == i2);
				}
				else if (pstoptwoarenums(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHBOOL(st, r1 == r2);
				}
				else {
					SAFE_POPBOOL(st, &b2);
					SAFE_POPBOOL(st, &b2);
					SAFE_PUSHBOOL(st, b1 == b2);
				}
				break;

			case PSOEXCH:
				psroll(st, 2, 1);
				break;

			case PSOEXP:
				SAFE_POPNUM(st, &r2);
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, pow(r1, r2));
				break;

			case PSOFALSE:
				SAFE_PUSHBOOL(st, 0);
				break;

			case PSOFLOOR:
				if (!pstopisint(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, floor(r1));
				}
				break;

			case PSOGE:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHBOOL(st, i1 >= i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHBOOL(st, r1 >= r2);
				}
				break;

			case PSOGT:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHBOOL(st, i1 > i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHBOOL(st, r1 > r2);
				}
				break;

			case PSOIDIV:
				SAFE_POPINT(st, &i2);
				SAFE_POPINT(st, &i1);
				SAFE_PUSHINT(st, i1 / i2);
				break;

			case PSOINDEX:
				SAFE_POPINT(st, &i1);
				SAFE_INDEX(st, i1);
				break;

			case PSOLE:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHBOOL(st, i1 <= i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHBOOL(st, r1 <= r2);
				}
				break;

			case PSOLN:
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, log(r1));
				break;

			case PSOLOG:
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, log10(r1));
				break;

			case PSOLT:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHBOOL(st, i1 < i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHBOOL(st, r1 < r2);
				}
				break;

			case PSOMOD:
				SAFE_POPINT(st, &i2);
				SAFE_POPINT(st, &i1);
				SAFE_PUSHINT(st, i1 % i2);
				break;

			case PSOMUL:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					/* FIXME should check for out-of-range, and push a real instead */
					SAFE_PUSHINT(st, i1 * i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, r1 * r2);
				}
				break;

			case PSONE:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHBOOL(st, i1 != i2);
				}
				else if (pstoptwoarenums(st)) {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHBOOL(st, r1 != r2);
				}
				else {
					SAFE_POPBOOL(st, &b2);
					SAFE_POPBOOL(st, &b1);
					SAFE_PUSHBOOL(st, b1 != b2);
				}
				break;

			case PSONEG:
				if (pstopisint(st)) {
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, -i1);
				}
				else {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, -r1);
				}
				break;

			case PSONOT:
				if (pstopisint(st)) {
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, ~i1);
				}
				else {
					SAFE_POPBOOL(st, &b1);
					SAFE_PUSHBOOL(st, !b1);
				}
				break;

			case PSOOR:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, i1 | i2);
				}
				else {
					SAFE_POPBOOL(st, &b2);
					SAFE_POPBOOL(st, &b1);
					SAFE_PUSHBOOL(st, b1 || b2);
				}
				break;

			case PSOPOP:
				SAFE_POP(st);
				break;

			case PSOROLL:
				SAFE_POPINT(st, &i2);
				SAFE_POPINT(st, &i1);
				psroll(st, i1, i2);
				break;

			case PSOROUND:
				if (!pstopisint(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, (r1 >= 0) ? floor(r1 + 0.5) : ceil(r1 - 0.5));
				}
				break;

			case PSOSIN:
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, sin(r1/RADIAN));
				break;

			case PSOSQRT:
				SAFE_POPNUM(st, &r1);
				SAFE_PUSHREAL(st, sqrt(r1));
				break;

			case PSOSUB:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, i1 - i2);
				}
				else {
					SAFE_POPNUM(st, &r2);
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, r1 - r2);
				}
				break;

			case PSOTRUE:
				SAFE_PUSHBOOL(st, 1);
				break;

			case PSOTRUNCATE:
				if (!pstopisint(st)) {
					SAFE_POPNUM(st, &r1);
					SAFE_PUSHREAL(st, (r1 >= 0) ? floor(r1) : ceil(r1));
				}
				break;

			case PSOXOR:
				if (pstoptwoareints(st)) {
					SAFE_POPINT(st, &i2);
					SAFE_POPINT(st, &i1);
					SAFE_PUSHINT(st, i1 ^ i2);
				}
				else {
					SAFE_POPBOOL(st, &b2);
					SAFE_POPBOOL(st, &b1);
					SAFE_PUSHBOOL(st, b1 ^ b2);
				}
				break;

			case PSOIF:
				SAFE_POPBOOL(st, &b1);
				if (b1) {
					error = evalpostscriptfunc(func, st, codeptr + 2);
					if (error)
						return fz_rethrow(error, "runtime error in if-branch");
				}
				codeptr = func->u.p.code[codeptr + 1].u.block;
				break;

			case PSOIFELSE:
				SAFE_POPBOOL(st, &b1);
				if (b1) {
					error = evalpostscriptfunc(func, st, codeptr + 2);
					if (error)
						return fz_rethrow(error, "runtime error in if-branch");
				}
				else {
					error = evalpostscriptfunc(func, st, func->u.p.code[codeptr].u.block);
					if (error)
						return fz_rethrow(error, "runtime error in else-branch");
				}
				codeptr = func->u.p.code[codeptr + 1].u.block;
				break;

			case PSORETURN:
				return fz_okay;

			default:
				return fz_throw("foreign operator in calculator function");
			}
			break;

		default:
			return fz_throw("foreign object in calculator function");
		}
	}
}

/*
 * Sample function
 */

static int bps_supported[] = { 1, 2, 4, 8, 12, 16, 24, 32 };

static fz_error
loadsamplefunc(pdf_function *func, pdf_xref *xref, fz_obj *dict, int oid, int gen)
{
	fz_error error;
	fz_stream *stream;
	fz_obj *obj;
	int samplecount;
	int bps;
	int i;

	pdf_logrsrc("sampled function {\n");

	func->u.sa.samples = nil;

	obj = fz_dictgets(dict, "Size");
	if (!fz_isarray(obj) || fz_arraylen(obj) != func->m)
		return fz_throw("malformed /Size");
	for (i = 0; i < func->m; ++i)
		func->u.sa.size[i] = fz_toint(fz_arrayget(obj, i));

	obj = fz_dictgets(dict, "BitsPerSample");
	if (!fz_isint(obj))
		return fz_throw("malformed /BitsPerSample");
	func->u.sa.bps = bps = fz_toint(obj);

	pdf_logrsrc("bps %d\n", bps);

	for (i = 0; i < nelem(bps_supported); ++i)
		if (bps == bps_supported[i])
			break;
	if (i == nelem(bps_supported))
		return fz_throw("unsupported BitsPerSample (%d)", bps);

	obj = fz_dictgets(dict, "Encode");
	if (fz_isarray(obj))
	{
		if (fz_arraylen(obj) != func->m * 2)
			return fz_throw("malformed /Encode");
		for (i = 0; i < func->m; ++i)
		{
			func->u.sa.encode[i][0] = fz_toreal(fz_arrayget(obj, i*2+0));
			func->u.sa.encode[i][1] = fz_toreal(fz_arrayget(obj, i*2+1));
		}
	}
	else
	{
		for (i = 0; i < func->m; ++i)
		{
			func->u.sa.encode[i][0] = 0;
			func->u.sa.encode[i][1] = func->u.sa.size[i] - 1;
		}
	}

	obj = fz_dictgets(dict, "Decode");
	if (fz_isarray(obj))
	{
		if (fz_arraylen(obj) != func->n * 2)
			return fz_throw("malformed /Decode");
		for (i = 0; i < func->n; ++i)
		{
			func->u.sa.decode[i][0] = fz_toreal(fz_arrayget(obj, i*2+0));
			func->u.sa.decode[i][1] = fz_toreal(fz_arrayget(obj, i*2+1));
		}
	}
	else
	{
		for (i = 0; i < func->n; ++i)
		{
			func->u.sa.decode[i][0] = func->range[i][0];
			func->u.sa.decode[i][1] = func->range[i][1];
		}
	}

	for (i = 0, samplecount = func->n; i < func->m; ++i)
		samplecount *= func->u.sa.size[i];

	pdf_logrsrc("samplecount %d\n", samplecount);

	func->u.sa.samples = fz_malloc(samplecount * sizeof(int));

	error = pdf_openstream(&stream, xref, oid, gen);
	if (error)
		return fz_rethrow(error, "cannot open samples stream");

	/* read samples */
	{
		unsigned int bitmask = (1 << bps) - 1;
		unsigned int buf = 0;
		int bits = 0;
		int s;

		for (i = 0; i < samplecount; ++i)
		{
			if (fz_peekbyte(stream) == EOF && bits == 0)
			{
				error = fz_readerror(stream);
				fz_dropstream(stream);
				if (error)
					return fz_rethrow(error, "truncated sample stream");
				return fz_throw("truncated sample stream");
			}

			if (bps == 8) {
				s = fz_readbyte(stream);
			}
			else if (bps == 16) {
				s = fz_readbyte(stream);
				s = (s << 8) + fz_readbyte(stream);
			}
			else if (bps == 32) {
				s = fz_readbyte(stream);
				s = (s << 8) + fz_readbyte(stream);
				s = (s << 8) + fz_readbyte(stream);
				s = (s << 8) + fz_readbyte(stream);
			}
			else {
				while (bits < bps)
				{
					buf = (buf << 8) | (fz_readbyte(stream) & 0xff);
					bits += 8;
				}
				s = (buf >> (bits - bps)) & bitmask;
				bits -= bps;
			}

			func->u.sa.samples[i] = s;
		}

		error = fz_readerror(stream);
		if (error)
			return fz_rethrow(error, "truncated sample stream");
	}

	fz_dropstream(stream);

	pdf_logrsrc("}\n");

	return fz_okay;
}

static fz_error
evalsamplefunc(pdf_function *func, float *in, float *out)
{
	float x;
	int e[2][MAXM];
	float efrac[MAXM];
	float static0[1 << 4];
	float static1[1 << 4];
	float *s0 = static0;
	float *s1 = static1;
	int i, j, k;
	int idx;

	/* encode input coordinates */
	for (i = 0; i < func->m; i++)
	{
		x = CLAMP(in[i], func->domain[i][0], func->domain[i][1]);
		x = LERP(x, func->domain[i][0], func->domain[i][1],
			func->u.sa.encode[i][0], func->u.sa.encode[i][1]);
		x = CLAMP(x, 0, func->u.sa.size[i] - 1);
		e[0][i] = floor(x);
		e[1][i] = ceil(x);
		efrac[i] = x - floor(x);
	}

	if (func->m > 4)
	{
		s0 = fz_malloc((1 << func->m) * 2 * sizeof(float));
		s1 = s0 + (1 << func->m);
	}

	/* FIXME i think this is wrong... test with 2 samples it gets wrong idxs */
	for (i = 0; i < func->n; i++)
	{
		/* pull 2^m values out of the sample array */
		for (j = 0; j < (1 << func->m); ++j)
		{
			idx = 0;
			for (k = func->m - 1; k >= 0; --k)
				idx = idx * func->u.sa.size[k] + e[(j >> k) & 1][k];
			idx = idx * func->n + i;
			s0[j] = func->u.sa.samples[idx];
		}

		/* do m sets of interpolations */
		for (j = 0; j < func->m; ++j)
		{
			for (k = 0; k < (1 << (func->m - j)); k += 2)
				s1[k >> 1] = (1 - efrac[j]) * s0[k] + efrac[j] * s0[k+1];
			memcpy(s0, s1, (1 << (func->m - j - 1)) * sizeof(float));
		}

		/* decode output values */
		out[i] = LERP(s0[0], 0, (1 << func->u.sa.bps) - 1,
			func->u.sa.decode[i][0], func->u.sa.decode[i][1]);
		out[i] = CLAMP(out[i], func->range[i][0], func->range[i][1]);
	}

	if (func->m > 4)
		fz_free(s0);

	return fz_okay;
}

/*
 * Exponential function
 */

static fz_error
loadexponentialfunc(pdf_function *func, fz_obj *dict)
{
	fz_obj *obj;
	int i;

	pdf_logrsrc("exponential function {\n");

	if (func->m != 1)
		return fz_throw("/Domain must be one dimension (%d)", func->m);

	obj = fz_dictgets(dict, "N");
	if (!fz_isint(obj) && !fz_isreal(obj))
		return fz_throw("malformed /N");
	func->u.e.n = fz_toreal(obj);
	pdf_logrsrc("n %g\n", func->u.e.n);

	obj = fz_dictgets(dict, "C0");
	if (fz_isarray(obj))
	{
		func->n = fz_arraylen(obj);
		if (func->n >= MAXN)
			return fz_throw("exponential function result array out of range");
		for (i = 0; i < func->n; ++i)
			func->u.e.c0[i] = fz_toreal(fz_arrayget(obj, i));
		pdf_logrsrc("c0 %d\n", func->n);
	}
	else
	{
		func->n = 1;
		func->u.e.c0[0] = 0;
	}

	obj = fz_dictgets(dict, "C1");
	if (fz_isarray(obj))
	{
		if (fz_arraylen(obj) != func->n)
			return fz_throw("/C1 must match /C0 length");
		for (i = 0; i < func->n; ++i)
			func->u.e.c1[i] = fz_toreal(fz_arrayget(obj, i));
		pdf_logrsrc("c1 %d\n", func->n);
	}
	else
	{
		if (func->n != 1)
			return fz_throw("/C1 must match /C0 length");
		func->u.e.c1[0] = 1;
	}

	pdf_logrsrc("}\n");

	return fz_okay;
}

static fz_error
evalexponentialfunc(pdf_function *func, float in, float *out)
{
	float x = in;
	float tmp;
	int i;

	x = CLAMP(x, func->domain[0][0], func->domain[0][1]);

	/* constraint */
	if (func->u.e.n != (int)func->u.e.n && x < 0)
		return fz_throw("constraint error");
	if (func->u.e.n < 0 && x == 0)
		return fz_throw("constraint error");

	tmp = pow(x, func->u.e.n);
	for (i = 0; i < func->n; ++i)
	{
		out[i] = func->u.e.c0[i] + tmp * (func->u.e.c1[i] - func->u.e.c0[i]);
		if (func->hasrange)
			out[i] = CLAMP(out[i], func->range[i][0], func->range[i][1]);
	}

	return fz_okay;
}

/*
 * Stitching function
 */

static fz_error
loadstitchingfunc(pdf_function *func, pdf_xref *xref, fz_obj *dict)
{
	pdf_function **funcs;
	fz_error error;
	fz_obj *obj;
	fz_obj *sub;
	fz_obj *num;
	int k;
	int i;

	pdf_logrsrc("stitching {\n");

	func->u.st.k = 0;

	if (func->m != 1)
		return fz_throw("/Domain must be one dimension (%d)", func->m);

	obj = fz_dictgets(dict, "Functions");
	if (!fz_isarray(obj))
		return fz_throw("stitching function has no input functions");
	{
		k = fz_arraylen(obj);
		func->u.st.k = k;

		pdf_logrsrc("k %d\n", func->u.st.k);

		func->u.st.funcs = fz_malloc(func->u.st.k * sizeof (pdf_function));
		func->u.st.bounds = fz_malloc((func->u.st.k - 1) * sizeof (float));
		func->u.st.encode = fz_malloc(func->u.st.k * 2 * sizeof (float));
		funcs = func->u.st.funcs;

		for (i = 0; i < k; ++i)
		{
			sub = fz_arrayget(obj, i);
			error = pdf_loadfunction(&func->u.st.funcs[i], xref, sub);
			if (error)
				return fz_rethrow(error, "cannot load sub function %d", i);
			if (funcs[i]->m != 1 || funcs[i]->n != funcs[0]->n)
				return fz_rethrow(error, "sub function %d /Domain or /Range mismatch", i);
		}

		if (!func->n)
			func->n = funcs[0]->n;
		else if (func->n != funcs[0]->n)
			return fz_throw("sub function /Domain or /Range mismatch");
	}

	obj = fz_dictgets(dict, "Bounds");
	if (!fz_isarray(obj))
		return fz_throw("stitching function has no bounds");
	{
		if (!fz_isarray(obj) || fz_arraylen(obj) != k - 1)
			return fz_throw("malformed /Bounds (not array or wrong length)");

		for (i = 0; i < k-1; ++i)
		{
			num = fz_arrayget(obj, i);
			if (!fz_isint(num) && !fz_isreal(num))
				return fz_throw("malformed /Bounds (item not number)");
			func->u.st.bounds[i] = fz_toreal(num);
			if (i && func->u.st.bounds[i-1] > func->u.st.bounds[i])
				return fz_throw("malformed /Bounds (item not monotonic)");
		}

		if (k != 1 && (func->domain[0][0] > func->u.st.bounds[0] ||
			func->domain[0][1] < func->u.st.bounds[k-2]))
			fz_warn("malformed shading function bounds (domain mismatch), proceeding anyway.");
	}

	obj = fz_dictgets(dict, "Encode");
	if (!fz_isarray(obj))
		return fz_throw("stitching function is missing encoding");
	{
		if (!fz_isarray(obj) || fz_arraylen(obj) != k * 2)
			return fz_throw("malformed /Encode");
		for (i = 0; i < k; ++i)
		{
			func->u.st.encode[i*2+0] = fz_toreal(fz_arrayget(obj, i*2+0));
			func->u.st.encode[i*2+1] = fz_toreal(fz_arrayget(obj, i*2+1));
		}
	}

	pdf_logrsrc("}\n");

	return fz_okay;
}

static fz_error
evalstitchingfunc(pdf_function *func, float in, float *out)
{
	fz_error error;
	float low, high;
	int k = func->u.st.k;
	float *bounds = func->u.st.bounds;
	int i;

	in = CLAMP(in, func->domain[0][0], func->domain[0][1]);

	for (i = 0; i < k - 1; ++i)
	{
		if (in < bounds[i])
			break;
	}

	if (i == 0 && k == 1)
	{
		low = func->domain[0][0];
		high = func->domain[0][1];
	}
	else if (i == 0)
	{
		low = func->domain[0][0];
		high = bounds[0];
	}
	else if (i == k - 1)
	{
		low = bounds[k-2];
		high = func->domain[0][1];
	}
	else
	{
		low = bounds[i-1];
		high = bounds[i];
	}

	in = LERP(in, low, high, func->u.st.encode[i*2+0], func->u.st.encode[i*2+1]);

	error = pdf_evalfunction(func->u.st.funcs[i], &in, 1, out, func->n);
	if (error)
		return fz_rethrow(error, "cannot evaluate sub function %d", i);

	return fz_okay;
}

/*
 * Common
 */

pdf_function *
pdf_keepfunction(pdf_function *func)
{
	func->refs ++;
	return func;
}

void
pdf_dropfunction(pdf_function *func)
{
	int i;
	if (--func->refs == 0)
	{
		switch(func->type)
		{
		case SAMPLE:
			fz_free(func->u.sa.samples);
			break;
		case EXPONENTIAL:
			break;
		case STITCHING:
			for (i = 0; i < func->u.st.k; ++i)
				pdf_dropfunction(func->u.st.funcs[i]);
			fz_free(func->u.st.funcs);
			fz_free(func->u.st.bounds);
			fz_free(func->u.st.encode);
			break;
		case POSTSCRIPT:
			fz_free(func->u.p.code);
			break;
		}
		fz_free(func);
	}
}

fz_error
pdf_loadfunction(pdf_function **funcp, pdf_xref *xref, fz_obj *ref)
{
	fz_error error;
	pdf_function *func;
	fz_obj *dict;
	fz_obj *obj;
	int i;

	if ((*funcp = pdf_finditem(xref->store, PDF_KFUNCTION, ref)))
	{
		pdf_keepfunction(*funcp);
		return fz_okay;
	}

	pdf_logrsrc("load function (%d %d R) {\n", fz_tonum(ref), fz_togen(ref));

	func = fz_malloc(sizeof(pdf_function));
	memset(func, 0, sizeof(pdf_function));
	func->refs = 1;

	dict = fz_resolveindirect(ref);

	obj = fz_dictgets(dict, "FunctionType");
	func->type = fz_toint(obj);

	pdf_logrsrc("type %d\n", func->type);

	/* required for all */
	obj = fz_dictgets(dict, "Domain");
	func->m = fz_arraylen(obj) / 2;
	for (i = 0; i < func->m; i++)
	{
		func->domain[i][0] = fz_toreal(fz_arrayget(obj, i * 2 + 0));
		func->domain[i][1] = fz_toreal(fz_arrayget(obj, i * 2 + 1));
	}
	pdf_logrsrc("domain %d\n", func->m);

	/* required for type0 and type4, optional otherwise */
	obj = fz_dictgets(dict, "Range");
	if (fz_isarray(obj))
	{
		func->hasrange = 1;
		func->n = fz_arraylen(obj) / 2;
		for (i = 0; i < func->n; i++)
		{
			func->range[i][0] = fz_toreal(fz_arrayget(obj, i * 2 + 0));
			func->range[i][1] = fz_toreal(fz_arrayget(obj, i * 2 + 1));
		}
		pdf_logrsrc("range %d\n", func->n);
	}
	else
	{
		func->hasrange = 0;
		func->n = 0;
	}

	if (func->m >= MAXM || func->n >= MAXN)
	{
		fz_free(func);
		return fz_throw("assert: /Domain or /Range too big");
	}

	switch(func->type)
	{
	case SAMPLE:
		error = loadsamplefunc(func, xref, dict, fz_tonum(ref), fz_togen(ref));
		if (error)
		{
			pdf_dropfunction(func);
			return fz_rethrow(error, "cannot load sampled function (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		break;

	case EXPONENTIAL:
		error = loadexponentialfunc(func, dict);
		if (error)
		{
			pdf_dropfunction(func);
			return fz_rethrow(error, "cannot load exponential function (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		break;

	case STITCHING:
		error = loadstitchingfunc(func, xref, dict);
		if (error)
		{
			pdf_dropfunction(func);
			return fz_rethrow(error, "cannot load stitching function (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		break;

	case POSTSCRIPT:
		error = loadpostscriptfunc(func, xref, dict, fz_tonum(ref), fz_togen(ref));
		if (error)
		{
			pdf_dropfunction(func);
			return fz_rethrow(error, "cannot load calculator function (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		break;

	default:
		fz_free(func);
		return fz_throw("unknown function type (%d %d R)", fz_tonum(ref), fz_togen(ref));
	}

	pdf_logrsrc("}\n");

	pdf_storeitem(xref->store, PDF_KFUNCTION, ref, func);

	*funcp = func;
	return fz_okay;
}

fz_error
pdf_evalfunction(pdf_function *func, float *in, int inlen, float *out, int outlen)
{
	fz_error error;
	float r;
	int i;

	if (func->m != inlen || func->n != outlen)
		return fz_throw("function argument count mismatch");

	switch(func->type)
	{
	case SAMPLE:
		error = evalsamplefunc(func, in, out);
		if (error)
			return fz_rethrow(error, "cannot evaluate sampled function");
		break;

	case EXPONENTIAL:
		error = evalexponentialfunc(func, *in, out);
		if (error)
			return fz_rethrow(error, "cannot evaluate exponential function");
		break;

	case STITCHING:
		error = evalstitchingfunc(func, *in, out);
		if (error)
			return fz_rethrow(error, "cannot evaluate stitching function");
		break;

	case POSTSCRIPT:
		{
			psstack st;
			psinitstack(&st);

			for (i = 0; i < func->m; ++i)
				SAFE_PUSHREAL(&st, in[i]);

			error = evalpostscriptfunc(func, &st, 0);
			if (error)
				return fz_rethrow(error, "cannot evaluate calculator function");

			for (i = func->n - 1; i >= 0; --i)
			{
				SAFE_POPNUM(&st, &r);
				out[i] = CLAMP(r, func->range[i][0], func->range[i][1]);
			}
		}
		break;

	default:
		return fz_throw("assert: unknown function type");
	}

	return fz_okay;
}

