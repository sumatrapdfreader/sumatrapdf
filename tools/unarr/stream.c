/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr-internals.h"

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

bool ar_seek(ar_stream *stream, ptrdiff_t offset, int origin)
{
    return stream->seek(stream->data, offset, origin);
}

bool ar_skip(ar_stream *stream, ptrdiff_t count)
{
    return stream->seek(stream->data, count, SEEK_CUR);
}

size_t ar_tell(ar_stream *stream)
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

static bool file_seek(void *data, ptrdiff_t offset, int origin)
{
#ifdef _WIN64
    return _fseeki64(data, offset, origin);
#else
    return fseek(data, offset, origin) == 0;
#endif
}

static size_t file_tell(void *data)
{
#ifdef _WIN64
    return _ftelli64(data);
#else
    return ftell(data);
#endif
}

ar_stream *ar_open_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    return ar_open_stream(f, file_close, file_read, file_seek, file_tell);
}

ar_stream *ar_open_file_w(const WCHAR *path)
{
#ifdef _WIN32
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return NULL;
    return ar_open_stream(f, file_close, file_read, file_seek, file_tell);
#else
    return NULL;
#endif
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
        res = IStream_Read(data, buffer, ULONG_MAX, &cbRead);
        if (FAILED(res))
            return read;
        read += cbRead;
        buffer += (BYTE *)buffer + ULONG_MAX;
        count -= ULONG_MAX;
    }
#endif
    res = IStream_Read((IStream *)data, buffer, (ULONG)count, &cbRead);
    if (SUCCEEDED(res))
        read += cbRead;
    return read;
}

static bool stream_seek(void *data, ptrdiff_t offset, int origin)
{
    LARGE_INTEGER off;
    ULARGE_INTEGER n;
    HRESULT res;
    off.QuadPart = offset;
    res = IStream_Seek((IStream *)data, off, origin, &n);
    return SUCCEEDED(res);
}

static size_t stream_tell(void *data)
{
    LARGE_INTEGER zero = { 0 };
    ULARGE_INTEGER n = { 0 };
    IStream_Seek((IStream *)data, zero, SEEK_CUR, &n);
    return (size_t)n.QuadPart;
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
