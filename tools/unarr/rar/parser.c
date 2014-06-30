/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "rar.h"

inline uint8_t uint8le(unsigned char *data) { return data[0]; }
inline uint16_t uint16le(unsigned char *data) { return data[0] | data[1] << 8; }
inline uint32_t uint32le(unsigned char *data) { return data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24; }

bool rar_parse_header(ar_archive *ar, struct rar_header *header)
{
    unsigned char header_data[7];
    size_t read = ar_read(ar->stream, header_data, sizeof(header_data));
    if (read == 0) {
        ar->at_eof = true;
        return false;
    }
    if (read < sizeof(header_data))
        return false;

    header->crc = uint16le(header_data + 0);
    header->type = uint8le(header_data + 2);
    header->flags = uint16le(header_data + 3);
    header->size = uint16le(header_data + 5);

    header->datasize = 0;
    if ((header->flags & LHD_LONG_BLOCK) || header->type == 0x74) {
        unsigned char size_data[4];
        if (!(header->flags & LHD_LONG_BLOCK))
            log("File header without LHD_LONG_BLOCK set");
        read += ar_read(ar->stream, size_data, sizeof(size_data));
        if (read < sizeof(header_data) + sizeof(size_data))
            return false;
        header->datasize = uint32le(size_data);
    }

    if (header->size < read) {
        warn("Invalid header size %" PRIuPTR, header->size);
        return false;
    }

    return true;
}

bool rar_check_header_crc(ar_archive *ar)
{
    unsigned char buffer[256];
    uint16_t crc16, size;
    uint32_t crc;

    if (!ar_seek(ar->stream, ar->entry_offset, SEEK_SET))
        return false;
    if (ar_read(ar->stream, buffer, 7) != 7)
        return false;

    crc16 = uint16le(buffer + 0);
    size = uint16le(buffer + 5);
    if (size < 7)
        return false;
    size -= 7;

    crc = crc32(0, buffer + 2, 5);
    while (size > 0) {
        if (ar_read(ar->stream, buffer, min(size, sizeof(buffer))) != min(size, sizeof(buffer)))
            return false;
        crc = crc32(crc, buffer, min(size, sizeof(buffer)));
        size -= min(size, sizeof(buffer));
    }
    return (crc & 0xFFFF) == crc16;
}

bool rar_parse_header_entry(ar_archive_rar *rar, struct rar_header *header, struct rar_entry *entry)
{
    unsigned char data[21];
    if (ar_read(rar->super.stream, data, sizeof(data)) != sizeof(data))
        return false;

    entry->size = uint32le(data + 0);
    entry->os = uint8le(data + 4);
    entry->crc = uint32le(data + 5);
    entry->dostime = uint32le(data + 9);
    entry->version = uint8le(data + 13);
    entry->method = uint8le(data + 14);
    entry->namelen = uint16le(data + 15);
    entry->attrs = uint32le(data + 17);
    if ((header->flags & LHD_LARGE)) {
        unsigned char more_data[8];
        if (ar_read(rar->super.stream, more_data, sizeof(more_data)) != sizeof(more_data))
            return false;
        header->datasize += (uint64_t)uint32le(more_data + 0);
        entry->size += (uint64_t)uint32le(more_data + 4);
    }
    if (!ar_skip(rar->super.stream, entry->namelen))
        return false;
    if ((header->flags & LHD_SALT)) {
        log("Skipping LHD_SALT");
        ar_skip(rar->super.stream, 8);
    }
    if ((header->flags & LHD_SOLID))
        todo("LHD_SOLID only supported for METHOD_STORE");
    if (entry->method != METHOD_STORE && entry->version != 29) {
        todo("Unsupported compression version: %d", entry->version);
        return false;
    }
    entry->solid = entry->version < 20 ? (rar->archive_flags & MHD_SOLID) : (header->flags & LHD_SOLID);

    rar->entry.method = entry->method;
    rar->entry.crc = entry->crc;
    rar->entry.header_size = header->size;
    free(rar->entry.name);
    rar->entry.name = NULL;
    free(rar->entry.name_w);
    rar->entry.name_w = NULL;

    if (!entry->solid)
        rar_clear_uncompress(&rar->uncomp);
    else
        todo("This only works if all previous entries of this solid block are uncompressed first");
    rar->progr.offset_in = header->size;
    rar->progr.offset_out = 0;
    rar->progr.crc = 0;

    return true;
}

const char *rar_get_name(ar_archive_rar *rar)
{
    if (!rar->entry.name) {
        unsigned char data[21];
        uint16_t namelen;
        char *name;

        struct rar_header header;
        if (!ar_seek(rar->super.stream, rar->super.entry_offset, SEEK_SET))
            return NULL;
        if (!rar_parse_header(&rar->super, &header))
            return NULL;
        if (ar_read(rar->super.stream, data, sizeof(data)) != sizeof(data))
            return NULL;
        if ((header.flags & LHD_LARGE) && !ar_skip(rar->super.stream, 8))
            return NULL;

        namelen = uint16le(data + 15);
        name = malloc(namelen + 2);
        if (!name || ar_read(rar->super.stream, name, namelen) != namelen) {
            warn("Out of resources in rar_get_name");
            free(name);
            return NULL;
        }
        name[namelen] = name[namelen + 1] = '\0';

        if (!(header.flags & LHD_UNICODE)) {
            rar->entry.name = conv_ansi_to_utf8_utf16(name, &rar->entry.name_w);
            free(name);
        }
        else if (namelen == strlen(name)) {
            rar->entry.name = name;
            rar->entry.name_w = NULL;
        }
        else {
            rar->entry.name = conv_utf16_to_utf8((const WCHAR *)name);
            rar->entry.name_w = (WCHAR *)name;
        }
        /* normalize path separators */
        if (rar->entry.name) {
            char *p = rar->entry.name;
            while ((p = strchr(p, '\\')) != NULL) {
                *p = '/';
            }
        }
        if (rar->entry.name_w) {
            WCHAR *pw;
            for (pw = rar->entry.name_w; *pw; pw++) {
                if (*pw == '\\')
                    *pw = '/';
            }
        }

        if (!ar_seek(rar->super.stream, rar->super.entry_offset + rar->entry.header_size, SEEK_SET))
            warn("Couldn't seek back to the end of the entry header");
    }
    return rar->entry.name;
}

const WCHAR *rar_get_name_w(ar_archive_rar *rar)
{
    if (!rar->entry.name_w && rar_get_name(rar))
        rar->entry.name_w = conv_utf8_to_utf16(rar->entry.name);
    return rar->entry.name_w;
}
