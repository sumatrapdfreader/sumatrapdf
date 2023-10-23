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

#ifndef MUPDF_FITZ_CONTEXT_H
#define MUPDF_FITZ_CONTEXT_H

#include "mupdf/fitz/version.h"
#include "mupdf/fitz/system.h"
#include "mupdf/fitz/geometry.h"


#ifndef FZ_VERBOSE_EXCEPTIONS
#define FZ_VERBOSE_EXCEPTIONS 0
#endif

typedef struct fz_font_context fz_font_context;
typedef struct fz_colorspace_context fz_colorspace_context;
typedef struct fz_style_context fz_style_context;
typedef struct fz_tuning_context fz_tuning_context;
typedef struct fz_store fz_store;
typedef struct fz_glyph_cache fz_glyph_cache;
typedef struct fz_document_handler_context fz_document_handler_context;
typedef struct fz_output fz_output;
typedef struct fz_context fz_context;

/**
	Allocator structure; holds callbacks and private data pointer.
*/
typedef struct
{
	void *user;
	void *(*malloc)(void *, size_t);
	void *(*realloc)(void *, void *, size_t);
	void (*free)(void *, void *);
} fz_alloc_context;

/**
	Exception macro definitions. Just treat these as a black box -
	pay no attention to the man behind the curtain.
*/
#define fz_var(var) fz_var_imp((void *)&(var))
#define fz_try(ctx) if (!fz_setjmp(*fz_push_try(ctx))) if (fz_do_try(ctx)) do
#define fz_always(ctx) while (0); if (fz_do_always(ctx)) do
#define fz_catch(ctx) while (0); if (fz_do_catch(ctx))

/**
	These macros provide a simple exception handling system. Use them as
	follows:

	fz_try(ctx)
		...
	fz_catch(ctx)
		...

	or as:

	fz_try(ctx)
		...
	fz_always(ctx)
		...
	fz_catch(ctx)
		...

	Code within the fz_try() section can then throw exceptions using fz_throw()
	(or fz_vthrow()).

	They are implemented with setjmp/longjmp, which can have unfortunate
	consequences for 'losing' local variable values on a throw. To avoid this
	we recommend calling 'fz_var(variable)' before the fz_try() for any
	local variable whose value may change within the fz_try() block and whose
	value will be required afterwards.

	Do not call anything in the fz_always() section that can throw.

	Any exception can be rethrown from the fz_catch() section using fz_rethrow()
	as long as there has been no intervening use of fz_try/fz_catch.
*/

/**
	Throw an exception.

	This assumes an enclosing fz_try() block within the callstack.
*/
FZ_NORETURN void fz_vthrow(fz_context *ctx, int errcode, const char *, va_list ap);
FZ_NORETURN void fz_throw(fz_context *ctx, int errcode, const char *, ...) FZ_PRINTFLIKE(3,4);
FZ_NORETURN void fz_rethrow(fz_context *ctx);

/**
	Called within a catch block this modifies the current
	exception's code. If it's of type 'fromcode' it is
	modified to 'tocode'. Typically used for 'downgrading'
	exception severity.
*/
void fz_morph_error(fz_context *ctx, int fromcode, int tocode);

/**
	Log a warning.

	This goes to the registered warning stream (stderr by
	default).
*/
void fz_vwarn(fz_context *ctx, const char *fmt, va_list ap);
void fz_warn(fz_context *ctx, const char *fmt, ...) FZ_PRINTFLIKE(2,3);

/**
	Within an fz_catch() block, retrieve the formatted message
	string for the current exception.

	This assumes no intervening use of fz_try/fz_catch.
*/
const char *fz_caught_message(fz_context *ctx);

/**
	Within an fz_catch() block, retrieve the error code for
	the current exception.

	This assumes no intervening use of fz_try/fz_catch.
*/
int fz_caught(fz_context *ctx);

/**
	Within an fz_catch() block, rethrow the current exception
	if the errcode of the current exception matches.

	This assumes no intervening use of fz_try/fz_catch.
*/
void fz_rethrow_if(fz_context *ctx, int errcode);

