#include "mupdf/pdf.h"

typedef struct psobj_s psobj;

enum
{
	SAMPLE = 0,
	EXPONENTIAL = 2,
	STITCHING = 3,
	POSTSCRIPT = 4
};

typedef struct pdf_function_s pdf_function;

struct pdf_function_s
{
	fz_function base;
	int type;				/* 0=sample 2=exponential 3=stitching 4=postscript */
	float domain[FZ_FN_MAXM][2];	/* even index : min value, odd index : max value */
	float range[FZ_FN_MAXN][2];	/* even index : min value, odd index : max value */
	int has_range;

	union
	{
		struct {
			unsigned short bps;
			int size[FZ_FN_MAXM];
			float encode[FZ_FN_MAXM][2];
			float decode[FZ_FN_MAXN][2];
			float *samples;
		} sa;

		struct {
			float n;
			float c0[FZ_FN_MAXN];
			float c1[FZ_FN_MAXN];
		} e;

		struct {
			int k;
			fz_function **funcs; /* k */
			float *bounds; /* k - 1 */
			float *encode; /* k * 2 */
		} st;

		struct {
			psobj *code;
			int cap;
		} p;
	} u;
};

#define RADIAN 57.2957795

static inline float lerp(float x, float xmin, float xmax, float ymin, float ymax)
{
	if (xmin == xmax)
		return ymin;
	if (ymin == ymax)
		return ymin;
	return ymin + (x - xmin) * (ymax - ymin) / (xmax - xmin);
}

/*
 * PostScript calculator
 */

enum { PS_BOOL, PS_INT, PS_REAL, PS_OPERATOR, PS_BLOCK };

enum
{
	PS_OP_ABS, PS_OP_ADD, PS_OP_AND, PS_OP_ATAN, PS_OP_BITSHIFT,
	PS_OP_CEILING, PS_OP_COPY, PS_OP_COS, PS_OP_CVI, PS_OP_CVR,
	PS_OP_DIV, PS_OP_DUP, PS_OP_EQ, PS_OP_EXCH, PS_OP_EXP,
	PS_OP_FALSE, PS_OP_FLOOR, PS_OP_GE, PS_OP_GT, PS_OP_IDIV, PS_OP_IF,
	PS_OP_IFELSE, PS_OP_INDEX, PS_OP_LE, PS_OP_LN, PS_OP_LOG, PS_OP_LT,
	PS_OP_MOD, PS_OP_MUL, PS_OP_NE, PS_OP_NEG, PS_OP_NOT, PS_OP_OR,
	PS_OP_POP, PS_OP_RETURN, PS_OP_ROLL, PS_OP_ROUND, PS_OP_SIN,
	PS_OP_SQRT, PS_OP_SUB, PS_OP_TRUE, PS_OP_TRUNCATE, PS_OP_XOR
};

