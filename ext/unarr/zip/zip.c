/* Copyright 2014 the unarr project authors (see AUTHORS file).
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
        ar->at_eof = true;
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

    if (entry.datasize == 0 && zip_get_name(ar) && zip->entry.name[strlen(zip->entry.name) - 1] == '/') {
        log("Skipping directory entry \"%s\"", zip->entry.name);
        return zip_parse_local_entry(ar, ar->entry_offset_next);
    }

    return true;
}

static bool zip_parse_entry(ar_archive *ar, off64_t offset)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    struct zip_entry entry;

    if (zip->dir.seen_count == zip->dir.length && offset >= zip->dir.seen_last_end_offset) {
        ar->at_eof = true;
        return false;
    }
    if (!ar_seek(ar->stream, offset, SEEK_SET)) {
        warn("Couldn't seek to offset %" PRIi64, offset);
        return false;
    }
    if (!zip_parse_directory_entry(zip, &entry)) {
        if (zip->dir.seen_count == 0) {
            warn("Couldn't read first directory entry @%" PRIi64 ", trying to work around...", offset);
            ar->parse_entry = zip_parse_local_entry;
            ar->entry_offset_first = 0;
            return zip_parse_local_entry(ar, 0);
        }
        warn("Couldn't read directory entry number %" PRIu64 " @%" PRIi64, zip->dir.seen_count + 1, offset);
        return false;
    }

    ar->entry_offset = offset;
    ar->entry_offset_next = offset + ZIP_DIR_ENTRY_FIXED_SIZE + entry.namelen + entry.extralen + entry.commentlen;
    ar->entry_size_uncompressed = (size_t)entry.uncompressed;
    ar->entry_filetime = ar_conv_dosdate_to_filetime(entry.dosdate);

    if (ar->entry_offset_next > zip->dir.seen_last_end_offset) {
        zip->dir.seen_last_end_offset = ar->entry_offset_next;
        zip->dir.seen_count++;
    }

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

    if (entry.datasize == 0 && (entry.version & 0xFF00) == 0 && (entry.attr_external & 0x10)) {
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
            warn("Couldn't find data for file %s", ar_entry_get_name(ar));
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
    zip->dir.length = eocd.numentries;
    zip->deflatedonly = deflatedonly;
    zip->comment_offset = offset + 22;
    zip->comment_size = eocd.commentlen;

    return ar;
}
