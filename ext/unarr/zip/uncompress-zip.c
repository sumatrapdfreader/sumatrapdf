/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "zip.h"

#ifdef HAVE_ZLIB
static void *gZlib_Alloc(void *self, uInt count, uInt size) { (void)self; return calloc(count, size); }
static void gZlib_Free(void *self, void *ptr) { (void)self; free(ptr); }

static bool zip_init_uncompress_deflate(struct ar_archive_zip_uncomp *uncomp, bool deflate64)
{
    int err;

    uncomp->state.zstream.zalloc = gZlib_Alloc;
    uncomp->state.zstream.zfree = gZlib_Free;
    uncomp->state.zstream.opaque = NULL;

    err = inflateInit2(&uncomp->state.zstream, deflate64 ? -16 : -15);
    return err == Z_OK;
}

static size_t zip_uncompress_data_deflate(struct ar_archive_zip_uncomp *uncomp, void *buffer, unsigned int buffer_size)
{
    int err;

    uncomp->state.zstream.next_in = &uncomp->input.data[uncomp->input.offset];
    uncomp->state.zstream.avail_in = uncomp->input.bytes_left;
    uncomp->state.zstream.next_out = buffer;
    uncomp->state.zstream.avail_out = buffer_size;

    err = inflate(&uncomp->state.zstream, Z_SYNC_FLUSH);

    uncomp->input.offset += uncomp->input.bytes_left - (uint16_t)uncomp->state.zstream.avail_in;
    uncomp->input.bytes_left = (uint16_t)uncomp->state.zstream.avail_in;

    if (err != Z_OK && err != Z_STREAM_END) {
        warn("Unexpected ZLIB error %d", err);
        return 0;
    }
    if (err == Z_STREAM_END && uncomp->input.bytes_left) {
        warn("Premature EOS in Deflate stream");
        return 0;
    }
    
    return buffer_size - uncomp->state.zstream.avail_out;
}

static void zip_clear_uncompress_deflate(struct ar_archive_zip_uncomp *uncomp)
{
    inflateEnd(&uncomp->state.zstream);
}
#endif

#ifdef HAVE_BZIP2
static void *gBzip2_Alloc(void *self, int count, int size) { (void)self; return calloc(count, size); }
static void gBzip2_Free(void *self, void *ptr) { (void)self; free(ptr); }

static bool zip_init_uncompress_bzip2(struct ar_archive_zip_uncomp *uncomp)
{
    int err;

    uncomp->state.bstream.bzalloc = gBzip2_Alloc;
    uncomp->state.bstream.bzfree = gBzip2_Free;
    uncomp->state.bstream.opaque = NULL;

    err = BZ2_bzDecompressInit(&uncomp->state.bstream, 0, 0);
    return err == BZ_OK;
}

static size_t zip_uncompress_data_bzip2(struct ar_archive_zip_uncomp *uncomp, void *buffer, unsigned int buffer_size)
{
    int err;

    uncomp->state.bstream.next_in = (char *)&uncomp->input.data[uncomp->input.offset];
    uncomp->state.bstream.avail_in = uncomp->input.bytes_left;
    uncomp->state.bstream.next_out = (char *)buffer;
    uncomp->state.bstream.avail_out = buffer_size;

    err = BZ2_bzDecompress(&uncomp->state.bstream);

    uncomp->input.offset += uncomp->input.bytes_left - (uint16_t)uncomp->state.bstream.avail_in;
    uncomp->input.bytes_left = (uint16_t)uncomp->state.bstream.avail_in;

    if (err != BZ_OK && err != BZ_STREAM_END) {
        warn("Unexpected BZIP2 error %d", err);
        return 0;
    }
    if (err == BZ_STREAM_END && uncomp->input.bytes_left) {
        warn("Premature EOS in BZIP2 stream");
        return 0;
    }
    
    return buffer_size - uncomp->state.bstream.avail_out;
}

static void zip_clear_uncompress_bzip2(struct ar_archive_zip_uncomp *uncomp)
{
    BZ2_bzDecompressEnd(&uncomp->state.bstream);
}
#endif

#ifdef HAVE_LZMA
static void *gSzAlloc_Alloc(void *self, size_t size) { (void)self; return malloc(size); }
static void gSzAlloc_Free(void *self, void *ptr) { (void)self; free(ptr); }
static ISzAlloc gSzAlloc = { gSzAlloc_Alloc, gSzAlloc_Free };

static bool zip_init_uncompress_lzma(struct ar_archive_zip_uncomp *uncomp)
{
    uncomp->state.lzmadec = calloc(1, sizeof(*uncomp->state.lzmadec));
    if (!uncomp->state.lzmadec)
        return false;
    LzmaDec_Construct(uncomp->state.lzmadec);
    return true;
}

