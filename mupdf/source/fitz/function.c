#include "mupdf/fitz.h"

void
fz_eval_function(fz_context *ctx, fz_function *func, const float *in, int inlen, float *out, int outlen)
{
	float fakein[FZ_FN_MAXM];
	float fakeout[FZ_FN_MAXN];
	int i;

	if (inlen < func->m)
	{
		/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=696012 */
		for (i = 0; i < inlen; ++i)
			fakein[i] = in[i];
		for (; i < func->m; ++i)
			fakein[i] = 0;
		in = fakein;
	}

	if (outlen < func->n)
	{
		func->evaluate(ctx, func, in, fakeout);
		/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=696012 */
		for (i = 0; i < outlen; ++i)
			out[i] = fakeout[i];
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
