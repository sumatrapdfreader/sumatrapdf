#include "mupdf/fitz.h"

void
fz_eval_function(fz_context *ctx, fz_function *func, float *in_, int inlen, float *out_, int outlen)
{
	float fakein[FZ_FN_MAXM];
	float fakeout[FZ_FN_MAXN];
	float *in = in_;
	float *out = out_;

	if (inlen < func->m)
	{
		in = fakein;
		memset(in, 0, sizeof(float) * func->m);
		memcpy(in, in_, sizeof(float) * inlen);
	}

	if (outlen < func->n)
	{
		out = fakeout;
		memset(out, 0, sizeof(float) * func->n);
	}
	else
		memset(out, 0, sizeof(float) * outlen);

	func->evaluate(ctx, func, in, out);

	if (outlen < func->n)
		memcpy(out_, out, sizeof(float) * outlen);
}

fz_function *
fz_keep_function(fz_context *ctx, fz_function *func)
{
	return (fz_function *)fz_keep_storable(ctx, &func->storable);
}

void
fz_drop_function(fz_context *ctx, fz_function *func)
{
	fz_drop_storable(ctx, &func->storable);
}

unsigned int
fz_function_size(fz_function *func)
{
	return (func ? func->size : 0);
}
