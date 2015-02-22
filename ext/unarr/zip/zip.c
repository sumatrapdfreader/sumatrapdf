/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "zip.h"

static void zip_close(ar_archive *ar)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    free(zip->entry.name);
    zip_clear_uncompress(&zip->uncomp);
}

static bool zip_parse_local_entry(ar_archive *ar, off64_t offset)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    struct zip_entry entry;

    offset = zip_find_next_local_file_entry(ar->stream, offset);
    if (offset < 0) {
        if (ar->entry_offset_next)
            ar->at_eof = true;
        else
            warn("Work around failed, no entries found in this file");
        return false;
    }
    if (!ar_seek(ar->stream, offset, SEEK_SET)) {
        warn("Couldn't seek to offset %" PRIi64, offset);
        return false;
    }
    if (!zip_parse_local_file_entry(zip, &entry))
        return false;

    ar->entry_offset = offset;
    ar->entry_offset_next = offset + ZIP_LOCAL_ENTRY_FIXED_SIZE + entry.namelen + entry.extralen + (off64_t)entry.datasize;
    if (ar->entry_offset_next <= ar->entry_offset) {
        warn("Compressed size is too large (%" PRIu64 ")", entry.datasize);
        return false;
    }
    ar->entry_size_uncompressed = (size_t)entry.uncompressed;
    ar->entry_filetime = ar_conv_dosdate_to_filetime(entry.dosdate);

    zip->entry.offset = offset;
    zip->entry.method = entry.method;
    zip->entry.flags = entry.flags;
    zip->entry.crc = entry.crc;
    free(zip->entry.name);
    zip->entry.name = NULL;
    zip->entry.dosdate = entry.dosdate;

    zip->progress.crc = 0;
    zip->progress.bytes_done = 0;
    zip->progress.data_left = (size_t)entry.datasize;
    zip_clear_uncompress(&zip->uncomp);

    if (entry.datasize == 0 && ar_entry_get_name(ar) && *zip->entry.name && zip->entry.name[strlen(zip->entry.name) - 1] == '/') {
        log("Skipping directory entry \"%s\"", zip->entry.name);
        return zip_parse_local_entry(ar, ar->entry_offset_next);
    }
    if (entry.datasize == 0 && entry.uncompressed == 0 && (entry.flags & (1 << 3))) {
        warn("Deferring sizes to data descriptor isn't supported");
        ar->entry_size_uncompressed = 1;
    }

    return true;
}

static bool zip_parse_entry(ar_archive *ar, off64_t offset)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    struct zip_entry entry;

    if (offset >= zip->dir.end_offset) {
        ar->at_eof = true;
        return false;
    }
    if (!ar_seek(ar->stream, offset, SEEK_SET)) {
        warn("Couldn't seek to offset %" PRIi64, offset);
        return false;
    }
    if (!zip_parse_directory_entry(zip, &entry)) {
        warn("Couldn't read directory entry @%" PRIi64, offset);
        return false;
    }

    ar->entry_offset = offset;
    ar->entry_offset_next = offset + ZIP_DIR_ENTRY_FIXED_SIZE + entry.namelen + entry.extralen + entry.commentlen;
    ar->entry_size_uncompressed = (size_t)entry.uncompressed;
    ar->entry_filetime = ar_conv_dosdate_to_filetime(entry.dosdate);

    zip->entry.offset = entry.header_offset;
    zip->entry.method = entry.method;
    zip->entry.flags = entry.flags;
    zip->entry.crc = entry.crc;
    free(zip->entry.name);
    zip->entry.name = NULL;
    zip->entry.dosdate = entry.dosdate;

    zip->progress.crc = 0;
    zip->progress.bytes_done = 0;
    zip->progress.data_left = (size_t)entry.datasize;
    zip_clear_uncompress(&zip->uncomp);

    if (entry.datasize == 0 && ((entry.version >> 8) == 0 || (entry.version >> 8) == 3) && (entry.attr_external & 0x40000010)) {
        log("Skipping directory entry \"%s\"", zip_get_name(ar));
        return zip_parse_entry(ar, ar->entry_offset_next);
    }

    return true;
}

static bool zip_copy_stored(ar_archive_zip *zip, void *buffer, size_t count)
{
    if (count > zip->progress.data_left) {
        warn("Unexpected EOS in stored data");
        return false;
    }
    if (ar_read(zip->super.stream, buffer, count) != count) {
        warn("Unexpected EOF in stored data");
        return false;
    }
    zip->progress.data_left -= count;
    zip->progress.bytes_done += count;
    return true;
}

static bool zip_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    if (zip->progress.bytes_done == 0) {
        if ((zip->entry.flags & ((1 << 0) | (1 << 6)))) {
            warn("Encrypted archives aren't supported");
            return false;
        }
        if (!zip_seek_to_compressed_data(zip)) {
            warn("Couldn't find data for file");
            return false;
        }
    }
    if (count > ar->entry_size_uncompressed - zip->progress.bytes_done) {
        warn("Requesting too much data (%" PRIuPTR " < %" PRIuPTR ")", ar->entry_size_uncompressed - zip->progress.bytes_done, count);
        return false;
    }
    if (zip->entry.method == METHOD_STORE) {
        if (!zip_copy_stored(zip, buffer, count))
            return false;
    }
    else if (zip->deflatedonly && zip->entry.method != METHOD_DEFLATE) {
        warn("Only store and deflate compression methods are allowed");
        return false;
    }
    else {
        if (!zip_uncompress_part(zip, buffer, count))
            return false;
    }

    zip->progress.crc = ar_crc32(zip->progress.crc, buffer, count);
    if (zip->progress.bytes_done < ar->entry_size_uncompressed)
        return true;
    if (zip->uncomp.initialized ? !zip->uncomp.input.at_eof || zip->uncomp.input.bytes_left : zip->progress.data_left)
        log("Compressed block has more data than required");
    if (zip->progress.crc != zip->entry.crc) {
        warn("Checksum of extracted data doesn't match");
        return false;
    }
    return true;
}

size_t zip_get_global_comment(ar_archive *ar, void *buffer, size_t count)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    if (!zip->comment_size)
        return 0;
    if (!buffer)
        return zip->comment_size;
    if (!ar_seek(ar->stream, zip->comment_offset, SEEK_SET))
        return 0;
    if (count > zip->comment_size)
        count = zip->comment_size;
    return ar_read(ar->stream, buffer, count);
}

ar_archive *ar_open_zip_archive(ar_stream *stream, bool deflatedonly)
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

    ar = ar_open_archive(stream, sizeof(ar_archive_zip), zip_close, zip_parse_entry, zip_get_name, zip_uncompress, zip_get_global_comment, eocd.dir_offset);
    if (!ar)
        return NULL;

    zip = (ar_archive_zip *)ar;
    zip->dir.end_offset = zip_find_end_of_last_directory_entry(stream, &eocd);
    if (zip->dir.end_offset < 0) {
        warn("Couldn't read central directory @%" PRIi64 ", trying to work around...", eocd.dir_offset);
        ar->parse_entry = zip_parse_local_entry;
        ar->entry_offset_first = ar->entry_offset_next = 0;
    }
    zip->deflatedonly = deflatedonly;
    zip->comment_offset = offset + ZIP_END_OF_CENTRAL_DIR_SIZE;
    zip->comment_size = eocd.commentlen;

    return ar;
}