/**
	Format an error message, and log it to the registered
	error stream (stderr by default).
*/
void fz_log_error_printf(fz_context *ctx, const char *fmt, ...) FZ_PRINTFLIKE(2,3);
void fz_vlog_error_printf(fz_context *ctx, const char *fmt, va_list ap);

/**
	Log a (preformatted) string to the registered
	error stream (stderr by default).
*/
void fz_log_error(fz_context *ctx, const char *str);

void fz_start_throw_on_repair(fz_context *ctx);
void fz_end_throw_on_repair(fz_context *ctx);

/**
	Now, a debugging feature. If FZ_VERBOSE_EXCEPTIONS is 1 then
	some of the above functions are replaced by versions that print
	FILE and LINE information.
*/
#if FZ_VERBOSE_EXCEPTIONS
#define fz_vthrow(CTX, ERRCODE, FMT, VA) fz_vthrowFL(CTX, __FILE__, __LINE__, ERRCODE, FMT, VA)
#define fz_throw(CTX, ERRCODE, ...) fz_throwFL(CTX, __FILE__, __LINE__, ERRCODE, __VA_ARGS__)
#define fz_rethrow(CTX) fz_rethrowFL(CTX, __FILE__, __LINE__)
#define fz_morph_error(CTX, FROM, TO) fz_morph_errorFL(CTX, __FILE__, __LINE__, FROM, TO)
#define fz_vwarn(CTX, FMT, VA) fz_vwarnFL(CTX, __FILE__, __LINE__, FMT, VA)
#define fz_warn(CTX, ...) fz_warnFL(CTX, __FILE__, __LINE__, __VA_ARGS__)
#define fz_rethrow_if(CTX, ERRCODE) fz_rethrow_ifFL(CTX, __FILE__, __LINE__, ERRCODE)
#define fz_log_error_printf(CTX, ...) fz_log_error_printfFL(CTX, __FILE__, __LINE__, __VA_ARGS__)
#define fz_vlog_error_printf(CTX, FMT, VA) fz_log_error_printfFL(CTX, __FILE__, __LINE__, FMT, VA)
#define fz_log_error(CTX, STR) fz_log_error_printfFL(CTX, __FILE__, __LINE__, STR)
FZ_NORETURN void fz_vthrowFL(fz_context *ctx, const char *file, int line, int errcode, const char *fmt, va_list ap);
FZ_NORETURN void fz_throwFL(fz_context *ctx, const char *file, int line, int errcode, const char *fmt, ...) FZ_PRINTFLIKE(5,6);
FZ_NORETURN void fz_rethrowFL(fz_context *ctx, const char *file, int line);
void fz_morph_errorFL(fz_context *ctx, const char *file, int line, int fromcode, int tocode);
void fz_vwarnFL(fz_context *ctx, const char *file, int line, const char *fmt, va_list ap);
void fz_warnFL(fz_context *ctx, const char *file, int line, const char *fmt, ...) FZ_PRINTFLIKE(4,5);
void fz_rethrow_ifFL(fz_context *ctx, const char *file, int line, int errcode);
void fz_log_error_printfFL(fz_context *ctx, const char *file, int line, const char *fmt, ...) FZ_PRINTFLIKE(4,5);
void fz_vlog_error_printfFL(fz_context *ctx, const char *file, int line, const char *fmt, va_list ap);
void fz_log_errorFL(fz_context *ctx, const char *file, int line, const char *str);
#endif

enum
{
	FZ_ERROR_NONE = 0,
	FZ_ERROR_MEMORY = 1,
	FZ_ERROR_GENERIC = 2,
	FZ_ERROR_SYNTAX = 3,
	FZ_ERROR_MINOR = 4,
	FZ_ERROR_TRYLATER = 5,
	FZ_ERROR_ABORT = 6,
	FZ_ERROR_REPAIRED = 7,
	FZ_ERROR_COUNT
};

