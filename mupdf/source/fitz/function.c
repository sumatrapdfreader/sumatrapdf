#include "mupdf/fitz.h"

void
fz_eval_function(fz_context *ctx, fz_function *func, const float *in, int inlen, float *out, int outlen)
{
	float fakein[FZ_FN_MAXM];
	float fakeout[FZ_FN_MAXN];
	int i;

	if (inlen < func->m)
	{
		for (i = 0; i < func->m; ++i)
			fakein[i] = in[i];
		for (; i < inlen; ++i)
			fakein[i] = 0;
		in = fakein;
	}

	if (outlen < func->n)
	{
		func->evaluate(ctx, func, in, fakeout);
		for (i = 0; i < func->n; ++i)
			out[i] = fakeout[i];
		for (; i < outlen; ++i)
			out[i] = 0;
	}
	else
	{
		func->evaluate(ctx, func, in, out);
		for (i = func->n; i < outlen; ++i)
			out[i] = 0;
	}
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
