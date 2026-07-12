#ifndef ARTIFEX_EXTRACT_BUFFER_H
#define ARTIFEX_EXTRACT_BUFFER_H

#include "extract/alloc.h"
#include "extract/extract.h"

#include <stddef.h>
#include <string.h>

#if !defined __cplusplus && defined(_MSC_VER)
	#if (_MSC_VER < 1500)

		#define inline
	#else

		#define inline __inline
	#endif
#endif

static inline int
extract_buffer_read(extract_buffer_t *buffer,
			void	     *data,
			size_t	      numbytes,
			size_t	     *o_actual);

static inline int
extract_buffer_write(extract_buffer_t *buffer,
			const void    *data,
			size_t	       numbytes,
			size_t	      *o_actual);

static inline int
extract_buffer_cat(extract_buffer_t *buffer,
			const char  *string);

size_t extract_buffer_pos(extract_buffer_t *buffer);

int extract_buffer_close(extract_buffer_t **io_buffer);

typedef int (extract_buffer_fn_read)(void *handle, void *destination, size_t numbytes, size_t *o_actual);

typedef int (extract_buffer_fn_write)(void *handle, const void *source, size_t numbytes, size_t *o_actual);

typedef int (extract_buffer_fn_cache)(void *handle, void **o_cache, size_t *o_numbytes);

typedef void (extract_buffer_fn_close)(void *handle);

extract_alloc_t *extract_buffer_alloc(extract_buffer_t *buffer);

int extract_buffer_open(extract_alloc_t		 *alloc,
			void			 *handle,
			extract_buffer_fn_read	 *fn_read,
			extract_buffer_fn_write	 *fn_write,
			extract_buffer_fn_cache	 *fn_cache,
			extract_buffer_fn_close	 *fn_close,
			extract_buffer_t	**o_buffer);

int extract_buffer_open_simple(extract_alloc_t           *alloc,
				const void               *data,
				size_t                    numbytes,
				void                     *handle,
				extract_buffer_fn_close  *fn_close,
				extract_buffer_t        **o_buffer);

int extract_buffer_open_file(extract_alloc_t      *alloc,
				const char        *path,
				int                writable,
				extract_buffer_t **o_buffer);

typedef struct
{
	extract_buffer_t *buffer;
	char             *data;
	size_t            alloc_size;
	size_t            data_size;
} extract_buffer_expanding_t;

int extract_buffer_expanding_create(extract_alloc_t        *alloc,
				extract_buffer_expanding_t *buffer_expanding);

typedef struct
{
	void   *cache;
	size_t  numbytes;
	size_t  pos;
} extract_buffer_cache_t;

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

		return extract_buffer_read_internal(buffer, data, numbytes, o_actual);
	}

	memcpy(data, (char *)cache->cache + cache->pos, numbytes);
	cache->pos += numbytes;
	if (o_actual) *o_actual = numbytes;
	return 0;
}

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

		return extract_buffer_write_internal(buffer, data, numbytes, o_actual);
	}

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
