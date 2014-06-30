/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "rar.h"

static void rar_close(ar_archive_rar *rar)
{
    free(rar->entry.name);
    free(rar->entry.name_w);
    rar_clear_uncompress(&rar->uncomp);
}

static bool rar_parse_entry(ar_archive_rar *rar)
{
    struct rar_header header;
    struct rar_entry entry;

    if (rar->super.entry_offset != 0) {
        if (!ar_seek(rar->super.stream, rar->super.entry_offset + rar->super.entry_size_block, SEEK_SET)) {
            warn("Couldn't seek to offset %" PRIuPTR, rar->super.entry_offset + rar->super.entry_size_block);
            return false;
        }
    }

    for (;;) {
        rar->super.entry_offset = ar_tell(rar->super.stream);
        rar->super.entry_size_block = 0;
        rar->super.entry_size_uncompressed = 0;

        if (!rar_parse_header(&rar->super, &header))
            return false;

        switch (header.type) {
        case TYPE_MAIN_HEADER:
            if ((header.flags & MHD_PASSWORD)) {
                todo("Encrypted archives aren't supported");
                return false;
            }
            ar_skip(rar->super.stream, 6 /* reserved data */);
            if ((header.flags & MHD_ENCRYPTVER)) {
                log("MHD_ENCRYPTVER is set");
                ar_skip(rar->super.stream, 1);
            }
            if ((header.flags & MHD_COMMENT))
                log("MHD_COMMENT is set");
            if (ar_tell(rar->super.stream) - rar->super.entry_offset > header.size) {
                warn("Invalid RAR header size: %" PRIuPTR, header.size);
                return false;
            }
            rar->archive_flags = header.flags;
            break;

        case TYPE_FILE_ENTRY:
            if (!rar_parse_header_entry(rar, &header, &entry))
                return false;
            if ((header.flags & LHD_PASSWORD))
                todo("Encrypted entries will fail to uncompress");
            if ((header.flags & LHD_DIRECTORY) == LHD_DIRECTORY) {
                log("Skipping directory entry \"%s\"", rar_get_name(rar));
                break;
            }
            if ((header.flags & (LHD_SPLIT_BEFORE | LHD_SPLIT_AFTER))) {
                todo("Splitting files isn't supported");
                break;
            }
            // TODO: handle multi-part files (only needed for split files)?
            rar->super.entry_size_block = header.size + (size_t)header.datasize;
            rar->super.entry_size_uncompressed = (size_t)entry.size;
            if (rar->super.entry_size_block < rar->entry.header_size) {
                warn("Integer overflow due to overly large data size");
                return false;
            }
#ifdef DEBUG
            // TODO: CRC checks don't always hold (claim in XADRARParser.m @readBlockHeader)
            if (!rar_check_header_crc(&rar->super))
                warn("Invalid header checksum @%" PRIuPTR, rar->super.entry_offset);
#endif
            if (!ar_seek(rar->super.stream, rar->super.entry_offset + rar->entry.header_size, SEEK_SET)) {
                warn("Couldn't seek to offset %" PRIuPTR, rar->super.entry_offset + rar->entry.header_size);
                return false;
            }
            return true;

        case TYPE_NEWSUB:
            log("Skipping newsub header @%" PRIuPTR, rar->super.entry_offset);
            break;

        case TYPE_END_OF_ARCHIVE:
            rar->super.at_eof = true;
            return false;

        default:
            log("Unknown RAR header type %02x", header.type);
            break;
        }

#ifdef DEBUG
        // TODO: CRC checks don't always hold (claim in XADRARParser.m @readBlockHeader)
        if (!rar_check_header_crc(&rar->super))
            warn("Invalid header checksum @%" PRIuPTR, rar->super.entry_offset);
#endif

        if (!ar_seek(rar->super.stream, rar->super.entry_offset + header.size + (ptrdiff_t)header.datasize, SEEK_SET)) {
            warn("Couldn't seek to offset %" PRIuPTR, rar->super.entry_offset + header.size + header.datasize);
            return false;
        }
        if (ar_tell(rar->super.stream) <= rar->super.entry_offset) {
            warn("Integer overflow due to overly large data size");
            return false;
        }
    }
}

static bool rar_copy_stored(ar_archive_rar *rar, void *buffer, size_t count)
{
    if (count > rar->super.entry_size_block - rar->progr.offset_in) {
        warn("Requesting too much data (%" PRIuPTR " < %" PRIuPTR ")", rar->super.entry_size_block - rar->progr.offset_in, count);
        return false;
    }
    if (ar_read(rar->super.stream, buffer, count) != count) {
        warn("Unexpected EOF in stored data");
        return false;
    }
    rar->progr.offset_in += count;
    rar->progr.offset_out += count;
    return true;
}

static bool rar_uncompress(ar_archive_rar *rar, void *buffer, size_t count)
{
    if (rar->entry.method == METHOD_STORE) {
        if (!rar_copy_stored(rar, buffer, count))
            return false;
    }
    else if (rar->entry.method == METHOD_FASTEST || rar->entry.method == METHOD_FAST ||
             rar->entry.method == METHOD_NORMAL || rar->entry.method == METHOD_GOOD ||
             rar->entry.method == METHOD_BEST) {
        if (!rar_uncompress_part(rar, buffer, count))
            return false;
    }
    else {
        warn("Unknown compression method %02x", rar->entry.method);
        return false;
    }

    rar->progr.crc = crc32(rar->progr.crc, buffer, count);
    if (rar->progr.offset_in <= rar->super.entry_size_block && rar->progr.offset_out < rar->super.entry_size_uncompressed)
        return true;
    if (rar->progr.offset_in < rar->super.entry_size_block)
        warn("Uncompressed block has more data than required");
    if (rar->progr.crc != rar->entry.crc) {
        warn("Checksum of extracted data doesn't match");
        return false;
    }
    return true;
}

ar_archive *ar_open_rar_archive(ar_stream *stream)
{
    char signature[7];
    if (ar_read(stream, signature, sizeof(signature)) != sizeof(signature))
        return NULL;
    if (memcmp(signature, "Rar!\x1A\x07\x00", 7) != 0) {
        if (memcmp(signature, "Rar!\x1A\x07\x01", 7) == 0)
            todo("RAR 5 format isn't supported");
        else if (memcmp(signature, "RE~^", 4) == 0)
            todo("Ancient RAR format isn't supported");
        else if (memcmp(signature, "MZ", 2) == 0 || memcmp(signature, "\x7F\x45LF", 4) == 0)
            todo("SFX archives aren't supported");
        return NULL;
    }

    return ar_open_archive(stream, sizeof(ar_archive_rar), rar_close, rar_parse_entry, rar_get_name, rar_get_name_w, rar_uncompress);
}
