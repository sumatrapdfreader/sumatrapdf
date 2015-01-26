/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "_7z.h"

#ifdef HAVE_7Z

static void *gSzAlloc_Alloc(void *self, size_t size) { (void)self; return malloc(size); }
static void gSzAlloc_Free(void *self, void *ptr) { (void)self; free(ptr); }
static ISzAlloc gSzAlloc = { gSzAlloc_Alloc, gSzAlloc_Free };

static SRes CSeekStream_Read(void *p, void *data, size_t *size)
{
    struct CSeekStream *stm = p;
    *size = ar_read(stm->stream, data, *size);
    return SZ_OK;
}

static SRes CSeekStream_Seek(void *p, Int64 *pos, ESzSeek origin)
{
    struct CSeekStream *stm = p;
    if (!ar_seek(stm->stream, *pos, (int)origin))
        return SZ_ERROR_FAIL;
    *pos = ar_tell(stm->stream);
    return SZ_OK;
}

static void CSeekStream_CreateVTable(struct CSeekStream *in_stream, ar_stream *stream)
{
    in_stream->super.Read = CSeekStream_Read;
    in_stream->super.Seek = CSeekStream_Seek;
    in_stream->stream = stream;
}

#ifndef USE_7Z_CRC32
UInt32 MY_FAST_CALL CrcCalc(const void *data, size_t size)
{
    return ar_crc32(0, data, size);
}
#endif

static void _7z_close(ar_archive *ar)
{
    ar_archive_7z *_7z = (ar_archive_7z *)ar;
    free(_7z->entry_name);
    SzArEx_Free(&_7z->data, &gSzAlloc);
    IAlloc_Free(&gSzAlloc, _7z->uncomp.buffer);
}

static const char *_7z_get_name(ar_archive *ar);

static bool _7z_parse_entry(ar_archive *ar, off64_t offset)
{
    ar_archive_7z *_7z = (ar_archive_7z *)ar;
    const CSzFileItem *item = _7z->data.db.Files + offset;

    if (offset < 0 || offset > _7z->data.db.NumFiles) {
        warn("Offsets must be between 0 and %u", _7z->data.db.NumFiles);
        return false;
    }
    if (offset == _7z->data.db.NumFiles) {
        ar->at_eof = true;
        return false;
    }

    ar->entry_offset = offset;
    ar->entry_offset_next = offset + 1;
    ar->entry_size_uncompressed = (size_t)item->Size;
    ar->entry_filetime = item->MTimeDefined ? (time64_t)(item->MTime.Low | ((time64_t)item->MTime.High << 32)) : 0;

    free(_7z->entry_name);
    _7z->entry_name = NULL;
    _7z->uncomp.initialized = false;

    if (item->IsDir) {
        log("Skipping directory entry \"%s\"", _7z_get_name(ar));
        return _7z_parse_entry(ar, offset + 1);
    }

    return true;
}

static char *SzArEx_GetFileNameUtf8(const CSzArEx *p, UInt32 fileIndex)
{
    size_t len = p->FileNameOffsets[fileIndex + 1] - p->FileNameOffsets[fileIndex];
    const Byte *src = p->FileNames.data + p->FileNameOffsets[fileIndex] * 2;
    const Byte *srcEnd = src + len * 2;
    size_t size = len * 3;
    char *str, *out;

    if (size == (size_t)-1)
        return NULL;
    str = malloc(size + 1);
    if (!str)
        return NULL;

    for (out = str; src < srcEnd - 1; src += 2) {
        out += ar_conv_rune_to_utf8(src[0] | src[1] << 8, out, str + size - out);
    }
    *out = '\0';

    return str;
}

static const char *_7z_get_name(ar_archive *ar)
{
    ar_archive_7z *_7z = (ar_archive_7z *)ar;
    if (!_7z->entry_name && ar->entry_offset_next && !ar->at_eof) {
        _7z->entry_name = SzArEx_GetFileNameUtf8(&_7z->data, (UInt32)ar->entry_offset);
        /* normalize path separators */
        if (_7z->entry_name) {
            char *p = _7z->entry_name;
            while ((p = strchr(p, '\\')) != NULL) {
                *p = '/';
            }
        }
    }
    return _7z->entry_name;
}

static bool _7z_uncompress(ar_archive *ar, void *buffer, size_t buffer_size)
{
    ar_archive_7z *_7z = (ar_archive_7z *)ar;
    struct ar_archive_7z_uncomp *uncomp = &_7z->uncomp;

    if (!uncomp->initialized) {
        /* TODO: this uncompresses all data for solid compressions */
        SRes res = SzArEx_Extract(&_7z->data, &_7z->look_stream.s, (UInt32)ar->entry_offset, &uncomp->folder_index, &uncomp->buffer, &uncomp->buffer_size, &uncomp->offset, &uncomp->bytes_left, &gSzAlloc, &gSzAlloc);
        if (res != SZ_OK) {
            warn("Failed to extract file at index %" PRIi64 " (failed with error %d)", ar->entry_offset, res);
            return false;
        }
        if (uncomp->bytes_left != ar->entry_size_uncompressed) {
            warn("Uncompressed sizes don't match (%" PRIuPTR " != %" PRIuPTR ")", uncomp->bytes_left, ar->entry_size_uncompressed);
            return false;
        }
        uncomp->initialized = true;
    }

    if (buffer_size > uncomp->bytes_left) {
        warn("Requesting too much data (%" PRIuPTR " < %" PRIuPTR ")", uncomp->bytes_left, buffer_size);
        return false;
    }

    memcpy(buffer, uncomp->buffer + uncomp->offset + ar->entry_size_uncompressed - uncomp->bytes_left, buffer_size);
    uncomp->bytes_left -= buffer_size;

    return true;
}

ar_archive *ar_open_7z_archive(ar_stream *stream)
{
    ar_archive *ar;
    ar_archive_7z *_7z;
    SRes res;

    if (!ar_seek(stream, 0, SEEK_SET))
        return NULL;

    ar = ar_open_archive(stream, sizeof(ar_archive_7z), _7z_close, _7z_parse_entry, _7z_get_name, _7z_uncompress, NULL, 0);
    if (!ar)
        return NULL;

    _7z = (ar_archive_7z *)ar;
    CSeekStream_CreateVTable(&_7z->in_stream, stream);
    LookToRead_CreateVTable(&_7z->look_stream, False);
    _7z->look_stream.realStream = &_7z->in_stream.super;
    LookToRead_Init(&_7z->look_stream);

#ifdef USE_7Z_CRC32
    CrcGenerateTable();
#endif

    SzArEx_Init(&_7z->data);
    res = SzArEx_Open(&_7z->data, &_7z->look_stream.s, &gSzAlloc, &gSzAlloc);
    if (res != SZ_OK) {
        if (res != SZ_ERROR_NO_ARCHIVE)
            warn("Invalid 7z archive (failed with error %d)", res);
        free(ar);
        return NULL;
    }

    return ar;
}

#else

ar_archive *ar_open_7z_archive(ar_stream *stream)
{
    (void)stream;
    warn("7z support requires 7z SDK (define HAVE_7Z)");
    return NULL;
}

#endif
