#include "extract/buffer.h"
#include "extract/alloc.h"

#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Check whether the whole complexity of the cache is actually justified. */

struct extract_buffer_t
{
	/* First member must be extract_buffer_cache_t - required by inline
	implementations of extract_buffer_read() and extract_buffer_write(). */
	extract_buffer_cache_t   cache;
	extract_alloc_t         *alloc;
	void                    *handle;
	extract_buffer_fn_read  *fn_read;
	extract_buffer_fn_write *fn_write;
	extract_buffer_fn_cache *fn_cache;
	extract_buffer_fn_close *fn_close;
	size_t                   pos;    /* Does not include bytes currently read/written to cache. */
};


extract_alloc_t *extract_buffer_alloc(extract_buffer_t* buffer)
{
	return buffer->alloc;
}


int extract_buffer_open(extract_alloc_t          *alloc,
			void                     *handle,
			extract_buffer_fn_read   *fn_read,
			extract_buffer_fn_write  *fn_write,
			extract_buffer_fn_cache  *fn_cache,
			extract_buffer_fn_close  *fn_close,
			extract_buffer_t        **o_buffer)
{
	extract_buffer_t *buffer;

	if (extract_malloc(alloc, &buffer, sizeof(*buffer)))
		return -1;

	buffer->alloc = alloc;
	buffer->handle = handle;
	buffer->fn_read = fn_read;
	buffer->fn_write = fn_write;
	buffer->fn_cache = fn_cache;
	buffer->fn_close = fn_close;
	buffer->cache.cache = NULL;
	buffer->cache.numbytes = 0;
	buffer->cache.pos = 0;
	buffer->pos = 0;

	*o_buffer = buffer;

	return 0;
}


size_t extract_buffer_pos(extract_buffer_t *buffer)
{
	size_t ret = buffer->pos;

	if (buffer->cache.cache)
		ret += buffer->cache.pos;

	return ret;
}


/* Send contents of cache to fn_write() using a loop to cope with short
writes. Returns with *o_actual containing the number of bytes successfully
sent, and buffer->cache.{cache,numbytes,pos} all set to zero.

If we return zero but *actual is less than original buffer->cache.numbytes,
then fn_write returned EOF. */
static int cache_flush(extract_buffer_t *buffer, size_t *o_actual)
{
	int e = -1;
	size_t p = 0;

	assert(buffer->cache.pos <= buffer->cache.numbytes);

	while (p != buffer->cache.pos)
	{
		size_t actual;
		if (buffer->fn_write(
			buffer->handle,
			(char*) buffer->cache.cache + p,
			buffer->cache.pos - p,
			&actual
			)) goto end;
		buffer->pos += actual;
		p += actual;
		if (actual == 0)
		{
			/* EOF while flushing cache. We set <pos> to the
			 * number of bytes in data..+numbytes that we know
			 * have been successfully handled by buffer->fn_write().
			 * This can be negative if we failed to flush
			 * earlier data. */
			outf("*** buffer->fn_write() EOF\n");
			e = 0;
			goto end;
		}
	}
	outfx("cache flush, buffer->pos=%i p=buffer->cache.pos=%i\n",
		buffer->pos, p);
	buffer->cache.cache = NULL;
	buffer->cache.numbytes = 0;
	buffer->cache.pos = 0;

	e = 0;
end:
	*o_actual = p;

	return e;
}

int extract_buffer_close(extract_buffer_t **p_buffer)
{
	extract_buffer_t *buffer = *p_buffer;
	int e = -1;

	if (buffer == NULL)
		return 0;

	if (buffer->cache.cache && buffer->fn_write)
	{
		/* Flush cache. */
		size_t cache_bytes = buffer->cache.pos;
		size_t actual;
		if (cache_flush(buffer, &actual)) goto end;
		if (actual != cache_bytes)
		{
			e = 1;
			goto end;
		}
	}

	if (buffer->fn_close)
		buffer->fn_close(buffer->handle);

	e = 0;
end:
	extract_free(buffer->alloc, &buffer);
	*p_buffer = NULL;

	return e;
}

static int simple_cache(void *handle, void **o_cache, size_t *o_numbytes)
{
	/* Indicate EOF. */
	(void) handle;
	*o_cache = NULL;
	*o_numbytes = 0;

	return 0;
}

int extract_buffer_open_simple(extract_alloc_t           *alloc,
				const void               *data,
				size_t                    numbytes,
				void                     *handle,
				extract_buffer_fn_close  *fn_close,
				extract_buffer_t        **o_buffer)
{
	extract_buffer_t *buffer;

	if (extract_malloc(alloc, &buffer, sizeof(*buffer)))
		return -1;

	/* We need cast away the const here. data[] will be written-to if caller
	uses us as a write buffer. */
	buffer->alloc = alloc;
	buffer->cache.cache = (void*) data;
	buffer->cache.numbytes = numbytes;
	buffer->cache.pos = 0;
	buffer->handle = handle;
	buffer->fn_read = NULL;
	buffer->fn_write = NULL;
	buffer->fn_cache = simple_cache;
	buffer->fn_close = fn_close;
	*o_buffer = buffer;

	return 0;
}


