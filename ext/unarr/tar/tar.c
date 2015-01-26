/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "tar.h"

static void tar_close(ar_archive *ar)
{
    ar_archive_tar *tar = (ar_archive_tar *)ar;
    free(tar->entry.name);
}

static bool tar_parse_entry(ar_archive *ar, off64_t offset)
{
    ar_archive_tar *tar = (ar_archive_tar *)ar;

    if (!ar_seek(ar->stream, offset, SEEK_SET)) {
        warn("Couldn't seek to offset %" PRIi64, offset);
        return false;
    }
    if (!tar_parse_header(tar)) {
        warn("Invalid tar header data @%" PRIi64, offset);
        return false;
    }
    if (!tar->entry.checksum) {
        ar->at_eof = true;
        return false;
    }

    ar->entry_offset = offset;
    ar->entry_offset_next = offset + TAR_BLOCK_SIZE + (tar->entry.filesize + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE * TAR_BLOCK_SIZE;
    ar->entry_size_uncompressed = tar->entry.filesize;
    ar->entry_filetime = tar->entry.mtime;
    tar->bytes_done = 0;

    if (tar->last_seen_dir > offset)
        tar->last_seen_dir = 0;

    switch (tar->entry.filetype) {
    case TYPE_FILE:
    case TYPE_FILE_OLD:
        return true;
    case TYPE_DIRECTORY:
        log("Skipping directory entry \"%s\"", tar_get_name(ar));
        tar->last_seen_dir = ar->entry_offset;
        return tar_parse_entry(ar, ar->entry_offset_next);
    case TYPE_PAX_GLOBAL:
        log("Skipping PAX global extended header record");
        return tar_parse_entry(ar, ar->entry_offset_next);
    case TYPE_PAX_EXTENDED:
        return tar_handle_pax_extended(ar);
    case TYPE_GNU_LONGNAME:
        return tar_handle_gnu_longname(ar);
    default:
        warn("Unknown entry type '%c'", tar->entry.filetype);
        return true;
    }
}

static bool tar_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    ar_archive_tar *tar = (ar_archive_tar *)ar;
    if (count > ar->entry_size_uncompressed - tar->bytes_done) {
        warn("Requesting too much data (%" PRIuPTR " < %" PRIuPTR ")", ar->entry_size_uncompressed - tar->bytes_done, count);
        return false;
    }
    if (ar_read(ar->stream, buffer, count) != count) {
        warn("Unexpected EOF in stored data");
        return false;
    }
    tar->bytes_done += count;
    return true;
}

ar_archive *ar_open_tar_archive(ar_stream *stream)
{
    ar_archive *ar;
    ar_archive_tar *tar;

    if (!ar_seek(stream, 0, SEEK_SET))
        return NULL;

    ar = ar_open_archive(stream, sizeof(ar_archive_tar), tar_close, tar_parse_entry, tar_get_name, tar_uncompress, NULL, 0);
    if (!ar)
        return NULL;

    tar = (ar_archive_tar *)ar;
    if (!tar_parse_header(tar) || !tar->entry.checksum) {
        free(ar);
        return NULL;
    }

    return ar;
}
