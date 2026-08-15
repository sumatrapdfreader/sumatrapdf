# Memory

All memory in Fitz is allocated through an allocator, which allows you to use
a custom allocator instead of the system malloc if you need to.

## Malloc & Free

You should not need to do raw memory allocation using the Fitz context, but if
you do, here are the functions you need. These work just like the regular C
functions, but take a Fitz context and throw an exception if the allocation
fails. They will **not** return `NULL`; either they will succeed or they will
throw an exception.

	void *fz_malloc(fz_context *ctx, size_t size);
	void *fz_realloc(fz_context *ctx, void *old, size_t size);
	void *fz_calloc(fz_context *ctx, size_t count, size_t size);
	void fz_free(fz_context *ctx, void *ptr);

There are also some macros that allocate structures and arrays, together with a type cast to catch typing errors.

	T *fz_malloc_struct(fz_context *ctx, T); // Allocate and zero the memory.
	T *fz_malloc_array(fz_context *ctx, size_t count, T); // Allocate uninitialized memory!
	T *fz_realloc_array(fz_context *ctx, T *old, size_t count, T);

In the rare case that you need an allocation that returns `NULL` on failure,
there are variants for that too: `fz_malloc_no_throw`, etc.

## Pool Allocator

The pool allocator is used for allocating many small objects that live and die
together. All objects allocated from the pool will be freed when the pool is
freed.

	fz_pool *fz_new_pool(fz_context *ctx);
	void *fz_pool_alloc(fz_context *ctx, fz_pool *pool, size_t size);
	char *fz_pool_strdup(fz_context *ctx, fz_pool *pool, const char *s);
	void fz_drop_pool(fz_context *ctx, fz_pool *pool);

## Reference Counting

Most objects in MuPDF use reference counting to keep track of when they are no
longer used and can be freed. We use the verbs "keep" and "drop" to increment
and decrement the reference count. For simplicity, we also use the word "drop"
for non-reference counted objects (so that in case we change our minds and
decide to add reference counting to an object, the code that uses it need not
change).