/* Implementation of extract_buffer_file*. */

static int file_read(void *handle, void *data, size_t numbytes, size_t *o_actual)
{
	FILE   *file = handle;
	size_t  n    = fread(data, 1, numbytes, file);

	outfx("file=%p numbytes=%i => n=%zi", file, numbytes, n);
	assert(o_actual); /* We are called by other extract_buffer fns, not by user code. */

	*o_actual = n;
	if (n == 0 && ferror(file))
	{
		errno = EIO;
		return -1;
	}

	return 0;
}

static int file_write(void *handle, const void *data, size_t numbytes, size_t *o_actual)
{
	FILE   *file = handle;
	size_t  n    = fwrite(data, 1 /*size*/, numbytes /*nmemb*/, file);

	outfx("file=%p numbytes=%i => n=%zi", file, numbytes, n);
	assert(o_actual); /* We are called by other extract_buffer fns, not by user code. */

	*o_actual = n;
	if (n == 0 && ferror(file))
	{
		errno = EIO;
		return -1;
	}

	return 0;
}

static void file_close(void *handle)
{
	FILE *file = handle;

	if (file)
		fclose(file);
}

int extract_buffer_open_file(extract_alloc_t *alloc, const char *path, int writable, extract_buffer_t **o_buffer)
{
	int   e    = -1;
	FILE *file = fopen(path, (writable) ? "wb" : "rb");

	if (!file)
	{
		outf("failed to open '%s': %s", path, strerror(errno));
		goto end;
	}

	if (extract_buffer_open(alloc,
				file /*handle*/,
				writable ? NULL : file_read,
				writable ? file_write : NULL,
				NULL /*fn_cache*/,
				file_close,
				o_buffer)) goto end;

	e = 0;
end:

	if (e)
	{
		if (file)
			fclose(file);
		*o_buffer = NULL;
	}

	return e;
}


/* Support for read/write. */

/* Called by extract_buffer_read() if not enough space in buffer->cache. */
int extract_buffer_read_internal(extract_buffer_t *buffer,
				void              *destination,
				size_t             numbytes,
				size_t            *o_actual)
{
	int    e   = -1;
	size_t pos = 0;    /* Number of bytes read so far. */

	/* In each iteration we either read from cache, or use buffer->fn_read()
	directly or repopulate the cache. */
	while (pos != numbytes)
	{
		size_t n = buffer->cache.numbytes - buffer->cache.pos;
		if (n)
		{
			/* There is data in cache. */
			if (n > numbytes - pos) n = numbytes - pos;
			memcpy((char *)destination + pos, (char *)buffer->cache.cache + buffer->cache.pos, n);
			pos += n;
			buffer->cache.pos += n;
		}
		/* No data in the cache - do we use fn_read or fn_cache ? */
		else if (buffer->fn_read &&
				(buffer->fn_cache == NULL ||
					(buffer->cache.numbytes && numbytes - pos > buffer->cache.numbytes / 2)))
		{
			/* Either there is no cache, or this read is large
			 * compared to previously-returned cache size, so
			 * let's ignore buffer->fn_cache and use
			 * buffer->fn_read() directly instead. */
			/* Carry on looping in case of short read. */
			size_t actual;
			outfx("using buffer->fn_read() directly for numbytes-pos=%i\n", numbytes-pos);
			if (buffer->fn_read(buffer->handle, (char*) destination + pos, numbytes - pos, &actual))
				goto end;
			if (actual == 0)
				break; /* EOF. */
			pos += actual;
			buffer->pos += actual;
		}
		else
		{
			/* Repopulate cache. */
			outfx("using buffer->fn_cache() for buffer->cache.numbytes=%i\n", buffer->cache.numbytes);
			if (buffer->fn_cache(buffer->handle, &buffer->cache.cache, &buffer->cache.numbytes))
				goto end;
			buffer->pos += buffer->cache.pos;
			buffer->cache.pos = 0;
			if (buffer->cache.numbytes == 0)
				break; /* EOF. */
		}
	}

	e = 0;
end:

	if (o_actual)
		*o_actual = pos;
	if (e == 0 && pos != numbytes)
		return +1; /* EOF. */

	return e;
}


