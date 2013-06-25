#include "mupdf/fitz.h"

struct fz_id_context_s
{
	int refs;
	int id;
};

static void
fz_drop_id_context(fz_context *ctx)
{
	int refs;
	fz_id_context *id = ctx->id;

	if (id == NULL)
		return;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	refs = --id->refs;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (refs == 0)
		fz_free(ctx, id);
}

static void
fz_new_id_context(fz_context *ctx)
{
	ctx->id = fz_malloc_struct(ctx, fz_id_context);
	ctx->id->refs = 1;
	ctx->id->id = 0;
}

static fz_id_context *
fz_keep_id_context(fz_context *ctx)
{
	fz_id_context *id = ctx->id;

	if (id == NULL)
		return NULL;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	++id->refs;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return id;
}

void
fz_free_context(fz_context *ctx)
{
	if (!ctx)
		return;

	/* Other finalisation calls go here (in reverse order) */
	fz_drop_glyph_cache_context(ctx);
	fz_drop_store_context(ctx);
	fz_free_aa_context(ctx);
	fz_drop_colorspace_context(ctx);
	fz_drop_font_context(ctx);
	fz_drop_id_context(ctx);

	if (ctx->warn)
	{
		fz_flush_warnings(ctx);
		fz_free(ctx, ctx->warn);
	}

	if (ctx->error)
	{
		assert(ctx->error->top == -1);
		fz_free(ctx, ctx->error);
	}

	/* Free the context itself */
	ctx->alloc->free(ctx->alloc->user, ctx);
}

/* Allocate new context structure, and initialise allocator, and sections
 * that aren't shared between contexts.
 */
static fz_context *
new_context_phase1(fz_alloc_context *alloc, fz_locks_context *locks)
{
	fz_context *ctx;

	ctx = alloc->malloc(alloc->user, sizeof(fz_context));
	if (!ctx)
		return NULL;
	memset(ctx, 0, sizeof *ctx);
	ctx->alloc = alloc;
	ctx->locks = locks;

	ctx->glyph_cache = NULL;

	ctx->error = fz_malloc_no_throw(ctx, sizeof(fz_error_context));
	if (!ctx->error)
		goto cleanup;
	ctx->error->top = -1;
	ctx->error->errcode = FZ_ERROR_NONE;
	ctx->error->message[0] = 0;

	ctx->warn = fz_malloc_no_throw(ctx, sizeof(fz_warn_context));
	if (!ctx->warn)
		goto cleanup;
	ctx->warn->message[0] = 0;
	ctx->warn->count = 0;

	/* New initialisation calls for context entries go here */
	fz_try(ctx)
	{
		fz_new_aa_context(ctx);
	}
	fz_catch(ctx)
	{
		goto cleanup;
	}

	return ctx;

cleanup:
	fprintf(stderr, "cannot create context (phase 1)\n");
	fz_free_context(ctx);
	return NULL;
}

fz_context *
fz_new_context(fz_alloc_context *alloc, fz_locks_context *locks, unsigned int max_store)
{
	fz_context *ctx;

	if (!alloc)
		alloc = &fz_alloc_default;

	if (!locks)
		locks = &fz_locks_default;

	ctx = new_context_phase1(alloc, locks);
	if (!ctx)
		return NULL;

	/* Now initialise sections that are shared */
	fz_try(ctx)
	{
		fz_new_store_context(ctx, max_store);
		fz_new_glyph_cache_context(ctx);
		fz_new_colorspace_context(ctx);
		fz_new_font_context(ctx);
		fz_new_id_context(ctx);
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot create context (phase 2)\n");
		fz_free_context(ctx);
		return NULL;
	}
	return ctx;
}

fz_context *
fz_clone_context(fz_context *ctx)
{
	/* We cannot safely clone the context without having locking/
	 * unlocking functions. */
	if (ctx == NULL || ctx->locks == &fz_locks_default)
		return NULL;
	return fz_clone_context_internal(ctx);
}

fz_context *
fz_clone_context_internal(fz_context *ctx)
{
	fz_context *new_ctx;

	if (ctx == NULL || ctx->alloc == NULL)
		return NULL;

	new_ctx = new_context_phase1(ctx->alloc, ctx->locks);
	if (!new_ctx)
		return NULL;

	/* Inherit AA defaults from old context. */
	fz_copy_aa_context(new_ctx, ctx);

	/* Keep thread lock checking happy by copying pointers first and locking under new context */
	new_ctx->store = ctx->store;
	new_ctx->store = fz_keep_store_context(new_ctx);
	new_ctx->glyph_cache = ctx->glyph_cache;
	new_ctx->glyph_cache = fz_keep_glyph_cache(new_ctx);
	new_ctx->colorspace = ctx->colorspace;
	new_ctx->colorspace = fz_keep_colorspace_context(new_ctx);
	new_ctx->font = ctx->font;
	new_ctx->font = fz_keep_font_context(new_ctx);
	new_ctx->id = ctx->id;
	new_ctx->id = fz_keep_id_context(new_ctx);

	return new_ctx;
}

int
fz_gen_id(fz_context *ctx)
{
	int id;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	/* We'll never wrap around in normal use, but if we *do*, then avoid
	 * 0. */
	do
	{
		id = ++ctx->id->id;
	}
	while (id == 0);
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return id;
}
