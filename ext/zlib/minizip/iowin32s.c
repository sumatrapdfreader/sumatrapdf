/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "zlib.h"
#include "ioapi.h"
#define COBJMACROS
#include "iowin32s.h"

typedef struct {
    IStream *stream;
    HRESULT error;
} Win32sIOState;

uLong ZCALLBACK win32s_read_file_func(voidpf opaque, voidpf stream, void *buf, uLong size)
{
    Win32sIOState *state = stream;
    ULONG cbRead = 0;
    state->error = IStream_Read(state->stream, buf, size, &cbRead);
    return cbRead;
}

uLong ZCALLBACK win32s_write_file_func(voidpf opaque, voidpf stream, const void *buf, uLong size)
{
    Win32sIOState *state = stream;
    ULONG cbWritten = 0;
    if (!state)
        return 0;
    state->error = IStream_Write(state->stream, buf, size, &cbWritten);
    return cbWritten;
}

ZPOS64_T ZCALLBACK win32s_tell64_file_func(voidpf opaque, voidpf stream)
{
    Win32sIOState *state = stream;
    ULARGE_INTEGER n;
    LARGE_INTEGER off;
    if (!state)
        return -1;
    off.QuadPart = 0;
    n.QuadPart = -1;
    state->error = IStream_Seek(state->stream, off, STREAM_SEEK_CUR, &n);
    return n.QuadPart;
}

long ZCALLBACK win32s_seek64_file_func(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    Win32sIOState *state = stream;
    LARGE_INTEGER off;
    if (!state)
        return -1;
    off.QuadPart = offset;
    state->error = IStream_Seek(state->stream, off, origin, NULL);
    return FAILED(state->error);
}

voidpf ZCALLBACK win32s_open64_file_func(voidpf opaque, const void *stream, int mode)
{
    Win32sIOState *state = malloc(sizeof(Win32sIOState));
    if (!state || !stream)
    {
        free(state);
        return NULL;
    }
    state->stream = (IStream *)stream;
    if (win32s_seek64_file_func(opaque, state, 0, ZLIB_FILEFUNC_SEEK_SET) != 0)
    {
        free(state);
        return NULL;
    }
    IStream_AddRef(state->stream);
    state->error = 0;
    return state;
}

int ZCALLBACK win32s_close_file_func(voidpf opaque, voidpf stream)
{
    Win32sIOState *state = stream;
    if (!state)
        return -1;
    IStream_Release(state->stream);
    free(state);
    return 0;
}

int ZCALLBACK win32s_error_file_func(voidpf opaque, voidpf stream)
{
    Win32sIOState *state = stream;
    if (!state)
        return -1;
    return (int)state->error;
}

void fill_win32s_filefunc64(zlib_filefunc64_def *pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32s_open64_file_func;
    pzlib_filefunc_def->zread_file = win32s_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32s_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32s_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32s_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32s_close_file_func;
    pzlib_filefunc_def->zerror_file = win32s_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}
