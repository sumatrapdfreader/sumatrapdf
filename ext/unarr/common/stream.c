/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr-imp.h"

ar_stream *ar_open_stream(void *data, ar_stream_close_fn close, ar_stream_read_fn read, ar_stream_seek_fn seek, ar_stream_tell_fn tell)
{
    ar_stream *stream = malloc(sizeof(ar_stream));
    if (!stream) {
        close(data);
        return NULL;
    }
    stream->data = data;
    stream->close = close;
    stream->read = read;
    stream->seek = seek;
    stream->tell = tell;
    return stream;
}

void ar_close(ar_stream *stream)
{
    if (stream)
        stream->close(stream->data);
    free(stream);
}

size_t ar_read(ar_stream *stream, void *buffer, size_t count)
{
    return stream->read(stream->data, buffer, count);
}

bool ar_seek(ar_stream *stream, off64_t offset, int origin)
{
    return stream->seek(stream->data, offset, origin);
}

bool ar_skip(ar_stream *stream, off64_t count)
{
    return stream->seek(stream->data, count, SEEK_CUR);
}

off64_t ar_tell(ar_stream *stream)
{
    return stream->tell(stream->data);
}

/***** stream based on FILE *****/

static void file_close(void *data)
{
    fclose(data);
}

static size_t file_read(void *data, void *buffer, size_t count)
{
    return fread(buffer, 1, count, data);
}

static bool file_seek(void *data, off64_t offset, int origin)
{
#ifdef _MSC_VER
    return _fseeki64(data, offset, origin) == 0;
#else
#if _POSIX_C_SOURCE >= 200112L
    if (sizeof(off_t) == 8)
        return fseeko(data, offset, origin) == 0;
#endif
    if (offset > INT32_MAX || offset < INT32_MIN)
        return false;
    return fseek(data, (long)offset, origin) == 0;
#endif
}

static off64_t file_tell(void *data)
{
#ifdef _MSC_VER
    return _ftelli64(data);
#elif _POSIX_C_SOURCE >= 200112L
    return ftello(data);
#else
    return ftell(data);
#endif
}

ar_stream *ar_open(FILE *f)
{
    if (!f)
        return NULL;
    return ar_open_stream(f, file_close, file_read, file_seek, file_tell);
}

ar_stream *ar_open_file(const char *path)
{
    FILE *f = path ? fopen(path, "rb") : NULL;
    return ar_open(f);
}

#ifdef _WIN32
ar_stream *ar_open_file_w(const wchar_t *path)
{
    FILE *f = path ? _wfopen(path, L"rb") : NULL;
    return ar_open(f);
}
#endif

/***** stream based on preallocated memory *****/

struct MemoryStream {
    const uint8_t *data;
    size_t length;
    size_t offset;
};

static void memory_close(void *data)
{
    struct MemoryStream *stm = data;
    free(stm);
}

static size_t memory_read(void *data, void *buffer, size_t count)
{
    struct MemoryStream *stm = data;
    if (count > stm->length - stm->offset)
        count = stm->length - stm->offset;
    memcpy(buffer, stm->data + stm->offset, count);
    stm->offset += count;
    return count;
}

static bool memory_seek(void *data, off64_t offset, int origin)
{
    struct MemoryStream *stm = data;
    if (origin == SEEK_CUR)
        offset += stm->offset;
    else if (origin == SEEK_END)
        offset += stm->length;
    if (offset < 0 || offset > (off64_t)stm->length || (size_t)offset > stm->length)
        return false;
    stm->offset = (size_t)offset;
    return true;
}

static off64_t memory_tell(void *data)
{
    struct MemoryStream *stm = data;
    return stm->offset;
}

ar_stream *ar_open_memory(const void *data, size_t datalen)
{
    struct MemoryStream *stm = malloc(sizeof(struct MemoryStream));
    if (!stm)
        return NULL;
    stm->data = data;
    stm->length = datalen;
    stm->offset = 0;
    return ar_open_stream(stm, memory_close, memory_read, memory_seek, memory_tell);
}

#ifdef _WIN32
/***** stream based on IStream *****/

#define COBJMACROS
#include <windows.h>

static void stream_close(void *data)
{
    IUnknown_Release((IStream *)data);
}

static size_t stream_read(void *data, void *buffer, size_t count)
{
    size_t read = 0;
    HRESULT res;
    ULONG cbRead;
#ifdef _WIN64
    while (count > ULONG_MAX) {
        res = IStream_Read((IStream *)data, buffer, ULONG_MAX, &cbRead);
        if (FAILED(res))
            return read;
        read += cbRead;
        buffer = (BYTE *)buffer + ULONG_MAX;
        count -= ULONG_MAX;
    }
#endif
    res = IStream_Read((IStream *)data, buffer, (ULONG)count, &cbRead);
    if (SUCCEEDED(res))
        read += cbRead;
    return read;
}

static bool stream_seek(void *data, off64_t offset, int origin)
{
    LARGE_INTEGER off;
    ULARGE_INTEGER n;
    HRESULT res;
    off.QuadPart = offset;
    res = IStream_Seek((IStream *)data, off, origin, &n);
    return SUCCEEDED(res);
}

static off64_t stream_tell(void *data)
{
    LARGE_INTEGER zero = { 0 };
    ULARGE_INTEGER n = { 0 };
    IStream_Seek((IStream *)data, zero, SEEK_CUR, &n);
    return (off64_t)n.QuadPart;
}

ar_stream *ar_open_istream(IStream *stream)
{
    LARGE_INTEGER zero = { 0 };
    HRESULT res = IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    if (FAILED(res))
        return NULL;
    IUnknown_AddRef(stream);
    return ar_open_stream(stream, stream_close, stream_read, stream_seek, stream_tell);
}
#endif
