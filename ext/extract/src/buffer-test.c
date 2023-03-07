#include "extract/buffer.h"
#include "extract/alloc.h"

#include "mem.h"
#include "memento.h"
#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>


static int rand_int(int max)
/* Returns random int from 0..max-1. */
{
    return (int) (rand() / (RAND_MAX+1.0) * max);
}


/* Support for an extract_buffer_t that reads from / writes to a fixed block of
memory, with a fn_cache() that returns a randomly-sized cache each time it is
called, and read/write functions that do random short reads and writes. */

typedef struct
{
    extract_alloc_t*    alloc;
    char*               data;
    size_t              bytes;  /* Size of data[]. */
    size_t              pos;    /* Current position in data[]. */
    char                cache[137];
    int                 num_calls_cache;
    int                 num_calls_read;
    int                 num_calls_write;
} mem_t;

static int s_read(void* handle, void* destination, size_t bytes, size_t* o_actual)
/* Does a randomised short read. */
{
    mem_t* r = handle;
    size_t n = 91;
    assert(bytes > 0);
    r->num_calls_read += 1;
    assert(r->pos <= r->bytes);
    if (n > bytes) n = bytes;
    if (n > r->bytes - r->pos) n = r->bytes - r->pos;
    if (n) n = rand_int((int) n-1) + 1;
    memcpy(destination, r->data + r->pos, n);
    r->pos += n;
    *o_actual = n;
    return 0;
}

static int s_read_cache(void* handle, void** o_cache, size_t* o_numbytes)
/* Returns a cache with randomised size. */
{
    mem_t* r = handle;
    int n;
    r->num_calls_cache += 1;
    *o_cache = r->cache;
    n = (int) (r->bytes - r->pos);
    if (n > (int) sizeof(r->cache)) n = sizeof(r->cache);
    if (n) n = rand_int( n - 1) + 1;
    memcpy(r->cache, r->data + r->pos, n);
    r->pos += n;
    *o_cache = r->cache;
    *o_numbytes = n;
    return 0;
}

static void s_read_buffer_close(void* handle)
{
    mem_t* r = handle;
    extract_free(r->alloc, &r->data);
}

static void s_create_read_buffer(extract_alloc_t* alloc, int bytes, mem_t* r, extract_buffer_t** o_buffer)
/* Creates extract_buffer_t that reads from randomised data using randomised
short reads and cache with randomised sizes. */
{
    int i;
    int e;
    if (extract_malloc(alloc, &r->data, bytes)) abort();
    for (i=0; i<bytes; ++i) {
        r->data[i] = (char) rand();
    }
    r->alloc = alloc;
    r->bytes = bytes;
    r->pos = 0;
    r->num_calls_cache = 0;
    r->num_calls_read = 0;
    r->num_calls_write = 0;
    e = extract_buffer_open(alloc, r, s_read, NULL /*write*/, s_read_cache, s_read_buffer_close, o_buffer);
    assert(!e);
}

static void test_read(void)
{
    /* Create read buffer with randomised content. */
    int len = 12345;
    mem_t r;
    char* out_buffer;
    int out_pos;
    int its;
    int e;
    extract_buffer_t* buffer;
    s_create_read_buffer(NULL /*alloc*/, len, &r, &buffer);

    /* Repeatedly read from read-buffer until we get EOF, and check we read the
    original content. */
    if (extract_malloc(r.alloc, &out_buffer, len)) abort();
    out_pos = 0;
    for (its=0;; ++its) {
        size_t actual;
        int n = rand_int(120)+1;
        int e = extract_buffer_read(buffer, out_buffer + out_pos, n, &actual);
        out_pos += (int) actual;
        assert(out_pos == (int) extract_buffer_pos(buffer));
        if (e == 1) break;
        assert(!e);
        assert(!memcmp(out_buffer, r.data, out_pos));
    }
    assert(out_pos == len);
    assert(!memcmp(out_buffer, r.data, len));
    outf("its=%i num_calls_read=%i num_calls_write=%i num_calls_cache=%i",
            its, r.num_calls_read, r.num_calls_write, r.num_calls_cache);
    extract_free(r.alloc, &out_buffer);
    out_buffer = NULL;
    e = extract_buffer_close(&buffer);
    assert(!e);

    outf("Read test passed.\n");
}


static int s_write(void* handle, const void* source, size_t bytes, size_t* o_actual)
/* Does a randomised short write. */
{
    mem_t* r = handle;
    int n = 61;
    r->num_calls_write += 1;
    if (n > (int) bytes) n = (int) bytes;
    if (n > (int) (r->bytes - r->pos)) n = (int) (r->bytes - r->pos);
    assert(n);
    n = rand_int((int) n-1) + 1;
    memcpy(r->data + r->pos, source, n);
    r->data[r->bytes] = 0;
    r->pos += n;
    *o_actual = n;
    return 0;
}

