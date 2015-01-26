/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "rar.h"

static void rar_close(ar_archive *ar)
{
    ar_archive_rar *rar = (ar_archive_rar *)ar;
    free(rar->entry.name);
    rar_clear_uncompress(&rar->uncomp);
}

static bool rar_parse_entry(ar_archive *ar, off64_t offset)
{
    ar_archive_rar *rar = (ar_archive_rar *)ar;
    struct rar_header header;
    struct rar_entry entry;
    bool out_of_order = offset != ar->entry_offset_next;

    if (!ar_seek(ar->stream, offset, SEEK_SET)) {
        warn("Couldn't seek to offset %" PRIi64, offset);
        return false;
    }

    for (;;) {
        ar->entry_offset = ar_tell(ar->stream);
        ar->entry_size_uncompressed = 0;

        if (!rar_parse_header(ar, &header))
            return false;

        ar->entry_offset_next = ar->entry_offset + header.size + header.datasize;
        if (ar->entry_offset_next < ar->entry_offset + header.size) {
            warn("Integer overflow due to overly large data size");
            return false;
        }

        switch (header.type) {
        case TYPE_MAIN_HEADER:
            if ((header.flags & MHD_PASSWORD)) {
                warn("Encrypted archives aren't supported");
                return false;
            }
            ar_skip(ar->stream, 6 /* reserved data */);
            if ((header.flags & MHD_ENCRYPTVER)) {
                log("MHD_ENCRYPTVER is set");
                ar_skip(ar->stream, 1);
            }
            if ((header.flags & MHD_COMMENT))
                log("MHD_COMMENT is set");
            if (ar_tell(ar->stream) - ar->entry_offset > header.size) {
                warn("Invalid RAR header size: %d", header.size);
                return false;
            }
            rar->archive_flags = header.flags;
            break;

        case TYPE_FILE_ENTRY:
            if (!rar_parse_header_entry(rar, &header, &entry))
                return false;
            if ((header.flags & LHD_PASSWORD))
                warn("Encrypted entries will fail to uncompress");
            if ((header.flags & LHD_DIRECTORY) == LHD_DIRECTORY) {
                if (header.datasize == 0) {
                    log("Skipping directory entry \"%s\"", rar_get_name(ar));
                    break;
                }
                warn("Can't skip directory entries containing data");
            }
            if ((header.flags & (LHD_SPLIT_BEFORE | LHD_SPLIT_AFTER)))
                warn("Splitting files isn't really supported");
            ar->entry_size_uncompressed = (size_t)entry.size;
            ar->entry_filetime = ar_conv_dosdate_to_filetime(entry.dosdate);
            if (!rar->entry.solid || rar->entry.method == METHOD_STORE || out_of_order) {
                rar_clear_uncompress(&rar->uncomp);
                memset(&rar->solid, 0, sizeof(rar->solid));
            }
            else {
                br_clear_leftover_bits(&rar->uncomp);
            }

            rar->solid.restart = rar->entry.solid && (out_of_order || !rar->solid.part_done);
            rar->solid.part_done = !ar->entry_size_uncompressed;
            rar->progress.data_left = (size_t)header.datasize;
            rar->progress.bytes_done = 0;
            rar->progress.crc = 0;

            /* TODO: CRC checks don't always hold (claim in XADRARParser.m @readBlockHeader) */
            if (!rar_check_header_crc(ar))
                warn("Invalid header checksum @%" PRIi64, ar->entry_offset);
            if (ar_tell(ar->stream) != ar->entry_offset + rar->entry.header_size) {
                warn("Couldn't seek to offset %" PRIi64, ar->entry_offset + rar->entry.header_size);
                return false;
            }
            return true;

        case TYPE_NEWSUB:
            log("Skipping newsub header @%" PRIi64, ar->entry_offset);
            break;

        case TYPE_END_OF_ARCHIVE:
            ar->at_eof = true;
            return false;

        default:
            log("Unknown RAR header type %02x", header.type);
            break;
        }

        /* TODO: CRC checks don't always hold (claim in XADRARParser.m @readBlockHeader) */
        if (!rar_check_header_crc(ar))
            warn("Invalid header checksum @%" PRIi64, ar->entry_offset);
        if (!ar_seek(ar->stream, ar->entry_offset_next, SEEK_SET)) {
            warn("Couldn't seek to offset %" PRIi64, ar->entry_offset_next);
            return false;
        }
    }
}

