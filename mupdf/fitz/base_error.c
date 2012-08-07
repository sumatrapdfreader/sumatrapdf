#include "fitz-internal.h"

/* Warning context */

void fz_var_imp(void *var)
{
	var = var; /* Do nothing */
}

void fz_flush_warnings(fz_context *ctx)
{
	if (ctx->warn->count > 1)
	{
		fprintf(stderr, "warning: ... repeated %d times ...\n", ctx->warn->count);
		LOGE("warning: ... repeated %d times ...\n", ctx->warn->count);
	}
	ctx->warn->message[0] = 0;
	ctx->warn->count = 0;
}

/* SumatraPDF: add filename and line number to errors and warnings */
void fz_warn_imp(fz_context *ctx, char *file, int line, char *fmt, ...)
{
	va_list ap;
	char buf[sizeof ctx->warn->message];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	if (!strcmp(buf, ctx->warn->message))
	{
		ctx->warn->count++;
	}
	else
	{
		fz_flush_warnings(ctx);
		fprintf(stderr, "- %s:%d: %s\n", file, line, buf);
		LOGE("warning: %s\n", buf);
		fz_strlcpy(ctx->warn->message, buf, sizeof ctx->warn->message);
		ctx->warn->count = 1;
	}
}

/* Error context */

int fz_too_deeply_nested(fz_context *ctx)
{
	fz_error_context *ex = ctx->error;
	return ex->top + 1 >= nelem(ex->stack);
}

/* SumatraPDF: force crash so that we get crash report */
inline void fz_crash_abort()
{
	char *p = NULL;
	*p = 0;
}

static void throw(fz_error_context *ex)
{
	if (ex->top >= 0) {
		fz_longjmp(ex->stack[ex->top].buffer, 1);
	} else {
		fprintf(stderr, "uncaught exception: %s\n", ex->message);
		LOGE("uncaught exception: %s\n", ex->message);
		fz_crash_abort();
	}
}

void fz_push_try(fz_error_context *ex)
{
	assert(ex);
	if (ex->top + 1 >= nelem(ex->stack))
	{
		fprintf(stderr, "exception stack overflow!\n");
		fz_crash_abort();
	}
	ex->top++;
}

char *fz_caught(fz_context *ctx)
{
	assert(ctx);
	assert(ctx->error);
	return ctx->error->message;
}

/* SumatraPDF: add filename and line number to errors and warnings */
void fz_throw_imp(fz_context *ctx, char *file, int line, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(ctx->error->message, sizeof ctx->error->message, fmt, args);
	va_end(args);

	fz_flush_warnings(ctx);
	fprintf(stderr, "! %s:%d: %s\n", file, line, ctx->error->message);
	LOGE("error: %s\n", ctx->error->message);

	throw(ctx->error);
}

void fz_rethrow(fz_context *ctx)
{
	throw(ctx->error);
}
