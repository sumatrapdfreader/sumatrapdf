/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "zip.h"

#define ERR_UNCOMP UINT32_MAX

static bool zip_fill_input_buffer(ar_archive_zip *zip)
{
    struct ar_archive_zip_uncomp *uncomp = &zip->uncomp;
    size_t count;

    if (uncomp->input.offset) {
        memmove(&uncomp->input.data[0], &uncomp->input.data[uncomp->input.offset], uncomp->input.bytes_left);
        uncomp->input.offset = 0;
    }
    count = sizeof(uncomp->input.data) - uncomp->input.bytes_left;
    if (count > zip->progress.data_left)
        count = zip->progress.data_left;
    if (ar_read(zip->super.stream, &uncomp->input.data[uncomp->input.bytes_left], count) != count) {
        warn("Unexpected EOF during decompression (invalid data size?)");
        return false;
    }
    zip->progress.data_left -= count;
    uncomp->input.bytes_left += (uint16_t)count;

    return true;
}

/***** Deflate and Deflate64 compression *****/

#ifdef HAVE_ZLIB
static void *gZlib_Alloc(void *opaque, uInt count, uInt size) { (void)opaque; return calloc(count, size); }
static void gZlib_Free(void *opaque, void *ptr) { (void)opaque; free(ptr); }

static bool zip_init_uncompress_deflate(struct ar_archive_zip_uncomp *uncomp, bool deflate64)
{
    int err;

    uncomp->state.zstream.zalloc = gZlib_Alloc;
    uncomp->state.zstream.zfree = gZlib_Free;
    uncomp->state.zstream.opaque = NULL;

    err = inflateInit2(&uncomp->state.zstream, deflate64 ? -16 : -15);
    if (err == Z_STREAM_ERROR && deflate64)
        warn("ZLIB doesn't seem to support Deflate64");
    return err == Z_OK;
}

static uint32_t zip_uncompress_data_deflate(struct ar_archive_zip_uncomp *uncomp, void *buffer, uint32_t buffer_size)
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
        return ERR_UNCOMP;
    }
    if (err == Z_STREAM_END && uncomp->input.bytes_left && uncomp->state.zstream.avail_out == buffer_size) {
        warn("Premature EOS in Deflate stream");
        return ERR_UNCOMP;
    }

    return buffer_size - uncomp->state.zstream.avail_out;
}

static void zip_clear_uncompress_deflate(struct ar_archive_zip_uncomp *uncomp)
{
    inflateEnd(&uncomp->state.zstream);
}
#endif

/***** BZIP2 compression *****/

#ifdef HAVE_BZIP2
static void *gBzip2_Alloc(void *opaque, int count, int size) { (void)opaque; return calloc(count, size); }
static void gBzip2_Free(void *opaque, void *ptr) { (void)opaque; free(ptr); }

static bool zip_init_uncompress_bzip2(struct ar_archive_zip_uncomp *uncomp)
{
    int err;

    uncomp->state.bstream.bzalloc = gBzip2_Alloc;
    uncomp->state.bstream.bzfree = gBzip2_Free;
    uncomp->state.bstream.opaque = NULL;

    err = BZ2_bzDecompressInit(&uncomp->state.bstream, 0, 0);
    return err == BZ_OK;
}

static uint32_t zip_uncompress_data_bzip2(struct ar_archive_zip_uncomp *uncomp, void *buffer, uint32_t buffer_size)
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
        return ERR_UNCOMP;
    }
    if (err == BZ_STREAM_END && uncomp->input.bytes_left && uncomp->state.bstream.avail_out == buffer_size) {
        warn("Premature EOS in BZIP2 stream");
        return ERR_UNCOMP;
    }

    return buffer_size - uncomp->state.bstream.avail_out;
}

static void zip_clear_uncompress_bzip2(struct ar_archive_zip_uncomp *uncomp)
{
    BZ2_bzDecompressEnd(&uncomp->state.bstream);
}
#endif

/***** LZMA compression *****/

#ifdef HAVE_LZMA
static void *gLzma_Alloc(void *self, size_t size) { (void)self; return malloc(size); }
static void gLzma_Free(void *self, void *ptr) { (void)self; free(ptr); }

static bool zip_init_uncompress_lzma(struct ar_archive_zip_uncomp *uncomp, uint16_t flags)
{
    uncomp->state.lzma.alloc.Alloc = gLzma_Alloc;
    uncomp->state.lzma.alloc.Free = gLzma_Free;
    uncomp->state.lzma.finish = (flags & (1 << 1)) ? LZMA_FINISH_END : LZMA_FINISH_ANY;
    LzmaDec_Construct(&uncomp->state.lzma.dec);
    return true;
}

