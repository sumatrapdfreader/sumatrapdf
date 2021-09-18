/* Implementation of inline functions.

We expose some implementation details to allow extract_buffer_read() and
extract_buffer_write() to be inline; specifically we allow the compiler to
optimise the common case where reading/writing uses the cache only. */


#include <string.h>

typedef struct
{
    void*   cache;
    size_t  numbytes;
    size_t  pos;
} extract_buffer_cache_t;


int extract_buffer_read_internal(
        extract_buffer_t*   buffer,
        void*               data,
        size_t              numbytes,
        size_t*             o_actual
        );
/* Internal use only. */

static inline int extract_buffer_read(
        extract_buffer_t*   buffer,
        void*               data,
        size_t              numbytes,
        size_t*             o_actual
        )
{
    extract_buffer_cache_t* cache = (extract_buffer_cache_t*)(void*) buffer;
    if (cache->numbytes - cache->pos < numbytes) {
        /* Can't use just the cache. */
        return extract_buffer_read_internal(buffer, data, numbytes, o_actual);
    }
    /* We can use just the cache. */
    memcpy(data, (char*) cache->cache + cache->pos, numbytes);
    cache->pos += numbytes;
    if (o_actual) *o_actual = numbytes;
    return 0;
}


int extract_buffer_write_internal(
        extract_buffer_t*   buffer,
        const void*         data,
        size_t              numbytes,
        size_t*             o_actual
        );
/* Internal use only. */

static inline int extract_buffer_write(
        extract_buffer_t*   buffer,
        const void*         data,
        size_t              numbytes,
        size_t*             o_actual
        )
{
    extract_buffer_cache_t* cache = (extract_buffer_cache_t*)(void*) buffer;
    if (cache->numbytes - cache->pos < numbytes) {
        /* Can't use just the cache. */
        return extract_buffer_write_internal(buffer, data, numbytes, o_actual);
    }
    /* We can use just the cache. */
    memcpy((char*) cache->cache + cache->pos, data, numbytes);
    cache->pos += numbytes;
    if (o_actual) *o_actual = numbytes;
    return 0;
}

