#ifndef ARTIFEX_EXTRACT_BUFFER_H
#define ARTIFEX_EXTRACT_BUFFER_H

/*
	Reading and writing abstractions.

	We use inline code in the common case where reading or writing
	can be satisfied using a cache.
*/

#include "extract/alloc.h"
#include "extract/extract.h"

#include <stddef.h>
#include <string.h>

/* Work around MSVC issues with our use of 'inline'. */
#if !defined __cplusplus && defined(_MSC_VER)
	#if (_MSC_VER < 1500)
		/* inline and inline__ not available so remove all mention of
		 * inline. This may result in warnings about unused static
		 * functions. */
		#define inline
	#else
		/* __inline is always available. */
		#define inline __inline
	#endif
#endif


/*
	Read specified number of bytes from buffer into data..+bytes,
	making multiple calls to the underlying extract_buffer_fn_read
	function until we have read <numbytes> or reached EOF. Return
	+1 if short read due to EOF.

	buffer:
		As returned by earlier call to extract_buffer_open().
	data:
		Location of transferred data.
	bytes:
		Number of bytes transferred.
	o_actual:
		Optional out-param, set to actual number of bytes read.
		If we return zero this will always be <numbytes>; otherwise
		will be less than <numbytes>.

	For speed reasons, this is implemented below and uses only
	inline code if the requested data can be read from the cache.
*/
static inline int
extract_buffer_read(extract_buffer_t *buffer,
			void	     *data,
			size_t	      numbytes,
			size_t	     *o_actual);


/*
	Write specified data into buffer. Returns +1 if short write due to EOF.

	buffer:
		As returned by earlier call to extract_buffer_open().
	data:
		Location of source data.
	bytes:
		Number of bytes to copy.
	out_actual:
		Optional out-param, set to actual number of bytes
		written. If we return zero this will always be
		<numbytes>; otherwise will be less than <numbytes>
		and can even be negative if internal cache-flush
		using fn_write() fails or returns EOF.

	For speed reasons, this is implemented below and uses only
	inline code if there is space in the cache for the data.
*/
static inline int
extract_buffer_write(extract_buffer_t *buffer,
			const void    *data,
			size_t	       numbytes,
			size_t	      *o_actual);

static inline int
extract_buffer_cat(extract_buffer_t *buffer,
			const char  *string);


/* Return number of bytes read or written so far. */
size_t extract_buffer_pos(extract_buffer_t *buffer);


/*
	Close down an extract_buffer_t and free all internal resources.

	Can return error or +1 for EOF if write buffer and fn_write()
	fails when flushing cache.

	Always sets *io_buffer to NULL. Does nothing if *io_buffer is
	already NULL.
*/
int extract_buffer_close(extract_buffer_t **io_buffer);


/*
	Callback used by read buffer. Should read data from buffer
	into the supplied destination. Short reads are not an error.

	E.g. used to fill cache or to handle large reads.

	Should return 0 on success (including short read or EOF) or
	-1 with errno set.

	handle:
		As passed to extract_buffer_open().
	destination:
		Start of destination.
	bytes:
		Number of bytes in destination.
	o_actual:
		Out-param, set to zero if EOF. Otherwise set to the
		number of bytes transferred in the range 1..<numbytes>
		inclusive.
*/
typedef int (extract_buffer_fn_read)(void *handle, void *destination, size_t numbytes, size_t *o_actual);

/*
	Callback used by write buffer. Should write data from the
	supplied source into the buffer; short writes are not an
	error.

	E.g. used to flush cache or to handle large writes.

	Should return 0 on success (including short write or EOF)
	or -1 with errno set.

	handle:
		As passed to extract_buffer_open().
	source:
		Start of source.
	bytes:
		Number of bytes in source.
	o_actual:
		Out-param, set to zero if EOF. Otherwise set to the
		number of bytes transferred in the range 1..<numbytes>
		inclusive.
*/
typedef int (extract_buffer_fn_write)(void *handle, const void *source, size_t numbytes, size_t *o_actual);

/*
	Callback to flush/populate cache.

	If the buffer is for writing:
		Should return a memory region to which data can be
		written. Any data written to a previous cache will
		have already been passed to fn_write() so this can
		overlap or be the same as any previously-returned cache.

	If the buffer is for reading:
		Should return a memory region containing more data
		to be read. All data in any previously-returned cache
		has been read so this can overlap or be the same as
		any previous cache.

	handle:
		As passed to extract_buffer_open().
	o_data:
		Out-param, set to point to new cache.
	o_numbytes:
		Out-param, set to size of new cache.

	If no data is available due to EOF, should return with
	*o_numbytes set to zero.
*/
typedef int (extract_buffer_fn_cache)(void *handle, void **o_cache, size_t *o_numbytes);

/*
	Called by extract_buffer_close().

	handle:
		As passed to extract_buffer_open().
*/
typedef void (extract_buffer_fn_close)(void *handle);


/*
	Returns the extract_alloc_t* originally passed to
	extract_buffer_open*().
*/
extract_alloc_t *extract_buffer_alloc(extract_buffer_t *buffer);