int extract_buffer_write_internal(extract_buffer_t *buffer,
                                  const void       *source,
                                  size_t            numbytes,
                                  size_t           *o_actual)
{
	int    e   = -1;
	size_t pos = 0;    /* Number of bytes written so far. */

	if (buffer->fn_write == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	/* In each iteration we either write to cache, or use buffer->fn_write()
	directly or flush the cache. */
	while (pos != numbytes)
	{
		size_t n = buffer->cache.numbytes - buffer->cache.pos;
		outfx("numbytes=%i pos=%i. buffer->cache.numbytes=%i buffer->cache.pos=%i\n",
			numbytes, pos, buffer->cache.numbytes, buffer->cache.pos);
		if (n)
		{
			/* There is space in cache for writing. */
			if (n > numbytes - pos)
				n = numbytes - pos;
			outfx("writing to cache: numbytes=%i n=%i\n", numbytes, n);
			memcpy((char*) buffer->cache.cache + buffer->cache.pos, (char*) source + pos, n);
			pos += n;
			buffer->cache.pos += n;
		}
		else
		{
			/* No space left in cache. */
			outfx("cache empty. pos=%i. buffer->cache.numbytes=%i buffer->cache.pos=%i\n",
				pos, buffer->cache.numbytes, buffer->cache.pos);
			{
				/* Flush the cache. */
				size_t actual;
				size_t b = buffer->cache.numbytes;
				ptrdiff_t delta;
				int ee = cache_flush(buffer, &actual);
				assert(actual <= b);
				delta = actual - b;
				pos += delta;
				buffer->pos += delta;
				if (delta)
				{
					/* We have only partially flushed the cache. This
					 * is not recoverable. <pos> will be the number of
					 * bytes in source..+numbytes that have been
					 * successfully flushed, and could be negative
					 * if we failed to flush earlier data. */
					outf("failed to flush. actual=%li delta=%li\n", (long) actual, (long) delta);
					e = 0;
					goto end;
				}
				if (ee) goto end;
			}

			if (buffer->fn_cache == NULL ||
				(buffer->cache.numbytes && numbytes - pos > buffer->cache.numbytes / 2))
			{
				/* Either there is no cache, or this write is large
				 * compared to previously-returned cache size, so let's
				 * ignore the cache and call buffer->fn_write()
				 * directly instead. Carry on looping in case of short
				 * write. */
				size_t actual;
				if (buffer->fn_write(buffer->handle, (char*) source + pos, numbytes - pos, &actual))
					goto end;
				if (actual == 0)
					break; /* EOF. */
				outfx("direct write numbytes-pos=%i actual=%i buffer->pos=%i => %i\n",
					numbytes-pos, actual, buffer->pos, buffer->pos + actual);
				pos += actual;
				buffer->pos += actual;
			}
			else
			{
				/* Repopulate cache. */
				outfx("repopulating cache buffer->pos=%i", buffer->pos);
				if (buffer->fn_cache(buffer->handle, &buffer->cache.cache, &buffer->cache.numbytes))
					goto end;
				buffer->cache.pos = 0;
				if (buffer->cache.numbytes == 0)
					break;    /* EOF. */
			}
		}
	}

	e = 0;
end:

	if (o_actual)
		*o_actual = pos;
	if (e == 0 && pos != numbytes)
		e = +1; /* EOF. */

	return e;
}


static int expanding_memory_buffer_write(void *handle, const void *source, size_t numbytes, size_t *o_actual)
{
	/* We realloc our memory region as required. For efficiency, we also use
	 * any currently-unused region of our memory buffer as an extract_buffer
	 * cache. So we can be called either to 'flush the cache' (in which case we
	 * don't actually copy any data) or to accept data from somewhere else (in
	 * which case we need to increase the size of our memory region. */
	extract_buffer_expanding_t *ebe = handle;
	if ((char *)source >= ebe->data && (char *)source < ebe->data + ebe->alloc_size)
	{
		/* Source is inside our memory region so we are being called by
		 * extract_buffer_write_internal() to re-populate the cache. We don't
		 * actually have to copy anything. */
		assert((size_t) ((char *)source - ebe->data) == ebe->data_size);
		assert((size_t) ((char *)source - ebe->data + numbytes) <= ebe->alloc_size);
		ebe->data_size += numbytes;
	}
	else
	{
		/* Data is external, so copy into our buffer. We will have already been
		called to flush the cache. */
		if (extract_realloc2(ebe->buffer->alloc, &ebe->data, ebe->alloc_size, ebe->data_size + numbytes))
			return -1;
		ebe->alloc_size = ebe->data_size + numbytes;
		memcpy(ebe->data + ebe->data_size, source, numbytes);
		ebe->data_size += numbytes;
	}
	*o_actual = numbytes;

	return 0;
}

static int expanding_memory_buffer_cache(void *handle, void **o_cache, size_t *o_numbytes)
{
	extract_buffer_expanding_t *ebe   = handle;
	size_t                      delta = 4096;

	if (extract_realloc2(ebe->buffer->alloc, &ebe->data, ebe->alloc_size, ebe->data_size + delta))
		return -1;

	ebe->alloc_size = ebe->data_size + delta;
	*o_cache = ebe->data + ebe->data_size;
	*o_numbytes = delta;

	return 0;
}

int extract_buffer_expanding_create(extract_alloc_t *alloc, extract_buffer_expanding_t *ebe)
{
	ebe->data = NULL;
	ebe->data_size = 0;
	ebe->alloc_size = 0;
	if (extract_buffer_open(alloc,
				ebe,
				NULL /*fn_read*/,
				expanding_memory_buffer_write,
				expanding_memory_buffer_cache,
				NULL /*fn_close*/,
				&ebe->buffer))
		return -1;

	return 0;
}