static bool rar_copy_stored(ar_archive_rar *rar, void *buffer, size_t count)
{
    if (count > rar->progress.data_left) {
        warn("Unexpected EOS in stored data");
        return false;
    }
    if (ar_read(rar->super.stream, buffer, count) != count) {
        warn("Unexpected EOF in stored data");
        return false;
    }
    rar->progress.data_left -= count;
    rar->progress.bytes_done += count;
    return true;
}

static bool rar_restart_solid(ar_archive *ar)
{
    ar_archive_rar *rar = (ar_archive_rar *)ar;
    off64_t current_offset = ar->entry_offset;
    log("Restarting decompression for solid entry");
    if (!ar_parse_entry_at(ar, ar->entry_offset_first)) {
        ar_parse_entry_at(ar, current_offset);
        return false;
    }
    while (ar->entry_offset < current_offset) {
        size_t size = ar->entry_size_uncompressed;
        rar->solid.restart = false;
        while (size > 0) {
            unsigned char buffer[1024];
            size_t count = smin(size, sizeof(buffer));
            if (!ar_entry_uncompress(ar, buffer, count)) {
                ar_parse_entry_at(ar, current_offset);
                return false;
            }
            size -= count;
        }
        if (!ar_parse_entry(ar)) {
            ar_parse_entry_at(ar, current_offset);
            return false;
        }
    }
    rar->solid.restart = false;
    return true;
}

static bool rar_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    ar_archive_rar *rar = (ar_archive_rar *)ar;
    if (count > ar->entry_size_uncompressed - rar->progress.bytes_done) {
        warn("Requesting too much data (%" PRIuPTR " < %" PRIuPTR ")", ar->entry_size_uncompressed - rar->progress.bytes_done, count);
        return false;
    }
    if (rar->entry.method == METHOD_STORE) {
        if (!rar_copy_stored(rar, buffer, count))
            return false;
    }
    else if (rar->entry.method == METHOD_FASTEST || rar->entry.method == METHOD_FAST ||
             rar->entry.method == METHOD_NORMAL || rar->entry.method == METHOD_GOOD ||
             rar->entry.method == METHOD_BEST) {
        if (rar->solid.restart && !rar_restart_solid(ar)) {
            warn("Failed to produce the required solid decompression state");
            return false;
        }
        if (!rar_uncompress_part(rar, buffer, count))
            return false;
    }
    else {
        warn("Unknown compression method %#02x", rar->entry.method);
        return false;
    }

    rar->progress.crc = ar_crc32(rar->progress.crc, buffer, count);
    if (rar->progress.bytes_done < ar->entry_size_uncompressed)
        return true;
    if (rar->progress.data_left)
        log("Compressed block has more data than required");
    rar->solid.part_done = true;
    rar->solid.size_total += rar->progress.bytes_done;
    if (rar->progress.crc != rar->entry.crc) {
        warn("Checksum of extracted data doesn't match");
        return false;
    }
    return true;
}

ar_archive *ar_open_rar_archive(ar_stream *stream)
{
    char signature[FILE_SIGNATURE_SIZE];
    if (!ar_seek(stream, 0, SEEK_SET))
        return NULL;
    if (ar_read(stream, signature, sizeof(signature)) != sizeof(signature))
        return NULL;
    if (memcmp(signature, "Rar!\x1A\x07\x00", sizeof(signature)) != 0) {
        if (memcmp(signature, "Rar!\x1A\x07\x01", sizeof(signature)) == 0)
            warn("RAR 5 format isn't supported");
        else if (memcmp(signature, "RE~^", 4) == 0)
            warn("Ancient RAR format isn't supported");
        else if (memcmp(signature, "MZ", 2) == 0 || memcmp(signature, "\x7F\x45LF", 4) == 0)
            warn("SFX archives aren't supported");
        return NULL;
    }

    return ar_open_archive(stream, sizeof(ar_archive_rar), rar_close, rar_parse_entry, rar_get_name, rar_uncompress, NULL, FILE_SIGNATURE_SIZE);
}