static char *ps_op_names[] =
{
	"abs", "add", "and", "atan", "bitshift", "ceiling", "copy",
	"cos", "cvi", "cvr", "div", "dup", "eq", "exch", "exp",
	"false", "floor", "ge", "gt", "idiv", "if", "ifelse", "index", "le", "ln",
	"log", "lt", "mod", "mul", "ne", "neg", "not", "or", "pop", "return",
	"roll", "round", "sin", "sqrt", "sub", "true", "truncate", "xor"
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

typedef struct ps_stack_s ps_stack;

struct ps_stack_s
{
	psobj stack[100];
	int sp;
};

#ifndef NDEBUG
void
pdf_debug_ps_stack(ps_stack *st)
{
	int i;

	printf("stack: ");

	for (i = 0; i < st->sp; i++)
	{
		switch (st->stack[i].type)
		{
		case PS_BOOL:
			if (st->stack[i].u.b)
				printf("true ");
			else
				printf("false ");
			break;

		case PS_INT:
			printf("%d ", st->stack[i].u.i);
			break;

		case PS_REAL:
			printf("%g ", st->stack[i].u.f);
			break;
		}
	}
	printf("\n");

}
#endif

static void
ps_init_stack(ps_stack *st)
{
	memset(st->stack, 0, sizeof(st->stack));
	st->sp = 0;
}

static inline int ps_overflow(ps_stack *st, int n)
{
	return n < 0 || st->sp + n >= nelem(st->stack);
}

static inline int ps_underflow(ps_stack *st, int n)
{
	return n < 0 || st->sp - n < 0;
}

static inline int ps_is_type(ps_stack *st, int t)
{
	return !ps_underflow(st, 1) && st->stack[st->sp - 1].type == t;
}

static inline int ps_is_type2(ps_stack *st, int t)
{
	return !ps_underflow(st, 2) && st->stack[st->sp - 1].type == t && st->stack[st->sp - 2].type == t;
}

static void
ps_push_bool(ps_stack *st, int b)
{
	if (!ps_overflow(st, 1))
	{
		st->stack[st->sp].type = PS_BOOL;
		st->stack[st->sp].u.b = b;
		st->sp++;
	}
}

static void
ps_push_int(ps_stack *st, int n)
{
	if (!ps_overflow(st, 1))
	{
		st->stack[st->sp].type = PS_INT;
		st->stack[st->sp].u.i = n;
		st->sp++;
	}
}

static void
ps_push_real(ps_stack *st, float n)
{
	if (!ps_overflow(st, 1))
	{
		st->stack[st->sp].type = PS_REAL;
		if (isnan(n))
		{
			/* Push 1.0, as it's a small known value that won't
			 * cause a divide by 0. Same reason as in fz_atof. */
			n = 1.0;
		}
		st->stack[st->sp].u.f = fz_clamp(n, -FLT_MAX, FLT_MAX);
		st->sp++;
	}
}

static int
ps_pop_bool(ps_stack *st)
{
	if (!ps_underflow(st, 1))
	{
		if (ps_is_type(st, PS_BOOL))
			return st->stack[--st->sp].u.b;
	}
	return 0;
}

static int
ps_pop_int(ps_stack *st)
{
	if (!ps_underflow(st, 1))
	{
		if (ps_is_type(st, PS_INT))
			return st->stack[--st->sp].u.i;
		if (ps_is_type(st, PS_REAL))
			return st->stack[--st->sp].u.f;
	}
	return 0;
}

static float
ps_pop_real(ps_stack *st)
{
	if (!ps_underflow(st, 1))
	{
		if (ps_is_type(st, PS_INT))
			return st->stack[--st->sp].u.i;
		if (ps_is_type(st, PS_REAL))
			return st->stack[--st->sp].u.f;
	}
	return 0;
}

static void
ps_copy(ps_stack *st, int n)
{
	if (!ps_underflow(st, n) && !ps_overflow(st, n))
	{
		memcpy(st->stack + st->sp, st->stack + st->sp - n, n * sizeof(psobj));
		st->sp += n;
	}
}

static void
ps_roll(ps_stack *st, int n, int j)
{
	psobj tmp;
	int i;

	if (ps_underflow(st, n) || j == 0 || n == 0)
		return;

	if (j >= 0)
	{
		j %= n;
	}
	else
	{
		j = -j % n;
		if (j != 0)
			j = n - j;
	}

	for (i = 0; i < j; i++)
	{
		tmp = st->stack[st->sp - 1];
		memmove(st->stack + st->sp - n + 1, st->stack + st->sp - n, n * sizeof(psobj));
		st->stack[st->sp - n] = tmp;
	}
}

static void
ps_index(ps_stack *st, int n)
{
	if (!ps_overflow(st, 1) && !ps_underflow(st, n))
	{
		st->stack[st->sp] = st->stack[st->sp - n - 1];
		st->sp++;
	}
}

static void
ps_run(fz_context *ctx, psobj *code, ps_stack *st, int pc)
{
	int i1, i2;
	float r1, r2;
	int b1, b2;

	while (1)
	{
		switch (code[pc].type)
		{
		case PS_INT:
			ps_push_int(st, code[pc++].u.i);
			break;

		case PS_REAL:
			ps_push_real(st, code[pc++].u.f);
			break;

		case PS_OPERATOR:
			switch (code[pc++].u.op)
			{
			case PS_OP_ABS:
				if (ps_is_type(st, PS_INT))
					ps_push_int(st, abs(ps_pop_int(st)));
				else
					ps_push_real(st, fabsf(ps_pop_real(st)));
				break;

			case PS_OP_ADD:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_int(st, i1 + i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_real(st, r1 + r2);
				}
				break;

			case PS_OP_AND:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_int(st, i1 & i2);
				}
				else {
					b2 = ps_pop_bool(st);
					b1 = ps_pop_bool(st);
					ps_push_bool(st, b1 && b2);
				}
				break;

			case PS_OP_ATAN:
				r2 = ps_pop_real(st);
				r1 = ps_pop_real(st);
				r1 = atan2f(r1, r2) * RADIAN;
				if (r1 < 0)
					r1 += 360;
				ps_push_real(st, r1);
				break;

			case PS_OP_BITSHIFT:
				i2 = ps_pop_int(st);
				i1 = ps_pop_int(st);
				if (i2 > 0 && i2 < 8 * sizeof (i2))
					ps_push_int(st, i1 << i2);
				else if (i2 < 0 && i2 > -8 * (int)sizeof (i2))
					ps_push_int(st, (int)((unsigned int)i1 >> -i2));
				else
					ps_push_int(st, i1);
				break;

			case PS_OP_CEILING:
				r1 = ps_pop_real(st);
				ps_push_real(st, ceilf(r1));
				break;

			case PS_OP_COPY:
				ps_copy(st, ps_pop_int(st));
				break;

			case PS_OP_COS:
				r1 = ps_pop_real(st);
				ps_push_real(st, cosf(r1/RADIAN));
				break;

			case PS_OP_CVI:
				ps_push_int(st, ps_pop_int(st));
				break;

			case PS_OP_CVR:
				ps_push_real(st, ps_pop_real(st));
				break;

			case PS_OP_DIV:
				r2 = ps_pop_real(st);
				r1 = ps_pop_real(st);
				if (fabsf(r2) >= FLT_EPSILON)
					ps_push_real(st, r1 / r2);
				else
					ps_push_real(st, DIV_BY_ZERO(r1, r2, -FLT_MAX, FLT_MAX));
				break;

			case PS_OP_DUP:
				ps_copy(st, 1);
				break;

			case PS_OP_EQ:
				if (ps_is_type2(st, PS_BOOL)) {
					b2 = ps_pop_bool(st);
					b1 = ps_pop_bool(st);
					ps_push_bool(st, b1 == b2);
				}
				else if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_bool(st, i1 == i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_bool(st, r1 == r2);
				}
				break;

			case PS_OP_EXCH:
				ps_roll(st, 2, 1);
				break;

			case PS_OP_EXP:
				r2 = ps_pop_real(st);
				r1 = ps_pop_real(st);
				ps_push_real(st, powf(r1, r2));
				break;

			case PS_OP_FALSE:
				ps_push_bool(st, 0);
				break;

			case PS_OP_FLOOR:
				r1 = ps_pop_real(st);
				ps_push_real(st, floorf(r1));
				break;

			case PS_OP_GE:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_bool(st, i1 >= i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_bool(st, r1 >= r2);
				}
				break;

			case PS_OP_GT:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_bool(st, i1 > i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_bool(st, r1 > r2);
				}
				break;

			case PS_OP_IDIV:
				i2 = ps_pop_int(st);
				i1 = ps_pop_int(st);
				if (i2 != 0)
					ps_push_int(st, i1 / i2);
				else
					ps_push_int(st, DIV_BY_ZERO(i1, i2, INT_MIN, INT_MAX));
				break;

			case PS_OP_INDEX:
				ps_index(st, ps_pop_int(st));
				break;

			case PS_OP_LE:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_bool(st, i1 <= i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_bool(st, r1 <= r2);
				}
				break;

			case PS_OP_LN:
				r1 = ps_pop_real(st);
				/* Bug 692941 - logf as separate statement */
				r2 = logf(r1);
				ps_push_real(st, r2);
				break;

			case PS_OP_LOG:
				r1 = ps_pop_real(st);
				ps_push_real(st, log10f(r1));
				break;

			case PS_OP_LT:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_bool(st, i1 < i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_bool(st, r1 < r2);
				}
				break;

			case PS_OP_MOD:
				i2 = ps_pop_int(st);
				i1 = ps_pop_int(st);
				if (i2 != 0)
					ps_push_int(st, i1 % i2);
				else
					ps_push_int(st, DIV_BY_ZERO(i1, i2, INT_MIN, INT_MAX));
				break;

			case PS_OP_MUL:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_int(st, i1 * i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_real(st, r1 * r2);
				}
				break;

			case PS_OP_NE:
				if (ps_is_type2(st, PS_BOOL)) {
					b2 = ps_pop_bool(st);
					b1 = ps_pop_bool(st);
					ps_push_bool(st, b1 != b2);
				}
				else if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_bool(st, i1 != i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_bool(st, r1 != r2);
				}
				break;

			case PS_OP_NEG:
				if (ps_is_type(st, PS_INT))
					ps_push_int(st, -ps_pop_int(st));
				else
					ps_push_real(st, -ps_pop_real(st));
				break;

			case PS_OP_NOT:
				if (ps_is_type(st, PS_BOOL))
					ps_push_bool(st, !ps_pop_bool(st));
				else
					ps_push_int(st, ~ps_pop_int(st));
				break;

			case PS_OP_OR:
				if (ps_is_type2(st, PS_BOOL)) {
					b2 = ps_pop_bool(st);
					b1 = ps_pop_bool(st);
					ps_push_bool(st, b1 || b2);
				}
				else {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_int(st, i1 | i2);
				}
				break;

			case PS_OP_POP:
				if (!ps_underflow(st, 1))
					st->sp--;
				break;

			case PS_OP_ROLL:
				i2 = ps_pop_int(st);
				i1 = ps_pop_int(st);
				ps_roll(st, i1, i2);
				break;

			case PS_OP_ROUND:
				if (!ps_is_type(st, PS_INT)) {
					r1 = ps_pop_real(st);
					ps_push_real(st, (r1 >= 0) ? floorf(r1 + 0.5f) : ceilf(r1 - 0.5f));
				}
				break;

			case PS_OP_SIN:
				r1 = ps_pop_real(st);
				ps_push_real(st, sinf(r1/RADIAN));
				break;

			case PS_OP_SQRT:
				r1 = ps_pop_real(st);
				ps_push_real(st, sqrtf(r1));
				break;

			case PS_OP_SUB:
				if (ps_is_type2(st, PS_INT)) {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_int(st, i1 - i2);
				}
				else {
					r2 = ps_pop_real(st);
					r1 = ps_pop_real(st);
					ps_push_real(st, r1 - r2);
				}
				break;

			case PS_OP_TRUE:
				ps_push_bool(st, 1);
				break;

			case PS_OP_TRUNCATE:
				if (!ps_is_type(st, PS_INT)) {
					r1 = ps_pop_real(st);
					ps_push_real(st, (r1 >= 0) ? floorf(r1) : ceilf(r1));
				}
				break;

			case PS_OP_XOR:
				if (ps_is_type2(st, PS_BOOL)) {
					b2 = ps_pop_bool(st);
					b1 = ps_pop_bool(st);
					ps_push_bool(st, b1 ^ b2);
				}
				else {
					i2 = ps_pop_int(st);
					i1 = ps_pop_int(st);
					ps_push_int(st, i1 ^ i2);
				}
				break;

			case PS_OP_IF:
				b1 = ps_pop_bool(st);
				if (b1)
					ps_run(ctx, code, st, code[pc + 1].u.block);
				pc = code[pc + 2].u.block;
				break;

			case PS_OP_IFELSE:
				b1 = ps_pop_bool(st);
				if (b1)
					ps_run(ctx, code, st, code[pc + 1].u.block);
				else
					ps_run(ctx, code, st, code[pc + 0].u.block);
				pc = code[pc + 2].u.block;
				break;

			case PS_OP_RETURN:
				return;

			default:
				fz_warn(ctx, "foreign operator in calculator function");
				return;
			}
			break;

		default:
			fz_warn(ctx, "foreign object in calculator function");
			return;
		}
	}
}