/*
	Creates an extract_buffer_t that uses specified callbacks.

	If fn_read is non-NULL the buffer is a read buffer, else
	if fn_write is non-NULL the buffer is a write buffer.
	Passing non-NULL for both or neither is not supported.

	alloc:
		NULL or from extract_alloc_create(). Is only used
		to allocate the extract_buffer_t returned in
		*o_buffer.
	handle:
		Passed to fn_read, fn_write, fn_cache and fn_close
		callbacks.
	fn_read:
		Callback for reading data.
	fn_write:
		Callback for writing data.
	fn_cache:
		Optional cache callback.
	fn_close:
		Optional close callback.
	o_buffer:
		Out-param. Set to NULL on error.
*/
int extract_buffer_open(extract_alloc_t		 *alloc,
			void			 *handle,
			extract_buffer_fn_read	 *fn_read,
			extract_buffer_fn_write	 *fn_write,
			extract_buffer_fn_cache	 *fn_cache,
			extract_buffer_fn_close	 *fn_close,
			extract_buffer_t	**o_buffer);


/*
	Create an extract_buffer_t that reads from or writes to
	a single fixed block of memory.

	The address region data..+data_length must exist for the
	lifetime of the returned extract_buffer_t.

	alloc:
		NULL or from extract_alloc_create(). Is only used
		to allocate the extract_buffer_t returned in *o_buffer.
	data:
		Start of memory region. Note that if the
		extract_buffer_t is used as a write buffer then
		data[] will be written-to, despite the 'const'. [This
		use of const avoids the need for the caller to use
		a cast when creating a read-buffer.]
	bytes:
		Size of memory region.
	handle:
		Passed to fn_close.
	fn_close:
		Optional callback called by extract_buffer_close().
		E.g. could copy the memory region elsewhere if the
		buffer was used as a write buffer.
	o_buffer:
		Out-param.
*/
int extract_buffer_open_simple(extract_alloc_t           *alloc,
				const void               *data,
				size_t                    numbytes,
				void                     *handle,
				extract_buffer_fn_close  *fn_close,
				extract_buffer_t        **o_buffer);


/*
	Creates a buffer that reads from, or writes to, a file.
	For portability uses an internal FILE* rather than an
	integer file descriptor, so doesn't use	extract_buffer's
	caching support because FILE* already provides caching.

	path:
		Path of file to read from.
	writable:
		We create read buffer if zero, else a write buffer.
	o_buffer:
		Out-param. Set to NULL on error.
*/
int extract_buffer_open_file(extract_alloc_t      *alloc,
				const char        *path,
				int                writable,
				extract_buffer_t **o_buffer);


/*
	A write buffer that writes to an automatically-growing
	contiguous area of memory.
*/
typedef struct
{
	extract_buffer_t *buffer;
	char             *data;
	size_t            alloc_size;
	size_t            data_size;
} extract_buffer_expanding_t;

/*
	Create a writable buffer that writes into an automatically-growing
	contiguous area of memory.

	alloc:
		NULL or from extract_alloc_create().
	buffer_expanding:
		Out-param; *buffer_expanding is initialised.

	Initialises buffer_expanding. buffer_expanding->buffer
	can be passed to extract_buffer_*() functions. After
	buffer_close(), the written data is available in
	buffer_expanding->data..+data_size, which will have been
	allocated using <alloc>.
*/
int extract_buffer_expanding_create(extract_alloc_t        *alloc,
				extract_buffer_expanding_t *buffer_expanding);


/* Everything after here is private implementation and subject to change
 * without notice! */

/*
	We expose some implementation details to allow
	extract_buffer_read() and extract_buffer_write() to be
	inline; specifically we allow the compiler to optimise
	the common case where reading/writing uses the cache only.
*/

typedef struct
{
	void   *cache;
	size_t  numbytes;
	size_t  pos;
} extract_buffer_cache_t;


/* Internal use only. */
int extract_buffer_read_internal(
	extract_buffer_t *buffer,
	void             *data,
	size_t            numbytes,
	size_t           *o_actual
	);

static inline int extract_buffer_read(
	extract_buffer_t *buffer,
	void             *data,
	size_t            numbytes,
	size_t           *o_actual
	)
{
	extract_buffer_cache_t *cache = (extract_buffer_cache_t *)(void *)buffer;
	if (cache->numbytes - cache->pos < numbytes) {
		/* Can't use just the cache. */
		return extract_buffer_read_internal(buffer, data, numbytes, o_actual);
	}
	/* We can use just the cache. */
	memcpy(data, (char *)cache->cache + cache->pos, numbytes);
	cache->pos += numbytes;
	if (o_actual) *o_actual = numbytes;
	return 0;
}


/* Internal use only. */
int extract_buffer_write_internal(
	extract_buffer_t *buffer,
	const void       *data,
	size_t            numbytes,
	size_t           *o_actual
	);

static inline int extract_buffer_write(
	extract_buffer_t *buffer,
	const void       *data,
	size_t            numbytes,
	size_t           *o_actual
	)
{
	extract_buffer_cache_t *cache = (extract_buffer_cache_t *)(void *)buffer;
	if (cache->numbytes - cache->pos < numbytes) {
		/* Can't use just the cache. */
		return extract_buffer_write_internal(buffer, data, numbytes, o_actual);
	}
	/* We can use just the cache. */
	memcpy((char *)cache->cache + cache->pos, data, numbytes);
	cache->pos += numbytes;
	if (o_actual) *o_actual = numbytes;
	return 0;
}

static inline int
extract_buffer_cat(extract_buffer_t *buffer,
		   const char	    *string)
{
	if (string == NULL)
		return 0;
	return extract_buffer_write(buffer, string, strlen(string), NULL);
}

#endif
