// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#include "context-imp.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


struct fz_style_context
{
	int refs;
	char *user_css;
	int use_document_css;
};

static void fz_new_style_context(fz_context *ctx)
{
	if (ctx)
	{
		ctx->style = fz_malloc_struct(ctx, fz_style_context);
		ctx->style->refs = 1;
		ctx->style->user_css = NULL;
		ctx->style->use_document_css = 1;
	}
}

static fz_style_context *fz_keep_style_context(fz_context *ctx)
{
	if (!ctx)
		return NULL;
	return fz_keep_imp(ctx, ctx->style, &ctx->style->refs);
}

static void fz_drop_style_context(fz_context *ctx)
{
	if (!ctx)
		return;
	if (fz_drop_imp(ctx, ctx->style, &ctx->style->refs))
	{
		fz_free(ctx, ctx->style->user_css);
		fz_free(ctx, ctx->style);
	}
}

void fz_set_use_document_css(fz_context *ctx, int use)
{
	ctx->style->use_document_css = use;
}

int fz_use_document_css(fz_context *ctx)
{
	return ctx->style->use_document_css;
}

void fz_set_user_css(fz_context *ctx, const char *user_css)
{
	fz_free(ctx, ctx->style->user_css);
	ctx->style->user_css = user_css ? fz_strdup(ctx, user_css) : NULL;
}

const char *fz_user_css(fz_context *ctx)
{
	return ctx->style->user_css;
}

void fz_load_user_css(fz_context *ctx, const char *filename)
{
	fz_buffer *buf = NULL;
	fz_var(buf);
	fz_try(ctx)
	{
		buf = fz_read_file(ctx, filename);
		fz_set_user_css(ctx, fz_string_from_buffer(ctx, buf));
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fz_warn(ctx, "cannot read user css file");
	}
}

static void fz_new_tuning_context(fz_context *ctx)
{
	if (ctx)
	{
		ctx->tuning = fz_malloc_struct(ctx, fz_tuning_context);
		ctx->tuning->refs = 1;
		ctx->tuning->image_decode = fz_default_image_decode;
		ctx->tuning->image_scale = fz_default_image_scale;
	}
}

static fz_tuning_context *fz_keep_tuning_context(fz_context *ctx)
{
	if (!ctx)
		return NULL;
	return fz_keep_imp(ctx, ctx->tuning, &ctx->tuning->refs);
}

static void fz_drop_tuning_context(fz_context *ctx)
{
	if (!ctx)
		return;
	if (fz_drop_imp(ctx, ctx->tuning, &ctx->tuning->refs))
	{
		fz_free(ctx, ctx->tuning);
	}
}

void fz_tune_image_decode(fz_context *ctx, fz_tune_image_decode_fn *image_decode, void *arg)
{
	ctx->tuning->image_decode = image_decode ? image_decode : fz_default_image_decode;
	ctx->tuning->image_decode_arg = arg;
}

void fz_tune_image_scale(fz_context *ctx, fz_tune_image_scale_fn *image_scale, void *arg)
{
	ctx->tuning->image_scale = image_scale ? image_scale : fz_default_image_scale;
	ctx->tuning->image_scale_arg = arg;
}

static void fz_init_random_context(fz_context *ctx)
{
	if (!ctx)
		return;

	ctx->seed48[0] = 0;
	ctx->seed48[1] = 0;
	ctx->seed48[2] = 0;
	ctx->seed48[3] = 0xe66d;
	ctx->seed48[4] = 0xdeec;
	ctx->seed48[5] = 0x5;
	ctx->seed48[6] = 0xb;

	fz_srand48(ctx, (uint32_t)time(NULL));
}