static void
resize_code(fz_context *ctx, pdf_function *func, int newsize)
{
	if (newsize >= func->u.p.cap)
	{
		int new_cap = func->u.p.cap + 64;
		func->u.p.code = fz_resize_array(ctx, func->u.p.code, new_cap, sizeof(psobj));
		func->u.p.cap = new_cap;
	}
}

static void
parse_code(pdf_function *func, fz_stream *stream, int *codeptr, pdf_lexbuf *buf)
{
	pdf_token tok;
	int opptr, elseptr, ifptr;
	int a, b, mid, cmp;
	fz_context *ctx = stream->ctx;

	while (1)
	{
		tok = pdf_lex(stream, buf);

		switch (tok)
		{
		case PDF_TOK_EOF:
			fz_throw(ctx, FZ_ERROR_GENERIC, "truncated calculator function");

		case PDF_TOK_INT:
			resize_code(ctx, func, *codeptr);
			func->u.p.code[*codeptr].type = PS_INT;
			func->u.p.code[*codeptr].u.i = buf->i;
			++*codeptr;
			break;

		case PDF_TOK_TRUE:
			resize_code(ctx, func, *codeptr);
			func->u.p.code[*codeptr].type = PS_BOOL;
			func->u.p.code[*codeptr].u.b = 1;
			++*codeptr;
			break;

		case PDF_TOK_FALSE:
			resize_code(ctx, func, *codeptr);
			func->u.p.code[*codeptr].type = PS_BOOL;
			func->u.p.code[*codeptr].u.b = 0;
			++*codeptr;
			break;

		case PDF_TOK_REAL:
			resize_code(ctx, func, *codeptr);
			func->u.p.code[*codeptr].type = PS_REAL;
			func->u.p.code[*codeptr].u.f = buf->f;
			++*codeptr;
			break;

		case PDF_TOK_OPEN_BRACE:
			opptr = *codeptr;
			*codeptr += 4;

			resize_code(ctx, func, *codeptr);

			ifptr = *codeptr;
			parse_code(func, stream, codeptr, buf);

			tok = pdf_lex(stream, buf);

			if (tok == PDF_TOK_OPEN_BRACE)
			{
				elseptr = *codeptr;
				parse_code(func, stream, codeptr, buf);

				tok = pdf_lex(stream, buf);
			}
			else
			{
				elseptr = -1;
			}

			if (tok != PDF_TOK_KEYWORD)
				fz_throw(ctx, FZ_ERROR_GENERIC, "missing keyword in 'if-else' context");

			if (!strcmp(buf->scratch, "if"))
			{
				if (elseptr >= 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "too many branches for 'if'");
				func->u.p.code[opptr].type = PS_OPERATOR;
				func->u.p.code[opptr].u.op = PS_OP_IF;
				func->u.p.code[opptr+2].type = PS_BLOCK;
				func->u.p.code[opptr+2].u.block = ifptr;
				func->u.p.code[opptr+3].type = PS_BLOCK;
				func->u.p.code[opptr+3].u.block = *codeptr;
			}
			else if (!strcmp(buf->scratch, "ifelse"))
			{
				if (elseptr < 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "not enough branches for 'ifelse'");
				func->u.p.code[opptr].type = PS_OPERATOR;
				func->u.p.code[opptr].u.op = PS_OP_IFELSE;
				func->u.p.code[opptr+1].type = PS_BLOCK;
				func->u.p.code[opptr+1].u.block = elseptr;
				func->u.p.code[opptr+2].type = PS_BLOCK;
				func->u.p.code[opptr+2].u.block = ifptr;
				func->u.p.code[opptr+3].type = PS_BLOCK;
				func->u.p.code[opptr+3].u.block = *codeptr;
			}
			else
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "unknown keyword in 'if-else' context: '%s'", buf->scratch);
			}
			break;

		case PDF_TOK_CLOSE_BRACE:
			resize_code(ctx, func, *codeptr);
			func->u.p.code[*codeptr].type = PS_OPERATOR;
			func->u.p.code[*codeptr].u.op = PS_OP_RETURN;
			++*codeptr;
			return;

		case PDF_TOK_KEYWORD:
			cmp = -1;
			a = -1;
			b = nelem(ps_op_names);
			while (b - a > 1)
			{
				mid = (a + b) / 2;
				cmp = strcmp(buf->scratch, ps_op_names[mid]);
				if (cmp > 0)
					a = mid;
				else if (cmp < 0)
					b = mid;
				else
					a = b = mid;
			}
			if (cmp != 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "unknown operator: '%s'", buf->scratch);

			resize_code(ctx, func, *codeptr);
			func->u.p.code[*codeptr].type = PS_OPERATOR;
			func->u.p.code[*codeptr].u.op = a;
			++*codeptr;
			break;

		default:
			fz_throw(ctx, FZ_ERROR_GENERIC, "calculator function syntax error");
		}
	}
}