static size_t zip_uncompress_data_lzma(struct ar_archive_zip_uncomp *uncomp, void *buffer, unsigned int buffer_size)
{
    ELzmaFinishMode lzmafinish = (uncomp->flags & (1 << 1)) ? LZMA_FINISH_END : LZMA_FINISH_ANY;
    SizeT srclen, dstlen;
    ELzmaStatus status;
    SRes res = SZ_OK;

    if (!uncomp->state.lzmadec->dic) {
        uint8_t propsize;
        if (uncomp->input.bytes_left < 9) {
            warn("Insufficient data in LZMA stream");
            return 0;
        }
        propsize = uncomp->input.data[uncomp->input.offset + 2];
        if (uncomp->input.data[uncomp->input.offset + 3] != 0 || uncomp->input.bytes_left < 4 + propsize) {
            warn("Insufficient data in LZMA stream");
            return 0;
        }
        res = LzmaDec_Allocate(uncomp->state.lzmadec, &uncomp->input.data[uncomp->input.offset + 4], propsize, &gSzAlloc);
        uncomp->input.offset += 4 + propsize;
        if (res != SZ_OK)
            return 0;
        LzmaDec_Init(uncomp->state.lzmadec);
    }

    srclen = uncomp->input.bytes_left;
    dstlen = buffer_size;
    res = LzmaDec_DecodeToBuf(uncomp->state.lzmadec, buffer, &dstlen, &uncomp->input.data[uncomp->input.offset], &srclen, lzmafinish, &status);

    uncomp->input.offset += (uint16_t)srclen;
    uncomp->input.bytes_left -= (uint16_t)srclen;

    if (res != SZ_OK || (srclen == 0 && dstlen == 0))
        return 0;
    if (status == LZMA_STATUS_FINISHED_WITH_MARK && uncomp->input.bytes_left) {
        warn("Premature EOS in LZMA stream");
        return 0;
    }

    return (unsigned int)dstlen;
}

static void zip_clear_uncompress_lzma(struct ar_archive_zip_uncomp *uncomp)
{
    LzmaDec_Free(uncomp->state.lzmadec, &gSzAlloc);
    free(uncomp->state.lzmadec);
}
#endif

static bool zip_init_uncompress(struct ar_archive_zip_uncomp *uncomp, uint16_t method, uint16_t flags)
{
    if (uncomp->initialized)
        return true;
    memset(uncomp, 0, sizeof(*uncomp));
    uncomp->flags = flags;
    if (method == METHOD_DEFLATE || method == METHOD_DEFLATE64) {
#ifdef HAVE_ZLIB
        if (zip_init_uncompress_deflate(uncomp, method == METHOD_DEFLATE64)) {
            uncomp->uncompress_data = zip_uncompress_data_deflate;
            uncomp->clear_state = zip_clear_uncompress_deflate;
        }
#else
        warn("Deflate support requires ZLIB (define HAVE_ZLIB)");
#endif
    }
    else if (method == METHOD_BZIP2) {
#ifdef HAVE_BZIP2
        if (zip_init_uncompress_bzip2(uncomp)) {
            uncomp->uncompress_data = zip_uncompress_data_bzip2;
            uncomp->clear_state = zip_clear_uncompress_bzip2;
        }
#else
        warn("BZIP2 support requires BZIP2 (define HAVE_BZIP2)");
#endif
    }
    else if (method == METHOD_LZMA) {
#ifdef HAVE_LZMA
        if (zip_init_uncompress_lzma(uncomp)) {
            uncomp->uncompress_data = zip_uncompress_data_lzma;
            uncomp->clear_state = zip_clear_uncompress_lzma;
        }
#else
        warn("LZMA support requires LZMA SDK (define HAVE_LZMA)");
#endif
    }
    else
        warn("Unsupported compression method %d", method);
    uncomp->initialized = uncomp->uncompress_data != NULL && uncomp->clear_state != NULL;
    return uncomp->initialized;
}

void zip_clear_uncompress(struct ar_archive_zip_uncomp *uncomp)
{
    if (!uncomp->initialized)
        return;
    uncomp->clear_state(uncomp);
    uncomp->initialized = false;
}

bool zip_uncompress_part(ar_archive_zip *zip, void *buffer, size_t buffer_size)
{
    struct ar_archive_zip_uncomp *uncomp = &zip->uncomp;
    unsigned int count;

    if (!zip_init_uncompress(uncomp, zip->entry.method, zip->entry.flags))
        return false;

    for (;;) {
        if (buffer_size == 0)
            return true;

        if (uncomp->input.bytes_left < sizeof(uncomp->input.data) / 2 && zip->progr.data_left) {
            if (uncomp->input.offset) {
                memmove(&uncomp->input.data[0], &uncomp->input.data[uncomp->input.offset], uncomp->input.bytes_left);
                uncomp->input.offset = 0;
            }
            count = sizeof(uncomp->input.data) - uncomp->input.bytes_left;
            if (count > zip->progr.data_left)
                count = (unsigned int)zip->progr.data_left;
            if (ar_read(zip->super.stream, &uncomp->input.data[uncomp->input.bytes_left], count) != count) {
                warn("Unexpected EOF during decompression (invalid data size?)");
                return false;
            }
            zip->progr.data_left -= count;
            uncomp->input.bytes_left += (uint16_t)count;
        }

        count = uncomp->uncompress_data(uncomp, buffer, buffer_size > UINT_MAX ? UINT_MAX : (unsigned int)buffer_size);
        if (count == 0)
            return false;
        zip->progr.bytes_done += count;
        buffer = (uint8_t *)buffer + count;
        buffer_size -= count;
    }
}
