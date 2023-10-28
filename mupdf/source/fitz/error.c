// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#ifndef NDEBUG
#define USE_OUTPUT_DEBUG_STRING
#include <windows.h>
#endif
#endif

#ifdef __ANDROID__
#define USE_ANDROID_LOG
#include <android/log.h>
#endif

void fz_default_error_callback(void *user, const char *message)
{
	fprintf(stderr, "error: %s\n", message);
#ifdef USE_OUTPUT_DEBUG_STRING
	OutputDebugStringA("error: ");
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
#endif
#ifdef USE_ANDROID_LOG
	__android_log_print(ANDROID_LOG_ERROR, "libmupdf", "%s", message);
#endif
}

void fz_default_warning_callback(void *user, const char *message)
{
	fprintf(stderr, "warning: %s\n", message);
#ifdef USE_OUTPUT_DEBUG_STRING
	OutputDebugStringA("warning: ");
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
#endif
#ifdef USE_ANDROID_LOG
	__android_log_print(ANDROID_LOG_WARN, "libmupdf", "%s", message);
#endif
}

/* Warning context */

void fz_set_warning_callback(fz_context *ctx, fz_warning_cb *warning_cb, void *user)
{
	ctx->warn.print_user = user;
	ctx->warn.print = warning_cb;
}

fz_warning_cb *fz_warning_callback(fz_context *ctx, void **user)
{
	if (user)
		*user = ctx->warn.print_user;
	return ctx->warn.print;
}

void fz_var_imp(void *var)
{
	/* Do nothing */
}

void fz_flush_warnings(fz_context *ctx)
{
	if (ctx->warn.count > 1)
	{
		char buf[50];
		fz_snprintf(buf, sizeof buf, "... repeated %d times...", ctx->warn.count);
		if (ctx->warn.print)
			ctx->warn.print(ctx->warn.print_user, buf);
	}
	ctx->warn.message[0] = 0;
	ctx->warn.count = 0;
}

void (fz_vwarn)(fz_context *ctx, const char *fmt, va_list ap)
{
	char buf[sizeof ctx->warn.message];

	fz_vsnprintf(buf, sizeof buf, fmt, ap);
	buf[sizeof(buf) - 1] = 0;

	if (!strcmp(buf, ctx->warn.message))
	{
		ctx->warn.count++;
	}
	else
	{
		fz_flush_warnings(ctx);
		if (ctx->warn.print)
			ctx->warn.print(ctx->warn.print_user, buf);
		fz_strlcpy(ctx->warn.message, buf, sizeof ctx->warn.message);
		ctx->warn.count = 1;
	}
}

void (fz_warn)(fz_context *ctx, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fz_vwarn(ctx, fmt, ap);
	va_end(ap);
}

#if FZ_VERBOSE_EXCEPTIONS
void fz_vwarnFL(fz_context *ctx, const char *file, int line, const char *fmt, va_list ap)
{
	char buf[sizeof ctx->warn.message];

	fz_vsnprintf(buf, sizeof buf, fmt, ap);
	buf[sizeof(buf) - 1] = 0;

	if (!strcmp(buf, ctx->warn.message))
	{
		ctx->warn.count++;
	}
	else
	{
		fz_flush_warnings(ctx);
		if (ctx->warn.print)
			ctx->warn.print(ctx->warn.print_user, buf);
		fz_strlcpy(ctx->warn.message, buf, sizeof ctx->warn.message);
		ctx->warn.count = 1;
	}
}

void fz_warnFL(fz_context *ctx, const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fz_vwarnFL(ctx, file, line, fmt, ap);
	va_end(ap);
}
#endif

/* Error context */

void fz_set_error_callback(fz_context *ctx, fz_error_cb *error_cb, void *user)
{
	ctx->error.print_user = user;
	ctx->error.print = error_cb;
}

fz_error_cb *fz_error_callback(fz_context *ctx, void **user)
{
	if (user)
		*user = ctx->error.print_user;
	return ctx->error.print;
}

/* When we first setjmp, state is set to 0. Whenever we throw, we add 2 to
 * this state. Whenever we enter the always block, we add 1.
 *
 * fz_push_try sets state to 0.
 * If (fz_throw called within fz_try)
 *     fz_throw makes state = 2.
 *     If (no always block present)
 *         enter catch region with state = 2. OK.
 *     else
 *         fz_always entered as state < 3; Makes state = 3;
 *         if (fz_throw called within fz_always)
 *             fz_throw makes state = 5
 *             fz_always is not reentered.
 *             catch region entered with state = 5. OK.
 *         else
 *             catch region entered with state = 3. OK
 * else
 *     if (no always block present)
 *         catch region not entered as state = 0. OK.
 *     else
 *         fz_always entered as state < 3. makes state = 1
 *         if (fz_throw called within fz_always)
 *             fz_throw makes state = 3;
 *             fz_always NOT entered as state >= 3
 *             catch region entered with state = 3. OK.
 *         else
 *             catch region entered with state = 1.
 */