static void
load_postscript_func(pdf_function *func, pdf_document *doc, pdf_obj *dict, int num, int gen)
{
	fz_stream *stream = NULL;
	int codeptr;
	pdf_lexbuf buf;
	pdf_token tok;
	fz_context *ctx = doc->ctx;
	int locked = 0;

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);

	fz_var(stream);
	fz_var(locked);

	fz_try(ctx)
	{
		stream = pdf_open_stream(doc, num, gen);

		tok = pdf_lex(stream, &buf);
		if (tok != PDF_TOK_OPEN_BRACE)
		{
			fz_throw(ctx, FZ_ERROR_GENERIC, "stream is not a calculator function");
		}

		func->u.p.code = NULL;
		func->u.p.cap = 0;

		codeptr = 0;
		parse_code(func, stream, &codeptr, &buf);
	}
	fz_always(ctx)
	{
		fz_close(stream);
		pdf_lexbuf_fin(&buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot parse calculator function (%d %d R)", num, gen);
	}

	func->base.size += func->u.p.cap * sizeof(psobj);
}

static void
eval_postscript_func(fz_context *ctx, pdf_function *func, float *in, float *out)
{
	ps_stack st;
	float x;
	int i;

	ps_init_stack(&st);

	for (i = 0; i < func->base.m; i++)
	{
		x = fz_clamp(in[i], func->domain[i][0], func->domain[i][1]);
		ps_push_real(&st, x);
	}

	ps_run(ctx, func->u.p.code, &st, 0);

	for (i = func->base.n - 1; i >= 0; i--)
	{
		x = ps_pop_real(&st);
		out[i] = fz_clamp(x, func->range[i][0], func->range[i][1]);
	}
}

/*
 * Sample function
 */

#define MAX_SAMPLE_FUNCTION_SIZE (100 << 20)