void
fz_drop_context(fz_context *ctx)
{
	int free_master = 0;
	int call_log = 0;

	if (!ctx)
		return;

	if (ctx->error.errcode)
	{
		fz_flush_warnings(ctx);
		fz_warn(ctx, "UNHANDLED EXCEPTION!");
		fz_report_error(ctx);
#ifdef CLUSTER
		abort();
#endif
	}


	assert(ctx->master);
	fz_lock(ctx, FZ_LOCK_ALLOC);
	ctx->master->context_count--;
	if (ctx->master->context_count == 0)
	{
		call_log = 1;
		if (ctx->master != ctx)
			free_master = 1;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	/* We call the log with ctx intact, apart from the master
	 * pointer having had the context_count reduced. The
	 * only possible problem here is if fz_log_activity
	 * clones the context, but it really shouldn't be doing
	 * that! */
	if (call_log)
		fz_log_activity(ctx, FZ_ACTIVITY_SHUTDOWN, NULL);
	if (free_master)
		ctx->alloc.free(ctx->alloc.user, ctx->master);

	/* Other finalisation calls go here (in reverse order) */
	fz_drop_document_handler_context(ctx);
	fz_drop_archive_handler_context(ctx);
	fz_drop_glyph_cache_context(ctx);
	fz_drop_store_context(ctx);
	fz_drop_style_context(ctx);
	fz_drop_tuning_context(ctx);
	fz_drop_colorspace_context(ctx);
	fz_drop_font_context(ctx);
	fz_drop_hyph_context(ctx);

	fz_flush_warnings(ctx);

	assert(ctx->error.top == ctx->error.stack_base);

	/* Free the context itself */
	if (ctx->master == ctx && ctx->context_count != 0)
	{
		/* Need to delay our freeing until all our children have died. */
		ctx->master = NULL;
	}
	else
	{
		ctx->alloc.free(ctx->alloc.user, ctx);
	}
}

static void
fz_init_error_context(fz_context *ctx)
{
#define ALIGN(addr, align)  ((((intptr_t)(addr)) + (align-1)) & ~(align-1))
	ctx->error.stack_base = (fz_error_stack_slot *)ALIGN(ctx->error.stack, FZ_JMPBUF_ALIGN);
	ctx->error.top = ctx->error.stack_base;
	ctx->error.errcode = FZ_ERROR_NONE;
	ctx->error.message[0] = 0;

	ctx->warn.message[0] = 0;
	ctx->warn.count = 0;
}

fz_context *
fz_new_context_imp(const fz_alloc_context *alloc, const fz_locks_context *locks, size_t max_store, const char *version)
{
	fz_context *ctx;

	if (strcmp(version, FZ_VERSION))
	{
		fprintf(stderr, "cannot create context: incompatible header (%s) and library (%s) versions\n", version, FZ_VERSION);
		return NULL;
	}

	if (!alloc)
		alloc = &fz_alloc_default;

	if (!locks)
		locks = &fz_locks_default;

	ctx = Memento_label(alloc->malloc(alloc->user, sizeof(fz_context)), "fz_context");
	if (!ctx)
	{
		fprintf(stderr, "cannot create context (phase 1)\n");
		return NULL;
	}
	memset(ctx, 0, sizeof *ctx);

	ctx->user = NULL;
	ctx->alloc = *alloc;
	ctx->locks = *locks;

	/* We are our own master! */
	ctx->master = ctx;
	ctx->context_count = 1;

	ctx->error.print = fz_default_error_callback;
	ctx->warn.print = fz_default_warning_callback;

	fz_init_error_context(ctx);
	fz_init_aa_context(ctx);
	fz_init_random_context(ctx);

	/* Now initialise sections that are shared */
	fz_try(ctx)
	{
		fz_new_store_context(ctx, max_store);
		fz_new_glyph_cache_context(ctx);
		fz_new_colorspace_context(ctx);
		fz_new_font_context(ctx);
		fz_new_hyph_context(ctx);
		fz_new_document_handler_context(ctx);
		fz_new_archive_handler_context(ctx);
		fz_new_style_context(ctx);
		fz_new_tuning_context(ctx);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot create context (phase 2)\n");
		fz_drop_context(ctx);
		return NULL;
	}
	return ctx;
}

fz_context *
fz_clone_context(fz_context *ctx)
{
	fz_context *new_ctx;

	/* We cannot safely clone the context without having locking/
	 * unlocking functions. */
	if (ctx == NULL || (ctx->locks.lock == fz_locks_default.lock && ctx->locks.unlock == fz_locks_default.unlock))
		return NULL;

	new_ctx = ctx->alloc.malloc(ctx->alloc.user, sizeof(fz_context));
	if (!new_ctx)
		return NULL;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	ctx->master->context_count++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	new_ctx->master = ctx->master;

	/* First copy old context, including pointers to shared contexts */
	memcpy(new_ctx, ctx, sizeof (fz_context));

	/* Reset error context to initial state. */
	fz_init_error_context(new_ctx);

	/* Then keep lock checking happy by keeping shared contexts with new context */
	fz_keep_document_handler_context(new_ctx);
	fz_keep_archive_handler_context(new_ctx);
	fz_keep_style_context(new_ctx);
	fz_keep_tuning_context(new_ctx);
	fz_keep_font_context(new_ctx);
	fz_keep_hyph_context(new_ctx);
	fz_keep_colorspace_context(new_ctx);
	fz_keep_store_context(new_ctx);
	fz_keep_glyph_cache(new_ctx);

	return new_ctx;
}

void fz_set_user_context(fz_context *ctx, void *user)
{
	if (ctx != NULL)
		ctx->user = user;
}

void *fz_user_context(fz_context *ctx)
{
	if (ctx == NULL)
		return NULL;

	return ctx->user;
}

void fz_register_activity_logger(fz_context *ctx, fz_activity_fn *activity, void *opaque)
{
	if (ctx == NULL)
		return;

	ctx->activity.activity = activity;
	ctx->activity.opaque = opaque;
}

void fz_log_activity(fz_context *ctx, fz_activity_reason reason, void *arg)
{
	if (ctx == NULL || ctx->activity.activity == NULL)
		return;

	ctx->activity.activity(ctx, ctx->activity.opaque, reason, arg);
}

int fz_new_document_id(fz_context *ctx)
{
	int id;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	while (ctx->master && ctx->master != ctx)
		ctx = ctx->master;
	id = ctx->next_document_id++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return id;
}