FZ_NORETURN static void throw(fz_context *ctx, int code)
{
	if (ctx->error.top > ctx->error.stack_base)
	{
		ctx->error.top->state += 2;
		if (ctx->error.top->code != FZ_ERROR_NONE)
			fz_warn(ctx, "clobbering previous error code and message (throw in always block?)");
		ctx->error.top->code = code;
		/* SumatraPDF: https://fossies.org/linux/tcsh/win32/fork.c#l_212
		https://stackoverflow.com/questions/26605063/an-invalid-or-unaligned-stack-was-encountered-during-an-unwind-operation
		*/
		#ifdef _M_AMD64
		((_JUMP_BUFFER*)&ctx->error.top->buffer)->Frame = 0;
		#endif
		fz_longjmp(ctx->error.top->buffer, 1);
	}
	else
	{
		fz_flush_warnings(ctx);
		if (ctx->error.print)
			ctx->error.print(ctx->error.print_user, "aborting process from uncaught error!");
		/* SumatraPDF: crash rather than exit */
		char* p = 0;
		*p = 0;
		exit(EXIT_FAILURE);
	}
}

fz_jmp_buf *fz_push_try(fz_context *ctx)
{
	/* If we would overflow the exception stack, throw an exception instead
	 * of entering the try block. We assume that we always have room for
	 * 1 extra level on the stack here - i.e. we throw the error on us
	 * starting to use the last level. */
	if (ctx->error.top + 2 >= ctx->error.stack_base + nelem(ctx->error.stack))
	{
		fz_strlcpy(ctx->error.message, "exception stack overflow!", sizeof ctx->error.message);

		fz_flush_warnings(ctx);
		if (ctx->error.print)
			ctx->error.print(ctx->error.print_user, ctx->error.message);

		/* We need to arrive in the always/catch block as if throw had taken place. */
		ctx->error.top++;
		ctx->error.top->state = 2;
		ctx->error.top->code = FZ_ERROR_GENERIC;
	}
	else
	{
		ctx->error.top++;
		ctx->error.top->state = 0;
		ctx->error.top->code = FZ_ERROR_NONE;
	}
	return &ctx->error.top->buffer;
}

int fz_do_try(fz_context *ctx)
{
#ifdef __COVERITY__
	return 1;
#else
	return ctx->error.top->state == 0;
#endif
}

int fz_do_always(fz_context *ctx)
{
#ifdef __COVERITY__
	return 1;
#else
	if (ctx->error.top->state < 3)
	{
		ctx->error.top->state++;
		return 1;
	}
	return 0;
#endif
}

int (fz_do_catch)(fz_context *ctx)
{
	ctx->error.errcode = ctx->error.top->code;
	return (ctx->error.top--)->state > 1;
}

int fz_caught(fz_context *ctx)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	return ctx->error.errcode;
}

const char *fz_caught_message(fz_context *ctx)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	return ctx->error.message;
}

void (fz_log_error_printf)(fz_context *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(fz_vlog_error_printf)(ctx, fmt, ap);
	va_end(ap);
}

void (fz_vlog_error_printf)(fz_context *ctx, const char *fmt, va_list ap)
{
	char message[256];

	fz_flush_warnings(ctx);
	if (ctx->error.print)
	{
		fz_vsnprintf(message, sizeof message, fmt, ap);
		message[sizeof(message) - 1] = 0;

		ctx->error.print(ctx->error.print_user, message);
	}
}

void (fz_log_error)(fz_context *ctx, const char *str)
{
	fz_flush_warnings(ctx);
	if (ctx->error.print)
		ctx->error.print(ctx->error.print_user, str);
}

/* coverity[+kill] */
FZ_NORETURN void (fz_vthrow)(fz_context *ctx, int code, const char *fmt, va_list ap)
{
	fz_vsnprintf(ctx->error.message, sizeof ctx->error.message, fmt, ap);
	ctx->error.message[sizeof(ctx->error.message) - 1] = 0;

	throw(ctx, code);
}

/* coverity[+kill] */
FZ_NORETURN void (fz_throw)(fz_context *ctx, int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fz_vthrow(ctx, code, fmt, ap);
	va_end(ap);
}

/* coverity[+kill] */
FZ_NORETURN void (fz_rethrow)(fz_context *ctx)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	throw(ctx, ctx->error.errcode);
}

void (fz_morph_error)(fz_context *ctx, int fromerr, int toerr)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	if (ctx->error.errcode == fromerr)
		ctx->error.errcode = toerr;
}

