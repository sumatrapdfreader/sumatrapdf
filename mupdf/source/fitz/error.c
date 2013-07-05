#include "mupdf/fitz.h"

#if defined(_WIN32) && !defined(NDEBUG)
#define USE_OUTPUT_DEBUG_STRING
#endif

#ifdef USE_OUTPUT_DEBUG_STRING
#include <windows.h>
#endif

/* Warning context */

void fz_var_imp(void *var)
{
	UNUSED(var); /* Do nothing */
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
void fz_warn_imp(fz_context *ctx, char *file, int line, const char *fmt, ...)
{
	va_list ap;
	char buf[sizeof ctx->warn->message];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
#ifdef USE_OUTPUT_DEBUG_STRING
	OutputDebugStringA(buf);
	OutputDebugStringA("\n");
#endif

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

/* When we first setjmp, code is set to 0. Whenever we throw, we add 2 to
 * this code. Whenever we enter the always block, we add 1.
 *
 * fz_push_try sets code to 0.
 * If (fz_throw called within fz_try)
 *     fz_throw makes code = 2.
 *     If (no always block present)
 *         enter catch region with code = 2. OK.
 *     else
 *         fz_always entered as code < 3; Makes code = 3;
 *         if (fz_throw called within fz_always)
 *             fz_throw makes code = 5
 *             fz_always is not reentered.
 *             catch region entered with code = 5. OK.
 *         else
 *             catch region entered with code = 3. OK
 * else
 *     if (no always block present)
 *         catch region not entered as code = 0. OK.
 *     else
 *         fz_always entered as code < 3. makes code = 1
 *         if (fz_throw called within fz_always)
 *             fz_throw makes code = 3;
 *             fz_always NOT entered as code >= 3
 *             catch region entered with code = 3. OK.
 *         else
 *             catch region entered with code = 1.
 */

/* SumatraPDF: force crash so that we get crash report */
inline void fz_crash_abort()
{
	char *p = NULL;
	*p = 0;
}

FZ_NORETURN static void throw(fz_error_context *ex)
{
	if (ex->top >= 0)
	{
		fz_longjmp(ex->stack[ex->top].buffer, ex->stack[ex->top].code + 2);
	}
	else
	{
		fprintf(stderr, "uncaught exception: %s\n", ex->message);
		LOGE("uncaught exception: %s\n", ex->message);
#ifdef USE_OUTPUT_DEBUG_STRING
		OutputDebugStringA("uncaught exception: ");
		OutputDebugStringA(ex->message);
		OutputDebugStringA("\n");
#endif
		fz_crash_abort();
	}
}

int fz_push_try(fz_error_context *ex)
{
	assert(ex);
	ex->top++;
	/* Normal case, get out of here quick */
	if (ex->top < nelem(ex->stack)-1)
		return 1; /* We exit here, and the setjmp sets the code to 0 */
	/* We reserve the top slot on the exception stack purely to cope with
	 * the case when we overflow. If we DO hit this, then we 'throw'
	 * immediately - returning 0 stops the setjmp happening and takes us
	 * direct to the always/catch clauses. */
	assert(ex->top == nelem(ex->stack)-1);
	strcpy(ex->message, "exception stack overflow!");
	ex->stack[ex->top].code = 2;
	/* SumatraPDF: add filename and line number to errors and warnings */
	fprintf(stderr, "! %s:%d: %s\n", __FILE__, __LINE__, ex->message);
	LOGE("error: %s\n", ex->message);
	return 0;
}

int fz_caught(fz_context *ctx)
{
	assert(ctx && ctx->error && ctx->error->errcode >= FZ_ERROR_NONE);
	return ctx->error->errcode;
}

const char *fz_caught_message(fz_context *ctx)
{
	assert(ctx && ctx->error && ctx->error->errcode >= FZ_ERROR_NONE);
	return ctx->error->message;
}

/* SumatraPDF: add filename and line number to errors and warnings */
void fz_throw_imp(fz_context *ctx, char *file, int line, int code, const char *fmt, ...)
{
	va_list args;
	ctx->error->errcode = code;
	va_start(args, fmt);
	vsnprintf(ctx->error->message, sizeof ctx->error->message, fmt, args);
	va_end(args);

	fz_flush_warnings(ctx);
	fprintf(stderr, "! %s:%d: %s\n", file, line, ctx->error->message);
	LOGE("error: %s\n", ctx->error->message);
#ifdef USE_OUTPUT_DEBUG_STRING
	OutputDebugStringA("error: ");
	OutputDebugStringA(ctx->error->message);
	OutputDebugStringA("\n");
#endif

	throw(ctx->error);
}

void fz_rethrow(fz_context *ctx)
{
	assert(ctx && ctx->error && ctx->error->errcode >= FZ_ERROR_NONE);
	throw(ctx->error);
}

/* SumatraPDF: add filename and line number to errors and warnings */
void fz_rethrow_message_imp(fz_context *ctx, char *file, int line, const char *fmt, ...)
{
	va_list args;

	assert(ctx && ctx->error && ctx->error->errcode >= FZ_ERROR_NONE);

	va_start(args, fmt);
	vsnprintf(ctx->error->message, sizeof ctx->error->message, fmt, args);
	va_end(args);

	fz_flush_warnings(ctx);
	fprintf(stderr, "! %s:%d: %s\n", file, line, ctx->error->message);
	LOGE("error: %s\n", ctx->error->message);
#ifdef USE_OUTPUT_DEBUG_STRING
	OutputDebugStringA("error: ");
	OutputDebugStringA(ctx->error->message);
	OutputDebugStringA("\n");
#endif

	throw(ctx->error);
}
