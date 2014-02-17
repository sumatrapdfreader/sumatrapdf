#ifndef MUPDF_FITZ_FUNCTION_H
#define MUPDF_FITZ_FUNCTION_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/colorspace.h"

/*
 * The generic function support.
 */

typedef struct fz_function_s fz_function;

void fz_eval_function(fz_context *ctx, fz_function *func, const float *in, int inlen, float *out, int outlen);
fz_function *fz_keep_function(fz_context *ctx, fz_function *func);
void fz_drop_function(fz_context *ctx, fz_function *func);
unsigned int fz_function_size(fz_function *func);
#ifndef NDEBUG
void pdf_debug_function(fz_function *func);
#endif

enum
{
	FZ_FN_MAXN = FZ_MAX_COLORS,
	FZ_FN_MAXM = FZ_MAX_COLORS
};

struct fz_function_s
{
	fz_storable storable;
	unsigned int size;
	int m;					/* number of input values */
	int n;					/* number of output values */
	void (*evaluate)(fz_context *ctx, fz_function *func, const float *in, float *out);
#ifndef NDEBUG
	void (*debug)(fz_function *func);
#endif
};

#endif