/**
	Flush any repeated warnings.

	Repeated warnings are buffered, counted and eventually printed
	along with the number of repetitions. Call fz_flush_warnings
	to force printing of the latest buffered warning and the
	number of repetitions, for example to make sure that all
	warnings are printed before exiting an application.
*/
void fz_flush_warnings(fz_context *ctx);

/**
	Locking functions

	MuPDF is kept deliberately free of any knowledge of particular
	threading systems. As such, in order for safe multi-threaded
	operation, we rely on callbacks to client provided functions.

	A client is expected to provide FZ_LOCK_MAX number of mutexes,
	and a function to lock/unlock each of them. These may be
	recursive mutexes, but do not have to be.

	If a client does not intend to use multiple threads, then it
	may pass NULL instead of a lock structure.

	In order to avoid deadlocks, we have one simple rule
	internally as to how we use locks: We can never take lock n
	when we already hold any lock i, where 0 <= i <= n. In order
	to verify this, we have some debugging code, that can be
	enabled by defining FITZ_DEBUG_LOCKING.
*/

typedef struct
{
	void *user;
	void (*lock)(void *user, int lock);
	void (*unlock)(void *user, int lock);
} fz_locks_context;

enum {
	FZ_LOCK_ALLOC = 0,
	FZ_LOCK_FREETYPE,
	FZ_LOCK_GLYPHCACHE,
	FZ_LOCK_MAX
};

#if defined(MEMENTO) || !defined(NDEBUG)
#define FITZ_DEBUG_LOCKING
#endif

#ifdef FITZ_DEBUG_LOCKING

void fz_assert_lock_held(fz_context *ctx, int lock);
void fz_assert_lock_not_held(fz_context *ctx, int lock);
void fz_lock_debug_lock(fz_context *ctx, int lock);
void fz_lock_debug_unlock(fz_context *ctx, int lock);

#else

#define fz_assert_lock_held(A,B) do { } while (0)
#define fz_assert_lock_not_held(A,B) do { } while (0)
#define fz_lock_debug_lock(A,B) do { } while (0)
#define fz_lock_debug_unlock(A,B) do { } while (0)

#endif /* !FITZ_DEBUG_LOCKING */

/**
	Specifies the maximum size in bytes of the resource store in
	fz_context. Given as argument to fz_new_context.

	FZ_STORE_UNLIMITED: Let resource store grow unbounded.

	FZ_STORE_DEFAULT: A reasonable upper bound on the size, for
	devices that are not memory constrained.
*/
enum {
	FZ_STORE_UNLIMITED = 0,
	FZ_STORE_DEFAULT = 256 << 20,
};

/**
	Allocate context containing global state.

	The global state contains an exception stack, resource store,
	etc. Most functions in MuPDF take a context argument to be
	able to reference the global state. See fz_drop_context for
	freeing an allocated context.

	alloc: Supply a custom memory allocator through a set of
	function pointers. Set to NULL for the standard library
	allocator. The context will keep the allocator pointer, so the
	data it points to must not be modified or freed during the
	lifetime of the context.

	locks: Supply a set of locks and functions to lock/unlock
	them, intended for multi-threaded applications. Set to NULL
	when using MuPDF in a single-threaded applications. The
	context will keep the locks pointer, so the data it points to
	must not be modified or freed during the lifetime of the
	context.

	max_store: Maximum size in bytes of the resource store, before
	it will start evicting cached resources such as fonts and
	images. FZ_STORE_UNLIMITED can be used if a hard limit is not
	desired. Use FZ_STORE_DEFAULT to get a reasonable size.

	May return NULL.
*/
#define fz_new_context(alloc, locks, max_store) fz_new_context_imp(alloc, locks, max_store, FZ_VERSION)

/**
	Make a clone of an existing context.

	This function is meant to be used in multi-threaded
	applications where each thread requires its own context, yet
	parts of the global state, for example caching, are shared.

	ctx: Context obtained from fz_new_context to make a copy of.
	ctx must have had locks and lock/functions setup when created.
	The two contexts will share the memory allocator, resource
	store, locks and lock/unlock functions. They will each have
	their own exception stacks though.

	May return NULL.
*/
fz_context *fz_clone_context(fz_context *ctx);