static void
load_sample_func(pdf_function *func, pdf_document *doc, pdf_obj *dict, int num, int gen)
{
	fz_context *ctx = doc->ctx;
	fz_stream *stream;
	pdf_obj *obj;
	int samplecount;
	int bps;
	int i;

	func->u.sa.samples = NULL;

	obj = pdf_dict_gets(dict, "Size");
	if (pdf_array_len(obj) < func->base.m)
		fz_throw(ctx, FZ_ERROR_GENERIC, "too few sample function dimension sizes");
	if (pdf_array_len(obj) > func->base.m)
		fz_warn(ctx, "too many sample function dimension sizes");
	for (i = 0; i < func->base.m; i++)
	{
		func->u.sa.size[i] = pdf_to_int(pdf_array_get(obj, i));
		if (func->u.sa.size[i] <= 0)
		{
			fz_warn(ctx, "non-positive sample function dimension size");
			func->u.sa.size[i] = 1;
		}
	}

	obj = pdf_dict_gets(dict, "BitsPerSample");
	func->u.sa.bps = bps = pdf_to_int(obj);

	for (i = 0; i < func->base.m; i++)
	{
		func->u.sa.encode[i][0] = 0;
		func->u.sa.encode[i][1] = func->u.sa.size[i] - 1;
	}
	obj = pdf_dict_gets(dict, "Encode");
	if (pdf_is_array(obj))
	{
		int ranges = fz_mini(func->base.m, pdf_array_len(obj) / 2);
		if (ranges != func->base.m)
			fz_warn(ctx, "wrong number of sample function input mappings");

		for (i = 0; i < ranges; i++)
		{
			func->u.sa.encode[i][0] = pdf_to_real(pdf_array_get(obj, i * 2 + 0));
			func->u.sa.encode[i][1] = pdf_to_real(pdf_array_get(obj, i * 2 + 1));
		}
	}

	for (i = 0; i < func->base.n; i++)
	{
		func->u.sa.decode[i][0] = func->range[i][0];
		func->u.sa.decode[i][1] = func->range[i][1];
	}

	obj = pdf_dict_gets(dict, "Decode");
	if (pdf_is_array(obj))
	{
		int ranges = fz_mini(func->base.n, pdf_array_len(obj) / 2);
		if (ranges != func->base.n)
			fz_warn(ctx, "wrong number of sample function output mappings");

		for (i = 0; i < ranges; i++)
		{
			func->u.sa.decode[i][0] = pdf_to_real(pdf_array_get(obj, i * 2 + 0));
			func->u.sa.decode[i][1] = pdf_to_real(pdf_array_get(obj, i * 2 + 1));
		}
	}

	for (i = 0, samplecount = func->base.n; i < func->base.m; i++)
		samplecount *= func->u.sa.size[i];

	if (samplecount > MAX_SAMPLE_FUNCTION_SIZE)
		fz_throw(ctx, FZ_ERROR_GENERIC, "sample function too large");

	func->u.sa.samples = fz_malloc_array(ctx, samplecount, sizeof(float));
	func->base.size += samplecount * sizeof(float);

	stream = pdf_open_stream(doc, num, gen);

	/* read samples */
	for (i = 0; i < samplecount; i++)
	{
		unsigned int x;
		float s;

		if (fz_is_eof_bits(stream))
		{
			fz_close(stream);
			fz_throw(ctx, FZ_ERROR_GENERIC, "truncated sample function stream");
		}

		switch (bps)
		{
		case 1: s = fz_read_bits(stream, 1); break;
		case 2: s = fz_read_bits(stream, 2) / 3.0f; break;
		case 4: s = fz_read_bits(stream, 4) / 15.0f; break;
		case 8: s = fz_read_byte(stream) / 255.0f; break;
		case 12: s = fz_read_bits(stream, 12) / 4095.0f; break;
		case 16:
			x = fz_read_byte(stream) << 8;
			x |= fz_read_byte(stream);
			s = x / 65535.0f;
			break;
		case 24:
			x = fz_read_byte(stream) << 16;
			x |= fz_read_byte(stream) << 8;
			x |= fz_read_byte(stream);
			s = x / 16777215.0f;
			break;
		case 32:
			x = fz_read_byte(stream) << 24;
			x |= fz_read_byte(stream) << 16;
			x |= fz_read_byte(stream) << 8;
			x |= fz_read_byte(stream);
			s = x / 4294967295.0f;
			break;
		default:
			fz_close(stream);
			fz_throw(ctx, FZ_ERROR_GENERIC, "sample stream bit depth %d unsupported", bps);
		}

		func->u.sa.samples[i] = s;
	}

	fz_close(stream);
}

static float
interpolate_sample(pdf_function *func, int *scale, int *e0, int *e1, float *efrac, int dim, int idx)
{
	float a, b;
	int idx0, idx1;

	idx0 = e0[dim] * scale[dim] + idx;
	idx1 = e1[dim] * scale[dim] + idx;

	if (dim == 0)
	{
		a = func->u.sa.samples[idx0];
		b = func->u.sa.samples[idx1];
	}
	else
	{
		a = interpolate_sample(func, scale, e0, e1, efrac, dim - 1, idx0);
		b = interpolate_sample(func, scale, e0, e1, efrac, dim - 1, idx1);
	}

	return a + (b - a) * efrac[dim];
}

static void
eval_sample_func(fz_context *ctx, pdf_function *func, float *in, float *out)
{
	int e0[FZ_FN_MAXM], e1[FZ_FN_MAXM], scale[FZ_FN_MAXM];
	float efrac[FZ_FN_MAXM];
	float x;
	int i;

	/* encode input coordinates */
	for (i = 0; i < func->base.m; i++)
	{
		x = fz_clamp(in[i], func->domain[i][0], func->domain[i][1]);
		x = lerp(x, func->domain[i][0], func->domain[i][1],
			func->u.sa.encode[i][0], func->u.sa.encode[i][1]);
		x = fz_clamp(x, 0, func->u.sa.size[i] - 1);
		e0[i] = floorf(x);
		e1[i] = ceilf(x);
		efrac[i] = x - floorf(x);
	}

	scale[0] = func->base.n;
	for (i = 1; i < func->base.m; i++)
		scale[i] = scale[i - 1] * func->u.sa.size[i];

	for (i = 0; i < func->base.n; i++)
	{
		if (func->base.m == 1)
		{
			float a = func->u.sa.samples[e0[0] * func->base.n + i];
			float b = func->u.sa.samples[e1[0] * func->base.n + i];

			float ab = a + (b - a) * efrac[0];

			out[i] = lerp(ab, 0, 1, func->u.sa.decode[i][0], func->u.sa.decode[i][1]);
			out[i] = fz_clamp(out[i], func->range[i][0], func->range[i][1]);
		}

		else if (func->base.m == 2)
		{
			int s0 = func->base.n;
			int s1 = s0 * func->u.sa.size[0];

			float a = func->u.sa.samples[e0[0] * s0 + e0[1] * s1 + i];
			float b = func->u.sa.samples[e1[0] * s0 + e0[1] * s1 + i];
			float c = func->u.sa.samples[e0[0] * s0 + e1[1] * s1 + i];
			float d = func->u.sa.samples[e1[0] * s0 + e1[1] * s1 + i];

			float ab = a + (b - a) * efrac[0];
			float cd = c + (d - c) * efrac[0];
			float abcd = ab + (cd - ab) * efrac[1];

			out[i] = lerp(abcd, 0, 1, func->u.sa.decode[i][0], func->u.sa.decode[i][1]);
			out[i] = fz_clamp(out[i], func->range[i][0], func->range[i][1]);
		}

		else
		{
			x = interpolate_sample(func, scale, e0, e1, efrac, func->base.m - 1, i);
			out[i] = lerp(x, 0, 1, func->u.sa.decode[i][0], func->u.sa.decode[i][1]);
			out[i] = fz_clamp(out[i], func->range[i][0], func->range[i][1]);
		}
	}
}