static int s_write_cache(void* handle, void** o_cache, size_t* o_numbytes)
/* Returns a cache with randomised size. */
{
    mem_t* r = handle;
    int n;
    r->num_calls_cache += 1;
    assert(r->bytes >= r->pos);
    *o_cache = r->cache;
    n = (int) (r->bytes - r->pos);
    if (n > (int) sizeof(r->cache)) n = sizeof(r->cache);
    if (n) n = rand_int( n - 1) + 1;
    *o_cache = r->cache;
    *o_numbytes = n;
    /* We will return a zero-length cache at EOF. */
    return 0;
}

static void s_write_buffer_close(void* handle)
{
    mem_t* mem = handle;
    outf("*** freeing mem->data=%p", mem->data);
    extract_free(mem->alloc, &mem->data);
}

static void s_create_write_buffer(extract_alloc_t* alloc, size_t bytes, mem_t* r, extract_buffer_t** o_buffer)
/* Creates extract_buffer_t that reads from randomised data using randomised
short reads and cache with randomised sizes. */
{
    int e;
    if (extract_malloc(alloc, &r->data, bytes+1)) abort();
    extract_bzero(r->data, bytes);
    r->alloc = alloc;
    r->bytes = bytes;
    r->pos = 0;
    r->num_calls_cache = 0;
    r->num_calls_read = 0;
    r->num_calls_write = 0;
    e = extract_buffer_open(r->alloc, r, NULL /*read*/, s_write, s_write_cache, s_write_buffer_close, o_buffer);
    assert(!e);
}


static void test_write(void)
{
    /* Create write buffer. */
    size_t len = 12345;
    mem_t r;
    extract_buffer_t* buffer;
    char* out_buffer;
    unsigned i;
    size_t out_pos = 0;
    int its;
    int e;

    s_create_write_buffer(NULL /*alloc*/, len, &r, &buffer);

    /* Write to read-buffer, and check it contains the original content. */
    if (extract_malloc(r.alloc, &out_buffer, len)) abort();
    for (i=0; i<len; ++i) {
        out_buffer[i] = (char) ('a' + rand_int(26));
    }
    for (its=0;; ++its) {
        size_t actual;
        size_t n = rand_int(12)+1;
        int e = extract_buffer_write(buffer, out_buffer+out_pos, n, &actual);
        out_pos += actual;
        assert(out_pos == extract_buffer_pos(buffer));
        if (e == 1) break;
        assert(!e);
    }
    assert(out_pos == len);
    assert(!memcmp(out_buffer, r.data, len));
    extract_free(r.alloc, &out_buffer);
    outf("its=%i num_calls_read=%i num_calls_write=%i num_calls_cache=%i",
            its, r.num_calls_read, r.num_calls_write, r.num_calls_cache);
    e = extract_buffer_close(&buffer);
    assert(!e);
    outf("Write test passed.\n");
}

static void test_file(void)
{
    /* Check we can write 3 bytes to file. */
    extract_buffer_t* file_buffer;
    if (extract_buffer_open_file(NULL /*alloc*/, "test/generated/buffer-file", 1 /*writable*/, &file_buffer)) abort();

    {
        size_t  n;
        int e;
        errno = 0;
        e = extract_buffer_write(file_buffer, "foo", 3, &n);
        if (e == 0 && n == 3) {}
        else {
            outf("extract_buffer_write() returned e=%i errno=%i n=%zi", e, errno, n);
            abort();
        }
    }
    if (extract_buffer_close(&file_buffer)) abort();

    /* Check we get back expected short reads and EOF when reading from 3-byte
    file created above. */
    if (extract_buffer_open_file(NULL /*alloc*/, "test/generated/buffer-file", 0 /*writable*/, &file_buffer)) abort();

    {
        size_t  n;
        char    buffer[10];
        int     e;
        errno = 0;
        e = extract_buffer_read(file_buffer, buffer, 2, &n);
        if (e == 0 && n == 2) {}
        else {
            outf("extract_buffer_read() returned e=%i errno=%i n=%zi", e, errno, n);
            abort();
        }
        e = extract_buffer_read(file_buffer, buffer, 3, &n);
        if (e == 1 && n == 1) {}
        else {
            outf("extract_buffer_read() returned e=%i errno=%i n=%zi", e, errno, n);
            abort();
        }
        e = extract_buffer_read(file_buffer, buffer, 3, &n);
        if (e == 1 && n == 0) {}
        else {
            outf("extract_buffer_read() returned e=%i errno=%i n=%zi", e, errno, n);
            abort();
        }
    }
    if (extract_buffer_close(&file_buffer)) abort();

    /* Check writing to read-only file buffer fails. */
    {
        int e;
        char text[] = "hello world";
        size_t  actual;
        if (extract_buffer_open_file(NULL /*alloc*/, "test/generated/buffer-file", 0 /*writable*/, &file_buffer)) {
            abort();
        }

        e = extract_buffer_write(file_buffer, text, sizeof(text)-1, &actual);
        outf("extract_buffer_write() on read buffer returned e=%i actual=%zi", e, actual);
        if (e != -1 || errno != EINVAL) abort();
        if (extract_buffer_close(&file_buffer)) abort();
    }

    outf("file buffer tests passed.\n");
}

int main(void)
{
    extract_outf_verbose_set(1);
    test_read();
    test_write();
    test_file();
    return 0;
}