/**
	Free a context and its global state.

	The context and all of its global state is freed, and any
	buffered warnings are flushed (see fz_flush_warnings). If NULL
	is passed in nothing will happen.

	Must not be called for a context that is being used in an active
	fz_try(), fz_always() or fz_catch() block.
*/
void fz_drop_context(fz_context *ctx);

/**
	Set the user field in the context.

	NULL initially, this field can be set to any opaque value
	required by the user. It is copied on clones.
*/
void fz_set_user_context(fz_context *ctx, void *user);

/**
	Read the user field from the context.
*/
void *fz_user_context(fz_context *ctx);

/**
	FIXME: Better not to expose fz_default_error_callback, and
	fz_default_warning callback and to allow 'NULL' to be used
	int fz_set_xxxx_callback to mean "defaults".

	FIXME: Do we need/want functions like
	fz_error_callback(ctx, message) to allow callers to inject
	stuff into the error/warning streams?
*/
/**
	The default error callback. Declared publicly just so that the
	error callback can be set back to this after it has been
	overridden.
*/
void fz_default_error_callback(void *user, const char *message);

/**
	The default warning callback. Declared publicly just so that
	the warning callback can be set back to this after it has been
	overridden.
*/
void fz_default_warning_callback(void *user, const char *message);

/**
	A callback called whenever an error message is generated.
	The user pointer passed to fz_set_error_callback() is passed
	along with the error message.
*/
typedef void (fz_error_cb)(void *user, const char *message);

/**
	A callback called whenever a warning message is generated.
	The user pointer passed to fz_set_warning_callback() is
	passed along with the warning message.
*/
typedef void (fz_warning_cb)(void *user, const char *message);

/**
	Set the error callback. This will be called as part of the
	exception handling.

	The callback must not throw exceptions!
*/
void fz_set_error_callback(fz_context *ctx, fz_error_cb *error_cb, void *user);

/**
	Retrieve the currently set error callback, or NULL if none
	has been set. Optionally, if user is non-NULL, the user pointer
	given when the warning callback was set is also passed back to
	the caller.
*/
fz_error_cb *fz_error_callback(fz_context *ctx, void **user);

/**
	Set the warning callback. This will be called as part of the
	exception handling.

	The callback must not throw exceptions!
*/
void fz_set_warning_callback(fz_context *ctx, fz_warning_cb *warning_cb, void *user);

/**
	Retrieve the currently set warning callback, or NULL if none
	has been set. Optionally, if user is non-NULL, the user pointer
	given when the warning callback was set is also passed back to
	the caller.
*/
fz_warning_cb *fz_warning_callback(fz_context *ctx, void **user);

/**
	In order to tune MuPDF's behaviour, certain functions can
	(optionally) be provided by callers.
*/

/**
	Given the width and height of an image,
	the subsample factor, and the subarea of the image actually
	required, the caller can decide whether to decode the whole
	image or just a subarea.

	arg: The caller supplied opaque argument.

	w, h: The width/height of the complete image.

	l2factor: The log2 factor for subsampling (i.e. image will be
	decoded to (w>>l2factor, h>>l2factor)).

	subarea: The actual subarea required for the current operation.
	The tuning function is allowed to increase this in size if
	required.
*/
typedef void (fz_tune_image_decode_fn)(void *arg, int w, int h, int l2factor, fz_irect *subarea);

/**
	Given the source width and height of
	image, together with the actual required width and height,
	decide whether we should use mitchell scaling.

	arg: The caller supplied opaque argument.

	dst_w, dst_h: The actual width/height required on the target
	device.

	src_w, src_h: The source width/height of the image.

	Return 0 not to use the Mitchell scaler, 1 to use the Mitchell
	scaler. All other values reserved.
*/
typedef int (fz_tune_image_scale_fn)(void *arg, int dst_w, int dst_h, int src_w, int src_h);