/*
 * Exponential function
 */

static void
load_exponential_func(fz_context *ctx, pdf_function *func, pdf_obj *dict)
{
	pdf_obj *obj;
	int i;

	if (func->base.m > 1)
		fz_warn(ctx, "exponential functions have at most one input");
	func->base.m = 1;

	obj = pdf_dict_gets(dict, "N");
	func->u.e.n = pdf_to_real(obj);

	/* See exponential functions (PDF 1.7 section 3.9.2) */
	if (func->u.e.n != (int) func->u.e.n)
	{
		/* If N is non-integer, input values may never be negative */
		for (i = 0; i < func->base.m; i++)
			if (func->domain[i][0] < 0 || func->domain[i][1] < 0)
				fz_warn(ctx, "exponential function input domain includes illegal negative input values");
	}
	else if (func->u.e.n < 0)
	{
		/* if N is negative, input values may never be zero */
		for (i = 0; i < func->base.m; i++)
			if (func->domain[i][0] == 0 || func->domain[i][1] == 0 ||
				(func->domain[i][0] < 0 && func->domain[i][1] > 0))
				fz_warn(ctx, "exponential function input domain includes illegal input value zero");
	}

	for (i = 0; i < func->base.n; i++)
	{
		func->u.e.c0[i] = 0;
		func->u.e.c1[i] = 1;
	}

	obj = pdf_dict_gets(dict, "C0");
	if (pdf_is_array(obj))
	{
		int ranges = fz_mini(func->base.n, pdf_array_len(obj));
		if (ranges != func->base.n)
			fz_warn(ctx, "wrong number of C0 constants for exponential function");

		for (i = 0; i < ranges; i++)
			func->u.e.c0[i] = pdf_to_real(pdf_array_get(obj, i));
	}

	obj = pdf_dict_gets(dict, "C1");
	if (pdf_is_array(obj))
	{
		int ranges = fz_mini(func->base.n, pdf_array_len(obj));
		if (ranges != func->base.n)
			fz_warn(ctx, "wrong number of C1 constants for exponential function");

		for (i = 0; i < ranges; i++)
			func->u.e.c1[i] = pdf_to_real(pdf_array_get(obj, i));
	}
}

static void
eval_exponential_func(fz_context *ctx, pdf_function *func, float in, float *out)
{
	float x = in;
	float tmp;
	int i;

	x = fz_clamp(x, func->domain[0][0], func->domain[0][1]);

	/* Default output is zero, which is suitable for violated constraints */
	if ((func->u.e.n != (int)func->u.e.n && x < 0) || (func->u.e.n < 0 && x == 0))
		return;

	tmp = powf(x, func->u.e.n);
	for (i = 0; i < func->base.n; i++)
	{
		out[i] = func->u.e.c0[i] + tmp * (func->u.e.c1[i] - func->u.e.c0[i]);
		if (func->has_range)
			out[i] = fz_clamp(out[i], func->range[i][0], func->range[i][1]);
	}
}

/*
 * Stitching function
 */