static uint32_t zip_uncompress_data_lzma(struct ar_archive_zip_uncomp *uncomp, void *buffer, uint32_t buffer_size)
{
    SizeT srclen, dstlen;
    ELzmaStatus status;
    SRes res = SZ_OK;

    if (!uncomp->state.lzma.dec.dic) {
        uint8_t propsize;
        if (uncomp->input.bytes_left < 9) {
            warn("Insufficient data in compressed stream");
            return ERR_UNCOMP;
        }
        propsize = uncomp->input.data[uncomp->input.offset + 2];
        if (uncomp->input.data[uncomp->input.offset + 3] != 0 || uncomp->input.bytes_left < 4 + propsize) {
            warn("Insufficient data in compressed stream");
            return ERR_UNCOMP;
        }
        res = LzmaDec_Allocate(&uncomp->state.lzma.dec, &uncomp->input.data[uncomp->input.offset + 4], propsize, &uncomp->state.lzma.alloc);
        uncomp->input.offset += 4 + propsize;
        uncomp->input.bytes_left -= 4 + propsize;
        if (res != SZ_OK)
            return ERR_UNCOMP;
        LzmaDec_Init(&uncomp->state.lzma.dec);
    }

    srclen = uncomp->input.bytes_left;
    dstlen = buffer_size;
    res = LzmaDec_DecodeToBuf(&uncomp->state.lzma.dec, buffer, &dstlen, &uncomp->input.data[uncomp->input.offset], &srclen, uncomp->state.lzma.finish, &status);

    uncomp->input.offset += (uint16_t)srclen;
    uncomp->input.bytes_left -= (uint16_t)srclen;

    if (res != SZ_OK || (srclen == 0 && dstlen == 0))
        return ERR_UNCOMP;
    if (status == LZMA_STATUS_FINISHED_WITH_MARK && uncomp->input.bytes_left) {
        warn("Premature EOS in LZMA stream");
        return ERR_UNCOMP;
    }

    return (uint32_t)dstlen;
}

static void zip_clear_uncompress_lzma(struct ar_archive_zip_uncomp *uncomp)
{
    LzmaDec_Free(&uncomp->state.lzma.dec, &uncomp->state.lzma.alloc);
}
#endif

/***** PPMd compression *****/

static void *gPpmd_Alloc(void *self, size_t size) { (void)self; return malloc(size); }
static void gPpmd_Free(void *self, void *ptr) { (void)self; free(ptr); }

static Byte gPpmd_ByteIn_Read(void *p)
{
    struct ByteReader *self = p;
    if (!self->input->bytes_left && (!self->zip->progress.data_left || !zip_fill_input_buffer(self->zip)))
        return 0;
    self->input->bytes_left--;
    return self->input->data[self->input->offset++];
}

static bool zip_init_uncompress_ppmd(ar_archive_zip *zip)
{
    struct ar_archive_zip_uncomp *uncomp = &zip->uncomp;
    uncomp->state.ppmd8.alloc.Alloc = gPpmd_Alloc;
    uncomp->state.ppmd8.alloc.Free = gPpmd_Free;
    uncomp->state.ppmd8.bytein.super.Read = gPpmd_ByteIn_Read;
    uncomp->state.ppmd8.bytein.input = &uncomp->input;
    uncomp->state.ppmd8.bytein.zip = zip;
    uncomp->state.ppmd8.ctx.Stream.In = &uncomp->state.ppmd8.bytein.super;
    Ppmd8_Construct(&uncomp->state.ppmd8.ctx);
    return true;
}

