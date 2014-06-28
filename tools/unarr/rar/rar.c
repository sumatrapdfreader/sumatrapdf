/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "rar.h"

static void rar_close(ar_archive *ar)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);

    free(ar_rar->entry.name);
    free(ar_rar->entry.name_w);
}

static bool rar_parse_entry(ar_archive *ar)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);

    if (ar->entry_offset != 0) {
        if (!ar_seek(ar->stream, ar->entry_offset + ar->entry_size_block, SEEK_SET)) {
            log("Couldn't seek to offset %Iu", ar->entry_offset + ar->entry_size_block);
            return false;
        }
    }

    for (;;) {
        struct rar_header header;
        struct rar_entry entry;
        if (!rar_parse_header(ar, &header))
            return false;

        switch (header.type) {
        case 0x73: /* archive header */
            if ((header.flags & MHD_PASSWORD)) {
                log("Encrypted archives aren't supported");
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
                log("Invalid RAR header size: %Iu", header.size);
                return false;
            }
            ar_rar->flags = header.flags;
            break;

        case 0x74: /* file header */
            if (!rar_parse_header_entry(ar, &header, &entry))
                return false;
            if ((header.flags & LHD_PASSWORD))
                log("Encrypted entries will fail to uncompress");
            // TODO: handle multi-part files (only needed for split files)?
#ifdef DEBUG
            // TODO: CRC checks don't always hold (claim in XADRARParser.m @readBlockHeader)
            if (!rar_check_header_crc(ar->stream, ar->entry_offset))
                log("Invalid header checksum @%Iu", ar->entry_offset);
#else
            if (!ar_seek(ar->stream, ar->entry_offset + header.size, SEEK_SET)) {
                log("Couldn't seek to offset %Iu", ar->entry_offset + header.size);
                return false;
            }
#endif
            return true;

        case 0x7A: /* newsub header */
            log("Skipping newsub header @%Iu", ar->entry_offset);
            break;

        case 0x7B: /* end of archive */
            ar->at_eof = true;
            return false;

        default:
            log("Unknown RAR header type %02x", header.type);
            break;
        }

#ifdef DEBUG
        // TODO: CRC checks don't always hold (claim in XADRARParser.m @readBlockHeader)
        if (!rar_check_header_crc(ar->stream, ar->entry_offset))
            log("Invalid header checksum @%Iu", ar->entry_offset);
#endif

        if (!ar_seek(ar->stream, ar->entry_offset + header.size + header.datasize, SEEK_SET)) {
            log("Couldn't seek to offset %Iu", ar->entry_offset + header.size + header.datasize);
            return false;
        }
    }
}

static const char *rar_get_name(ar_archive *ar)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);
    return ar_rar->entry.name;
}

static const WCHAR *rar_get_name_w(ar_archive *ar)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);
#ifdef _WIN32
    if (!ar_rar->entry.name_w && ar_rar->entry.name) {
        int res = MultiByteToWideChar(CP_UTF8, 0, ar_rar->entry.name, -1, NULL, 0);
        ar_rar->entry.name_w = malloc(res);
        MultiByteToWideChar(CP_UTF8, 0, ar_rar->entry.name, -1, ar_rar->entry.name_w, res);
    }
#endif
    return ar_rar->entry.name_w;
}

static bool rar_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    return rar_uncompress_part(ar, buffer, count);
}

ar_archive *ar_open_rar_archive(ar_stream *stream)
{
    char signature[7];
    if (ar_read(stream, signature, sizeof(signature)) != sizeof(signature))
        return NULL;
    if (memcmp(signature, "Rar!\x1A\x07\x00", 7) != 0) {
        if (memcmp(signature, "Rar!\x1A\x07\x01", 7) == 0)
            log("RAR 5 format isn't supported");
        else if (memcmp(signature, "RE~^", 4) == 0)
            log("Ancient RAR formatn isn't supported");
        // TODO: detect and skip SFX header?
        return NULL;
    }

    return ar_open_archive(stream, sizeof(struct rar_data), rar_close, rar_parse_entry, rar_get_name, rar_get_name_w, rar_uncompress);
}