/**
	Set the tuning function to use for
	image decode.

	image_decode: Function to use.

	arg: Opaque argument to be passed to tuning function.
*/
void fz_tune_image_decode(fz_context *ctx, fz_tune_image_decode_fn *image_decode, void *arg);

/**
	Set the tuning function to use for
	image scaling.

	image_scale: Function to use.

	arg: Opaque argument to be passed to tuning function.
*/
void fz_tune_image_scale(fz_context *ctx, fz_tune_image_scale_fn *image_scale, void *arg);

/**
	Get the number of bits of antialiasing we are
	using (for graphics). Between 0 and 8.
*/
int fz_aa_level(fz_context *ctx);

/**
	Set the number of bits of antialiasing we should
	use (for both text and graphics).

	bits: The number of bits of antialiasing to use (values are
	clamped to within the 0 to 8 range).
*/
void fz_set_aa_level(fz_context *ctx, int bits);

/**
	Get the number of bits of antialiasing we are
	using for text. Between 0 and 8.
*/
int fz_text_aa_level(fz_context *ctx);

/**
	Set the number of bits of antialiasing we
	should use for text.

	bits: The number of bits of antialiasing to use (values are
	clamped to within the 0 to 8 range).
*/
void fz_set_text_aa_level(fz_context *ctx, int bits);

/**
	Get the number of bits of antialiasing we are
	using for graphics. Between 0 and 8.
*/
int fz_graphics_aa_level(fz_context *ctx);

/**
	Set the number of bits of antialiasing we
	should use for graphics.

	bits: The number of bits of antialiasing to use (values are
	clamped to within the 0 to 8 range).
*/
void fz_set_graphics_aa_level(fz_context *ctx, int bits);

/**
	Get the minimum line width to be
	used for stroked lines.

	min_line_width: The minimum line width to use (in pixels).
*/
float fz_graphics_min_line_width(fz_context *ctx);

/**
	Set the minimum line width to be
	used for stroked lines.

	min_line_width: The minimum line width to use (in pixels).
*/
void fz_set_graphics_min_line_width(fz_context *ctx, float min_line_width);

/**
	Get the user stylesheet source text.
*/
const char *fz_user_css(fz_context *ctx);

/**
	Set the user stylesheet source text for use with HTML and EPUB.
*/
void fz_set_user_css(fz_context *ctx, const char *text);

/**
	Return whether to respect document styles in HTML and EPUB.
*/
int fz_use_document_css(fz_context *ctx);

/**
	Toggle whether to respect document styles in HTML and EPUB.
*/
void fz_set_use_document_css(fz_context *ctx, int use);

/**
	Enable icc profile based operation.
*/
void fz_enable_icc(fz_context *ctx);

/**
	Disable icc profile based operation.
*/
void fz_disable_icc(fz_context *ctx);

/**
	Memory Allocation and Scavenging:

	All calls to MuPDF's allocator functions pass through to the
	underlying allocators passed in when the initial context is
	created, after locks are taken (using the supplied locking
	function) to ensure that only one thread at a time calls
	through.

	If the underlying allocator fails, MuPDF attempts to make room
	for the allocation by evicting elements from the store, then
	retrying.

	Any call to allocate may then result in several calls to the
	underlying allocator, and result in elements that are only
	referred to by the store being freed.
*/