static uint32_t zip_uncompress_data_ppmd(struct ar_archive_zip_uncomp *uncomp, void *buffer, uint32_t buffer_size)
{
    uint32_t bytes_done = 0;

    if (!uncomp->state.ppmd8.ctx.Base) {
        uint8_t order, size, method;
        if (uncomp->input.bytes_left < 2) {
            warn("Insufficient data in compressed stream");
            return ERR_UNCOMP;
        }
        order = (uncomp->input.data[uncomp->input.offset] & 0x0F) + 1;
        size = (((uncomp->input.data[uncomp->input.offset] >> 4) | ((uncomp->input.data[uncomp->input.offset + 1] << 4) & 0xFF))) + 1;
        method = uncomp->input.data[uncomp->input.offset + 1] >> 4;
        uncomp->input.bytes_left -= 2;
        uncomp->input.offset += 2;
        if (order < 2 || method > 2) {
            warn("Invalid PPMd data stream");
            return ERR_UNCOMP;
        }
#ifndef PPMD8_FREEZE_SUPPORT
        if (order == 2) {
            warn("PPMd freeze method isn't supported");
            return ERR_UNCOMP;
        }
#endif
        if (!Ppmd8_Alloc(&uncomp->state.ppmd8.ctx, size << 20, &uncomp->state.ppmd8.alloc))
            return ERR_UNCOMP;
        if (!Ppmd8_RangeDec_Init(&uncomp->state.ppmd8.ctx))
            return ERR_UNCOMP;
        Ppmd8_Init(&uncomp->state.ppmd8.ctx, order, method);
    }

    while (bytes_done < buffer_size) {
        int symbol = Ppmd8_DecodeSymbol(&uncomp->state.ppmd8.ctx);
        if (symbol < 0) {
            warn("Invalid PPMd data stream");
            return ERR_UNCOMP;
        }
        ((uint8_t *)buffer)[bytes_done++] = (uint8_t)symbol;
    }

    /* TODO: simplify checking for last uncompressed block */
    if (uncomp->state.ppmd8.bytein.zip->progress.bytes_done + bytes_done == uncomp->state.ppmd8.bytein.zip->super.entry_size_uncompressed) {
        int symbol = Ppmd8_DecodeSymbol(&uncomp->state.ppmd8.ctx);
        if (symbol != -1 || !Ppmd8_RangeDec_IsFinishedOK(&uncomp->state.ppmd8.ctx)) {
            warn("Invalid PPMd data stream");
            return ERR_UNCOMP;
        }
    }

    return bytes_done;
}

static void zip_clear_uncompress_ppmd(struct ar_archive_zip_uncomp *uncomp)
{
    Ppmd8_Free(&uncomp->state.ppmd8.ctx, &uncomp->state.ppmd8.alloc);
}

/***** common decompression handling *****/

static bool zip_init_uncompress(ar_archive_zip *zip)
{
    struct ar_archive_zip_uncomp *uncomp = &zip->uncomp;
    if (uncomp->initialized)
        return true;
    memset(uncomp, 0, sizeof(*uncomp));
    if (zip->entry.method == METHOD_DEFLATE || zip->entry.method == METHOD_DEFLATE64) {
#ifdef HAVE_ZLIB
        if (zip_init_uncompress_deflate(uncomp, zip->entry.method == METHOD_DEFLATE64)) {
            uncomp->uncompress_data = zip_uncompress_data_deflate;
            uncomp->clear_state = zip_clear_uncompress_deflate;
        }
#else
        warn("Deflate support requires ZLIB (define HAVE_ZLIB)");
#endif
    }
    else if (zip->entry.method == METHOD_BZIP2) {
#ifdef HAVE_BZIP2
        if (zip_init_uncompress_bzip2(uncomp)) {
            uncomp->uncompress_data = zip_uncompress_data_bzip2;
            uncomp->clear_state = zip_clear_uncompress_bzip2;
        }
#else
        warn("BZIP2 support requires BZIP2 (define HAVE_BZIP2)");
#endif
    }
    else if (zip->entry.method == METHOD_LZMA) {
#ifdef HAVE_LZMA
        if (zip_init_uncompress_lzma(uncomp, zip->entry.flags)) {
            uncomp->uncompress_data = zip_uncompress_data_lzma;
            uncomp->clear_state = zip_clear_uncompress_lzma;
        }
#else
        warn("LZMA support requires LZMA SDK (define HAVE_LZMA)");
#endif
    }
    else if (zip->entry.method == METHOD_PPMD) {
        if (zip_init_uncompress_ppmd(zip)) {
            uncomp->uncompress_data = zip_uncompress_data_ppmd;
            uncomp->clear_state = zip_clear_uncompress_ppmd;
        }
    }
    else
        warn("Unsupported compression method %d", zip->entry.method);
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
    uint32_t count;

    if (!zip_init_uncompress(zip))
        return false;

    for (;;) {
        if (buffer_size == 0)
            return true;

        if (uncomp->input.bytes_left < sizeof(uncomp->input.data) / 2 && zip->progress.data_left) {
            if (!zip_fill_input_buffer(zip))
                return false;
        }

        count = buffer_size >= UINT32_MAX ? UINT32_MAX - 1 : (uint32_t)buffer_size;
        count = uncomp->uncompress_data(uncomp, buffer, count);
        if (count == ERR_UNCOMP)
            return false;
        if (count == 0 && !zip->progress.data_left) {
            warn("Insufficient data in compressed stream");
            return false;
        }
        zip->progress.bytes_done += count;
        buffer = (uint8_t *)buffer + count;
        buffer_size -= count;
    }
}