void (fz_rethrow_if)(fz_context *ctx, int err)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	if (ctx->error.errcode == err)
		fz_rethrow(ctx);
}

void (fz_rethrow_unless)(fz_context *ctx, int err)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	if (ctx->error.errcode != err)
		fz_rethrow(ctx);
}

#if FZ_VERBOSE_EXCEPTIONS
static const char *
errcode_to_string(int exc)
{
	switch (exc)
	{
	case FZ_ERROR_NONE:
		return "NONE";
	case FZ_ERROR_MEMORY:
		return "MEMORY";
	case FZ_ERROR_GENERIC:
		return "GENERIC";
	case FZ_ERROR_SYNTAX:
		return "SYNTAX";
	case FZ_ERROR_MINOR:
		return "MINOR";
	case FZ_ERROR_TRYLATER:
		return "TRYLATER";
	case FZ_ERROR_ABORT:
		return "ABORT";
	case FZ_ERROR_REPAIRED:
		return "REPAIRED";
	case FZ_ERROR_COUNT:
		return "COUNT";
	default:
		return "<Invalid>";
	}
}

int fz_do_catchFL(fz_context *ctx, const char *file, int line)
{
	int rc = (fz_do_catch)(ctx);
	if (rc)
		(fz_log_error_printf)(ctx, "%s:%d: Catching", file, line);
	return rc;
}


void fz_log_error_printfFL(fz_context *ctx, const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fz_vlog_error_printfFL(ctx, file, line, fmt, ap);
	va_end(ap);
}

void fz_vlog_error_printfFL(fz_context *ctx, const char *file, int line, const char *fmt, va_list ap)
{
	char message[256];

	fz_flush_warnings(ctx);
	if (ctx->error.print)
	{
		fz_vsnprintf(message, sizeof message, fmt, ap);
		message[sizeof(message) - 1] = 0;

		fz_log_errorFL(ctx, file, line, message);
	}
}

void fz_log_errorFL(fz_context *ctx, const char *file, int line, const char *str)
{
	char message[256];

	fz_flush_warnings(ctx);
	if (ctx->error.print)
	{
		fz_snprintf(message, sizeof message, "%s:%d '%s'", file, line, str);
		message[sizeof(message) - 1] = 0;
		ctx->error.print(ctx->error.print_user, message);
	}
}

/* coverity[+kill] */
FZ_NORETURN void fz_vthrowFL(fz_context *ctx, const char *file, int line, int code, const char *fmt, va_list ap)
{
	fz_vsnprintf(ctx->error.message, sizeof ctx->error.message, fmt, ap);
	ctx->error.message[sizeof(ctx->error.message) - 1] = 0;

	(fz_log_error_printf)(ctx, "%s:%d: Throwing %s '%s'", file, line, errcode_to_string(code), ctx->error.message);

	throw(ctx, code);
}

/* coverity[+kill] */
FZ_NORETURN void fz_throwFL(fz_context *ctx, const char *file, int line, int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fz_vthrowFL(ctx, file, line, code, fmt, ap);
	va_end(ap);
}

/* coverity[+kill] */
FZ_NORETURN void fz_rethrowFL(fz_context *ctx, const char *file, int line)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	(fz_log_error_printf)(ctx, "%s:%d: Rethrowing", file, line);
	throw(ctx, ctx->error.errcode);
}

void fz_morph_errorFL(fz_context *ctx, const char *file, int line, int fromerr, int toerr)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	if (ctx->error.errcode == fromerr)
	{
		(fz_log_error_printf)(ctx, "%s:%d: Morphing %s->%s", file, line, errcode_to_string(fromerr), errcode_to_string(toerr));
		ctx->error.errcode = toerr;
	}
}

void fz_rethrow_unlessFL(fz_context *ctx, const char *file, int line, int err)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	if (ctx->error.errcode != err)
	{
		(fz_log_error_printf)(ctx, "%s:%d: Rethrowing", file, line);
		(fz_rethrow)(ctx);
	}
}

void fz_rethrow_ifFL(fz_context *ctx, const char *file, int line, int err)
{
	assert(ctx && ctx->error.errcode >= FZ_ERROR_NONE);
	if (ctx->error.errcode == err)
	{
		(fz_log_error_printf)(ctx, "%s:%d: Rethrowing", file, line);
		(fz_rethrow)(ctx);
	}
}
#endif

void fz_start_throw_on_repair(fz_context *ctx)
{
	fz_lock(ctx, FZ_LOCK_ALLOC);
	ctx->throw_on_repair++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

void fz_end_throw_on_repair(fz_context *ctx)
{
	fz_lock(ctx, FZ_LOCK_ALLOC);
	ctx->throw_on_repair--;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}