/**
	Allocate memory for a structure, clear it, and tag the pointer
	for Memento.

	Throws exception in the event of failure to allocate.
*/
#define fz_malloc_struct(CTX, TYPE) \
	((TYPE*)Memento_label(fz_calloc(CTX, 1, sizeof(TYPE)), #TYPE))

/**
	Allocate memory for an array of structures, clear it, and tag
	the pointer for Memento.

	Throws exception in the event of failure to allocate.
*/
#define fz_malloc_struct_array(CTX, N, TYPE) \
	((TYPE*)Memento_label(fz_calloc(CTX, N, sizeof(TYPE)), #TYPE "[]"))

/**
	Allocate uninitialized memory for an array of structures, and
	tag the pointer for Memento. Does NOT clear the memory!

	Throws exception in the event of failure to allocate.
*/
#define fz_malloc_array(CTX, COUNT, TYPE) \
	((TYPE*)Memento_label(fz_malloc(CTX, (COUNT) * sizeof(TYPE)), #TYPE "[]"))
#define fz_realloc_array(CTX, OLD, COUNT, TYPE) \
	((TYPE*)Memento_label(fz_realloc(CTX, OLD, (COUNT) * sizeof(TYPE)), #TYPE "[]"))

/**
	Allocate uninitialized memory of a given size.
	Does NOT clear the memory!

	May return NULL for size = 0.

	Throws exception in the event of failure to allocate.
*/
void *fz_malloc(fz_context *ctx, size_t size);

/**
	Allocate array of memory of count entries of size bytes.
	Clears the memory to zero.

	Throws exception in the event of failure to allocate.
*/
void *fz_calloc(fz_context *ctx, size_t count, size_t size);

/**
	Reallocates a block of memory to given size. Existing contents
	up to min(old_size,new_size) are maintained. The rest of the
	block is uninitialised.

	fz_realloc(ctx, NULL, size) behaves like fz_malloc(ctx, size).

	fz_realloc(ctx, p, 0); behaves like fz_free(ctx, p).

	Throws exception in the event of failure to allocate.
*/
void *fz_realloc(fz_context *ctx, void *p, size_t size);

/**
	Free a previously allocated block of memory.

	fz_free(ctx, NULL) does nothing.

	Never throws exceptions.
*/
void fz_free(fz_context *ctx, void *p);

/**
	fz_malloc equivalent that returns NULL rather than throwing
	exceptions.
*/
void *fz_malloc_no_throw(fz_context *ctx, size_t size);

/**
	fz_calloc equivalent that returns NULL rather than throwing
	exceptions.
*/
void *fz_calloc_no_throw(fz_context *ctx, size_t count, size_t size);

/**
	fz_realloc equivalent that returns NULL rather than throwing
	exceptions.
*/
void *fz_realloc_no_throw(fz_context *ctx, void *p, size_t size);

/**
	Portable strdup implementation, using fz allocators.
*/
char *fz_strdup(fz_context *ctx, const char *s);

/**
	Fill block with len bytes of pseudo-randomness.
*/
void fz_memrnd(fz_context *ctx, uint8_t *block, int len);


/* Implementation details: subject to change. */

/* Implementations exposed for speed, but considered private. */

void fz_var_imp(void *);
fz_jmp_buf *fz_push_try(fz_context *ctx);
int fz_do_try(fz_context *ctx);
int fz_do_always(fz_context *ctx);
int fz_do_catch(fz_context *ctx);

#ifndef FZ_JMPBUF_ALIGN
#define FZ_JMPBUF_ALIGN 32
#endif

typedef struct
{
	fz_jmp_buf buffer;
	int state, code;
	char padding[FZ_JMPBUF_ALIGN-sizeof(int)*2];
} fz_error_stack_slot;

typedef struct
{
	fz_error_stack_slot *top;
	fz_error_stack_slot stack[256];
	fz_error_stack_slot padding;
	fz_error_stack_slot *stack_base;
	int errcode;
	void *print_user;
	void (*print)(void *user, const char *message);
	char message[256];
} fz_error_context;

typedef struct
{
	void *print_user;
	void (*print)(void *user, const char *message);
	int count;
	char message[256];
} fz_warn_context;

typedef struct
{
	int hscale;
	int vscale;
	int scale;
	int bits;
	int text_bits;
	float min_line_width;
} fz_aa_context;

struct fz_context
{
	void *user;
	fz_alloc_context alloc;
	fz_locks_context locks;
	fz_error_context error;
	fz_warn_context warn;

	/* unshared contexts */
	fz_aa_context aa;
	uint16_t seed48[7];
#if FZ_ENABLE_ICC
	int icc_enabled;
#endif
	int throw_on_repair;

	/* TODO: should these be unshared? */
	fz_document_handler_context *handler;
	fz_style_context *style;
	fz_tuning_context *tuning;

	/* shared contexts */
	fz_output *stddbg;
	fz_font_context *font;
	fz_colorspace_context *colorspace;
	fz_store *store;
	fz_glyph_cache *glyph_cache;
};

fz_context *fz_new_context_imp(const fz_alloc_context *alloc, const fz_locks_context *locks, size_t max_store, const char *version);

/**
	Lock one of the user supplied mutexes.
*/
static inline void
fz_lock(fz_context *ctx, int lock)
{
	fz_lock_debug_lock(ctx, lock);
	ctx->locks.lock(ctx->locks.user, lock);
}

/**
	Unlock one of the user supplied mutexes.
*/
static inline void
fz_unlock(fz_context *ctx, int lock)
{
	fz_lock_debug_unlock(ctx, lock);
	ctx->locks.unlock(ctx->locks.user, lock);
}

/* Lock-safe reference counting functions */

static inline void *
fz_keep_imp(fz_context *ctx, void *p, int *refs)
{
	if (p)
	{
		(void)Memento_checkIntPointerOrNull(refs);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (*refs > 0)
		{
			(void)Memento_takeRef(p);
			++*refs;
		}
		fz_unlock(ctx, FZ_LOCK_ALLOC);
	}
	return p;
}

static inline void *
fz_keep_imp_locked(fz_context *ctx FZ_UNUSED, void *p, int *refs)
{
	if (p)
	{
		(void)Memento_checkIntPointerOrNull(refs);
		if (*refs > 0)
		{
			(void)Memento_takeRef(p);
			++*refs;
		}
	}
	return p;
}

static inline void *
fz_keep_imp8_locked(fz_context *ctx FZ_UNUSED, void *p, int8_t *refs)
{
	if (p)
	{
		(void)Memento_checkIntPointerOrNull(refs);
		if (*refs > 0)
		{
			(void)Memento_takeRef(p);
			++*refs;
		}
	}
	return p;
}

static inline void *
fz_keep_imp8(fz_context *ctx, void *p, int8_t *refs)
{
	if (p)
	{
		(void)Memento_checkBytePointerOrNull(refs);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (*refs > 0)
		{
			(void)Memento_takeRef(p);
			++*refs;
		}
		fz_unlock(ctx, FZ_LOCK_ALLOC);
	}
	return p;
}

static inline void *
fz_keep_imp16(fz_context *ctx, void *p, int16_t *refs)
{
	if (p)
	{
		(void)Memento_checkShortPointerOrNull(refs);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (*refs > 0)
		{
			(void)Memento_takeRef(p);
			++*refs;
		}
		fz_unlock(ctx, FZ_LOCK_ALLOC);
	}
	return p;
}

static inline int
fz_drop_imp(fz_context *ctx, void *p, int *refs)
{
	if (p)
	{
		int drop;
		(void)Memento_checkIntPointerOrNull(refs);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (*refs > 0)
		{
			(void)Memento_dropIntRef(p);
			drop = --*refs == 0;
		}
		else
			drop = 0;
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		return drop;
	}
	return 0;
}

static inline int
fz_drop_imp8(fz_context *ctx, void *p, int8_t *refs)
{
	if (p)
	{
		int drop;
		(void)Memento_checkBytePointerOrNull(refs);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (*refs > 0)
		{
			(void)Memento_dropByteRef(p);
			drop = --*refs == 0;
		}
		else
			drop = 0;
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		return drop;
	}
	return 0;
}

static inline int
fz_drop_imp16(fz_context *ctx, void *p, int16_t *refs)
{
	if (p)
	{
		int drop;
		(void)Memento_checkShortPointerOrNull(refs);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (*refs > 0)
		{
			(void)Memento_dropShortRef(p);
			drop = --*refs == 0;
		}
		else
			drop = 0;
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		return drop;
	}
	return 0;
}

#endif