static void
load_stitching_func(pdf_function *func, pdf_document *doc, pdf_obj *dict)
{
	fz_context *ctx = doc->ctx;
	fz_function **funcs;
	pdf_obj *obj;
	pdf_obj *sub;
	pdf_obj *num;
	int k;
	int i;

	func->u.st.k = 0;

	if (func->base.m > 1)
		fz_warn(ctx, "stitching functions have at most one input");
	func->base.m = 1;

	obj = pdf_dict_gets(dict, "Functions");
	if (!pdf_is_array(obj))
		fz_throw(ctx, FZ_ERROR_GENERIC, "stitching function has no input functions");

	fz_try(ctx)
	{
		pdf_mark_obj(obj);
		k = pdf_array_len(obj);

		func->u.st.funcs = fz_malloc_array(ctx, k, sizeof(fz_function*));
		func->u.st.bounds = fz_malloc_array(ctx, k - 1, sizeof(float));
		func->u.st.encode = fz_malloc_array(ctx, k * 2, sizeof(float));
		funcs = func->u.st.funcs;

		for (i = 0; i < k; i++)
		{
			sub = pdf_array_get(obj, i);
			funcs[i] = pdf_load_function(doc, sub, 1, func->base.n);

			func->base.size += fz_function_size(funcs[i]);
			func->u.st.k ++;

			if (funcs[i]->m != func->base.m)
				fz_warn(ctx, "wrong number of inputs for sub function %d", i);
			if (funcs[i]->n != func->base.n)
				fz_warn(ctx, "wrong number of outputs for sub function %d", i);
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	obj = pdf_dict_gets(dict, "Bounds");
	if (!pdf_is_array(obj))
		fz_throw(ctx, FZ_ERROR_GENERIC, "stitching function has no bounds");
	{
		if (pdf_array_len(obj) < k - 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "too few subfunction boundaries");
		if (pdf_array_len(obj) > k)
			fz_warn(ctx, "too many subfunction boundaries");

		for (i = 0; i < k - 1; i++)
		{
			num = pdf_array_get(obj, i);
			func->u.st.bounds[i] = pdf_to_real(num);
			if (i && func->u.st.bounds[i - 1] > func->u.st.bounds[i])
				fz_throw(ctx, FZ_ERROR_GENERIC, "subfunction %d boundary out of range", i);
		}

		if (k > 1 && (func->domain[0][0] > func->u.st.bounds[0] ||
			func->domain[0][1] < func->u.st.bounds[k - 2]))
			fz_warn(ctx, "subfunction boundaries outside of input mapping");
	}

	for (i = 0; i < k; i++)
	{
		func->u.st.encode[i * 2 + 0] = 0;
		func->u.st.encode[i * 2 + 1] = 0;
	}

	obj = pdf_dict_gets(dict, "Encode");
	if (pdf_is_array(obj))
	{
		int ranges = fz_mini(k, pdf_array_len(obj) / 2);
		if (ranges != k)
			fz_warn(ctx, "wrong number of stitching function input mappings");

		for (i = 0; i < ranges; i++)
		{
			func->u.st.encode[i * 2 + 0] = pdf_to_real(pdf_array_get(obj, i * 2 + 0));
			func->u.st.encode[i * 2 + 1] = pdf_to_real(pdf_array_get(obj, i * 2 + 1));
		}
	}
}

static void
eval_stitching_func(fz_context *ctx, pdf_function *func, float in, float *out)
{
	float low, high;
	int k = func->u.st.k;
	float *bounds = func->u.st.bounds;
	int i;

	in = fz_clamp(in, func->domain[0][0], func->domain[0][1]);

	for (i = 0; i < k - 1; i++)
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
		low = bounds[k - 2];
		high = func->domain[0][1];
	}
	else
	{
		low = bounds[i - 1];
		high = bounds[i];
	}

	in = lerp(in, low, high, func->u.st.encode[i * 2 + 0], func->u.st.encode[i * 2 + 1]);

	fz_eval_function(ctx, func->u.st.funcs[i], &in, 1, out, func->u.st.funcs[i]->n);
}

/*
 * Common
 */

static void
pdf_free_function_imp(fz_context *ctx, fz_storable *func_)
{
	pdf_function *func = (pdf_function *)func_;
	int i;

	switch (func->type)
	{
	case SAMPLE:
		fz_free(ctx, func->u.sa.samples);
		break;
	case EXPONENTIAL:
		break;
	case STITCHING:
		for (i = 0; i < func->u.st.k; i++)
			fz_drop_function(ctx, func->u.st.funcs[i]);
		fz_free(ctx, func->u.st.funcs);
		fz_free(ctx, func->u.st.bounds);
		fz_free(ctx, func->u.st.encode);
		break;
	case POSTSCRIPT:
		fz_free(ctx, func->u.p.code);
		break;
	}
	fz_free(ctx, func);
}

static void
pdf_eval_function(fz_context *ctx, fz_function *func_, float *in, float *out)
{
	pdf_function *func = (pdf_function *)func_;

	switch (func->type)
	{
	case SAMPLE: eval_sample_func(ctx, func, in, out); break;
	case EXPONENTIAL: eval_exponential_func(ctx, func, *in, out); break;
	case STITCHING: eval_stitching_func(ctx, func, *in, out); break;
	case POSTSCRIPT: eval_postscript_func(ctx, func, in, out); break;
	}
}

/*
 * Debugging prints
 */

#ifndef NDEBUG
static void
pdf_debug_indent(char *prefix, int level, char *suffix)
{
	int i;

	printf("%s", prefix);

	for (i = 0; i < level; i++)
		printf("\t");

	printf("%s", suffix);
}

static void
pdf_debug_ps_func_code(psobj *funccode, psobj *code, int level)
{
	int eof, wasop;

	pdf_debug_indent("", level, "{");

	/* Print empty blocks as { }, instead of separating braces on different lines. */
	if (code->type == PS_OPERATOR && code->u.op == PS_OP_RETURN)
	{
		printf(" } ");
		return;
	}

	pdf_debug_indent("\n", ++level, "");

	eof = 0;
	wasop = 0;
	while (!eof)
	{
		switch (code->type)
		{
		case PS_INT:
			if (wasop)
				pdf_debug_indent("\n", level, "");

			printf("%d ", code->u.i);
			wasop = 0;
			code++;
			break;

		case PS_REAL:
			if (wasop)
				pdf_debug_indent("\n", level, "");

			printf("%g ", code->u.f);
			wasop = 0;
			code++;
			break;

		case PS_OPERATOR:
			if (code->u.op == PS_OP_RETURN)
			{
				printf("\n");
				eof = 1;
			}
			else if (code->u.op == PS_OP_IF)
			{
				printf("\n");
				pdf_debug_ps_func_code(funccode, &funccode[(code + 2)->u.block], level);

				printf("%s", ps_op_names[code->u.op]);
				code = &funccode[(code + 3)->u.block];
				if (code->type != PS_OPERATOR || code->u.op != PS_OP_RETURN)
					pdf_debug_indent("\n", level, "");

				wasop = 0;
			}
			else if (code->u.op == PS_OP_IFELSE)
			{
				printf("\n");
				pdf_debug_ps_func_code(funccode, &funccode[(code + 2)->u.block], level);

				printf("\n");
				pdf_debug_ps_func_code(funccode, &funccode[(code + 1)->u.block], level);

				printf("%s", ps_op_names[code->u.op]);
				code = &funccode[(code + 3)->u.block];
				if (code->type != PS_OPERATOR || code->u.op != PS_OP_RETURN)
					pdf_debug_indent("\n", level, "");

				wasop = 0;
			}
			else
			{
				printf("%s ", ps_op_names[code->u.op]);
				code++;
				wasop = 1;
			}
			break;
		}
	}

	pdf_debug_indent("", --level, "} ");
}

static void
pdf_debug_function_imp(fz_function *func_, int level)
{
	int i;
	pdf_function *func = (pdf_function *)func_;

	pdf_debug_indent("", level, "function {\n");

	pdf_debug_indent("", ++level, "");
	switch (func->type)
	{
	case SAMPLE:
		printf("sampled");
		break;
	case EXPONENTIAL:
		printf("exponential");
		break;
	case STITCHING:
		printf("stitching");
		break;
	case POSTSCRIPT:
		printf("postscript");
		break;
	}

	pdf_debug_indent("\n", level, "");
	printf("%d input -> %d output\n", func->base.m, func->base.n);

	pdf_debug_indent("", level, "domain ");
	for (i = 0; i < func->base.m; i++)
		printf("%g %g ", func->domain[i][0], func->domain[i][1]);
	printf("\n");

	if (func->has_range)
	{
		pdf_debug_indent("", level, "range ");
		for (i = 0; i < func->base.n; i++)
			printf("%g %g ", func->range[i][0], func->range[i][1]);
		printf("\n");
	}

	switch (func->type)
	{
	case SAMPLE:
		pdf_debug_indent("", level, "");
		printf("bps: %d\n", func->u.sa.bps);

		pdf_debug_indent("", level, "");
		printf("size: [ ");
		for (i = 0; i < func->base.m; i++)
			printf("%d ", func->u.sa.size[i]);
		printf("]\n");

		pdf_debug_indent("", level, "");
		printf("encode: [ ");
		for (i = 0; i < func->base.m; i++)
			printf("%g %g ", func->u.sa.encode[i][0], func->u.sa.encode[i][1]);
		printf("]\n");

		pdf_debug_indent("", level, "");
		printf("decode: [ ");
		for (i = 0; i < func->base.m; i++)
			printf("%g %g ", func->u.sa.decode[i][0], func->u.sa.decode[i][1]);
		printf("]\n");
		break;

	case EXPONENTIAL:
		pdf_debug_indent("", level, "");
		printf("n: %g\n", func->u.e.n);

		pdf_debug_indent("", level, "");
		printf("c0: [ ");
		for (i = 0; i < func->base.n; i++)
			printf("%g ", func->u.e.c0[i]);
		printf("]\n");

		pdf_debug_indent("", level, "");
		printf("c1: [ ");
		for (i = 0; i < func->base.n; i++)
			printf("%g ", func->u.e.c1[i]);
		printf("]\n");
		break;

	case STITCHING:
		pdf_debug_indent("", level, "");
		printf("%d functions\n", func->u.st.k);

		pdf_debug_indent("", level, "");
		printf("bounds: [ ");
		for (i = 0; i < func->u.st.k - 1; i++)
			printf("%g ", func->u.st.bounds[i]);
		printf("]\n");

		pdf_debug_indent("", level, "");
		printf("encode: [ ");
		for (i = 0; i < func->u.st.k * 2; i++)
			printf("%g ", func->u.st.encode[i]);
		printf("]\n");

		for (i = 0; i < func->u.st.k; i++)
			pdf_debug_function_imp(func->u.st.funcs[i], level);
		break;

	case POSTSCRIPT:
		pdf_debug_ps_func_code(func->u.p.code, func->u.p.code, level);
		printf("\n");
		break;
	}

	pdf_debug_indent("", --level, "}\n");
}

void
pdf_debug_function(fz_function *func)
{
	pdf_debug_function_imp(func, 0);
}
#endif

fz_function *
pdf_load_function(pdf_document *doc, pdf_obj *dict, int in, int out)
{
	fz_context *ctx = doc->ctx;
	pdf_function *func;
	pdf_obj *obj;
	int i;

	if (pdf_obj_marked(dict))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Recursion in function definition");

	if ((func = pdf_find_item(ctx, pdf_free_function_imp, dict)))
	{
		return (fz_function *)func;
	}

	func = fz_malloc_struct(ctx, pdf_function);
	FZ_INIT_STORABLE(&func->base, 1, pdf_free_function_imp);
	func->base.size = sizeof(*func);
	func->base.evaluate = pdf_eval_function;
#ifndef NDEBUG
	func->base.debug = pdf_debug_function;
#endif

	obj = pdf_dict_gets(dict, "FunctionType");
	func->type = pdf_to_int(obj);

	/* required for all */
	obj = pdf_dict_gets(dict, "Domain");
	func->base.m = fz_clampi(pdf_array_len(obj) / 2, 1, FZ_FN_MAXM);
	for (i = 0; i < func->base.m; i++)
	{
		func->domain[i][0] = pdf_to_real(pdf_array_get(obj, i * 2 + 0));
		func->domain[i][1] = pdf_to_real(pdf_array_get(obj, i * 2 + 1));
	}

	/* required for type0 and type4, optional otherwise */
	obj = pdf_dict_gets(dict, "Range");
	if (pdf_is_array(obj))
	{
		func->has_range = 1;
		func->base.n = fz_clampi(pdf_array_len(obj) / 2, 1, FZ_FN_MAXN);
		for (i = 0; i < func->base.n; i++)
		{
			func->range[i][0] = pdf_to_real(pdf_array_get(obj, i * 2 + 0));
			func->range[i][1] = pdf_to_real(pdf_array_get(obj, i * 2 + 1));
		}
	}
	else
	{
		func->has_range = 0;
		func->base.n = out;
	}

	if (func->base.m != in)
		fz_warn(ctx, "wrong number of function inputs");
	if (func->base.n != out)
		fz_warn(ctx, "wrong number of function outputs");

	fz_try(ctx)
	{
		switch (func->type)
		{
		case SAMPLE:
			load_sample_func(func, doc, dict, pdf_to_num(dict), pdf_to_gen(dict));
			break;

		case EXPONENTIAL:
			load_exponential_func(ctx, func, dict);
			break;

		case STITCHING:
			load_stitching_func(func, doc, dict);
			break;

		case POSTSCRIPT:
			load_postscript_func(func, doc, dict, pdf_to_num(dict), pdf_to_gen(dict));
			break;

		default:
			fz_free(ctx, func);
			fz_throw(ctx, FZ_ERROR_GENERIC, "unknown function type (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict));
		}

		pdf_store_item(ctx, dict, func, func->base.size);
	}
	fz_catch(ctx)
	{
		int type = func->type;
		fz_drop_function(ctx, (fz_function *)func);
		fz_rethrow_message(ctx, "cannot load %s function (%d %d R)",
					type == SAMPLE ? "sampled" :
					type == EXPONENTIAL ? "exponential" :
					type == STITCHING ? "stitching" :
					type == POSTSCRIPT ? "calculator" :
					"unknown",
					pdf_to_num(dict), pdf_to_gen(dict));
	}

	return (fz_function *)func;
}
