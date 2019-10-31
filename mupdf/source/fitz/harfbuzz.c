/*
 * Some additional glue functions for using Harfbuzz with
 * custom allocators.
 */

#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include "hb.h"

#include <assert.h>

/* Harfbuzz has some major design flaws (for our usage
 * at least).
 *
 * By default it uses malloc and free as the underlying
 * allocators. Thus in its default form we cannot get
 * a record (much less control) over how much allocation
 * is done.
 *
 * Harfbuzz does allow build options to control where
 * malloc and free go - in particular we point them at
 * fz_hb_malloc and fz_hb_free in our implementation.
 * Unfortunately, this has problems too.
 *
 * Firstly, there is no mechanism for getting a context
 * through the call. Most other libraries allow us to
 * pass a "void *" value in, and have it passed through
 * to arrive unchanged at the allocator functions.
 *
 * Without this rudimentary functionality, we are forced
 * to serialise all access to Harfbuzz.
 *
 * By taking a mutex around all calls to Harfbuzz, we
 * can use a static of our own to get a fz_context safely
 * through to the allocators. This obviously costs us
 * performance in the multi-threaded case.
 *
 * This does not protect us against the possibility of
 * other people calling harfbuzz; for instance, if we
 * link MuPDF into an app that either calls harfbuzz
 * itself, or uses another library that calls harfbuzz,
 * there is no guarantee that that library will take
 * the same lock while calling harfbuzz. This leaves
 * us open to the possibility of crashes. The only
 * way around this would be to use completely separate
 * harfbuzz instances.
 *
 * In order to ensure that allocations throughout mupdf
 * are done consistently, we get harfbuzz to call our
 * own fz_hb_malloc/realloc/calloc/free functions that
 * call down to fz_malloc/realloc/calloc/free. These
 * require context variables, so we get our fz_hb_lock
 * and unlock to set these. Any attempt to call through
 * without setting these will be detected.
 *
 * It is therefore vital that any fz_lock/fz_unlock
 * handlers are shared between all the fz_contexts in
 * use at a time.
 *
 * Secondly, Harfbuzz allocates some 'internal' memory
 * on the first call, and leaves this linked from static
 * variables. By default, this data is never freed back.
 * This means it is impossible to clear the library back
 * to a default state. Memory debugging will always show
 * Harfbuzz as having leaked a set amount of memory.
 *
 * There is a mechanism in Harfbuzz for freeing these
 * blocks - that of building with HAVE_ATEXIT. This
 * causes the blocks to be freed back on exit, but a)
 * this doesn't reset the fz_context value, so we can't
 * free them correctly, and b) any fz_context value it
 * did keep would already have been closed down due to
 * the program exit.
 *
 * In addition, because of these everlasting blocks, we
 * cannot safely call Harfbuzz after we close down any
 * allocator that Harfbuzz has been using (because
 * Harfbuzz may still be holding pointers to data within
 * that allocators managed space).
 *
 * There is nothing we can do about the leaking blocks
 * except to add some hacks to our memory debugging
 * library to allow it to suppress the blocks that
 * harfbuzz leaks.
 *
 * Consequently, we leave them to leak, and warn Memento
 * about this.
 */

/* Potentially we can write different versions
 * of get_context and set_context for different
 * threading systems.
 *
 * This simple version relies on harfbuzz never
 * trying to make 2 allocations at once on
 * different threads. The only way that can happen
 * is when one of those other threads is someone
 * outside MuPDF calling harfbuzz while MuPDF
 * is running. This will cause us such huge
 * problems that for now, we'll just forbid it.
 */

static fz_context *fz_hb_secret = NULL;

static void set_hb_context(fz_context *ctx)
{
	fz_hb_secret = ctx;
}

static fz_context *get_hb_context(void)
{
	return fz_hb_secret;
}

/*
	Lock against Harfbuzz being called
	simultaneously in several threads. This reuses
	FZ_LOCK_FREETYPE.
*/
void fz_hb_lock(fz_context *ctx)
{
	fz_lock(ctx, FZ_LOCK_FREETYPE);

	set_hb_context(ctx);
}

/*
	Unlock after a Harfbuzz call. This reuses
	FZ_LOCK_FREETYPE.
*/
void fz_hb_unlock(fz_context *ctx)
{
	set_hb_context(NULL);

	fz_unlock(ctx, FZ_LOCK_FREETYPE);
}

void *fz_hb_malloc(size_t size)
{
	fz_context *ctx = get_hb_context();

	assert(ctx != NULL);

	return Memento_label(fz_malloc_no_throw(ctx, size), "hb");
}

void *fz_hb_calloc(size_t n, size_t size)
{
	fz_context *ctx = get_hb_context();

	assert(ctx != NULL);

	return Memento_label(fz_calloc_no_throw(ctx, n, size), "hb");
}

void *fz_hb_realloc(void *ptr, size_t size)
{
	fz_context *ctx = get_hb_context();

	assert(ctx != NULL);

	return Memento_label(fz_realloc_no_throw(ctx, ptr, size), "hb");
}

void fz_hb_free(void *ptr)
{
	fz_context *ctx = get_hb_context();

	assert(ctx != NULL);

	fz_free(ctx, ptr);
}
