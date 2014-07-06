/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "zip.h"

static void zip_close(ar_archive *ar)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    free(zip->entry.name);
    free(zip->entry.name_w);
    zip_clear_uncompress(&zip->uncomp);
}

static bool zip_parse_entry(ar_archive *ar)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    struct zip_entry entry;

    if (zip->dir.seen_count == zip->dir.length && zip->dir.seen_last_offset == ar->entry_offset) {
        ar->at_eof = true;
        return false;
    }
    if (!ar_seek(ar->stream, ar->entry_offset_next, SEEK_SET)) {
        warn("Couldn't seek to offset %" PRIi64, ar->entry_offset_next);
        return false;
    }
    if (!zip_parse_directory_entry(zip, &entry)) {
        warn("Couldn't read directory entry number %" PRIu64, zip->dir.seen_count + 1);
        return false;
    }

    ar->entry_offset = ar->entry_offset_next;
    ar->entry_offset_next += ZIP_DIR_ENTRY_FIXED_SIZE + entry.namelen + entry.extralen + entry.commentlen;
    ar->entry_size_uncompressed = (size_t)entry.uncompressed;
    ar->entry_dosdate = entry.dosdate;

    if (ar->entry_offset > zip->dir.seen_last_offset) {
        zip->dir.seen_last_offset = ar->entry_offset;
        zip->dir.seen_count++;
    }

    if ((entry.flags & ((1 << 0) | (1 << 6))))
        warn("Encrypted archives aren't supported");
    zip->entry.offset = entry.header_offset;
    zip->entry.crc = entry.crc;
    free(zip->entry.name);
    zip->entry.name = NULL;
    free(zip->entry.name_w);
    zip->entry.name_w = NULL;
    zip->progr.crc = 0;
    zip->progr.bytes_done = 0;
    zip->progr.data_left = (size_t)entry.datasize;

    if (ar->entry_size_uncompressed == 0 && (entry.version & 0xFF00) == 0 && (entry.attr_external & 0x10)) {
        log("Skipping directory entry \"%s\"", zip_get_name(ar));
        return zip_parse_entry(ar);
    }

    return true;
}

static bool zip_copy_stored(ar_archive_zip *zip, void *buffer, size_t count)
{
    if (count > zip->progr.data_left) {
        warn("Requesting too much data (%" PRIuPTR " < %" PRIuPTR ")", zip->progr.data_left, count);
        return false;
    }
    if (ar_read(zip->super.stream, buffer, count) != count) {
        warn("Unexpected EOF in stored data");
        return false;
    }
    zip->progr.data_left -= count;
    zip->progr.bytes_done += count;
    return true;
}

static bool zip_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    bool ok = false;
    if (zip->progr.bytes_done == 0) {
        if (!zip_seek_to_compressed_data(zip)) {
            warn("Couldn't find data for file %s", ar_entry_get_name(ar));
            return false;
        }
    }
    if (zip->entry.method == METHOD_STORE)
        ok = zip_copy_stored(zip, buffer, count);
    else if (zip->entry.method == METHOD_DEFLATE)
        ok = zip_uncompress_deflate(zip, buffer, count);
    else if (zip->deflateonly)
        warn("Only store and deflate compression methods are allowed");
    else if (zip->entry.method == METHOD_DEFLATE64)
        ok = zip_uncompress_deflate64(zip, buffer, count);
    else if (zip->entry.method == METHOD_BZIP2)
        ok = zip_uncompress_bzip2(zip, buffer, count);
    else if (zip->entry.method == METHOD_LZMA)
        ok = zip_uncompress_lzma(zip, buffer, count);
    else
        warn("Unsupported compression method %02x", zip->entry.method);
    if (!ok)
        return false;

    zip->progr.crc = ar_crc32(zip->progr.crc, buffer, count);
    if (zip->progr.bytes_done < ar->entry_size_uncompressed)
        return true;
    if (zip->progr.data_left)
        log("Compressed block has more data than required");
    if (zip->progr.crc != zip->entry.crc) {
        warn("Checksum of extracted data doesn't match");
        return false;
    }
    return true;
}

ar_archive *ar_open_zip_archive(ar_stream *stream, bool deflateonly)
{
    ar_archive *ar;
    ar_archive_zip *zip;
    struct zip_eocd64 eocd = { 0 };

    off64_t offset = zip_find_end_of_central_directory(stream);
    if (offset < 0)
        return NULL;
    if (!ar_seek(stream, offset, SEEK_SET))
        return NULL;
    if (!zip_parse_end_of_central_directory(stream, &eocd))
        return NULL;

    ar = ar_open_archive(stream, sizeof(ar_archive_zip), zip_close, zip_parse_entry, zip_get_name, zip_get_name_w, zip_uncompress);
    if (!ar)
        return NULL;

    zip = (ar_archive_zip *)ar;
    zip->dir.offset = eocd.dir_offset;
    zip->dir.length = eocd.numentries;
    zip->deflateonly = deflateonly;
    zip->comment_offset = offset + 22;
    zip->comment_size = eocd.commentlen;
    ar->entry_offset_next = zip->dir.offset;

    return ar;
}
