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
        log("Invalid header size %Iu", header->size);
        return false;
    }

    ar->entry_offset = ar_tell(ar->stream) - read;
    ar->entry_size_block = 0;
    ar->entry_size_uncompressed = 0;
    return true;
}

bool rar_check_header_crc(ar_stream *stream, size_t offset)
{
    unsigned char buffer[256];
    uint16_t crc16, size;
    uint32_t crc;

    if (!ar_seek(stream, offset, SEEK_SET))
        return false;
    if (ar_read(stream, buffer, 7) != 7)
        return false;

    crc16 = uint16le(buffer + 0);
    size = uint16le(buffer + 5);
    if (size < 7)
        return false;
    size -= 7;

    crc = crc32(0, buffer + 2, 5);
    while (size > 0) {
        if (ar_read(stream, buffer, min(size, sizeof(buffer))) != min(size, sizeof(buffer)))
            return false;
        crc = crc32(crc, buffer, min(size, sizeof(buffer)));
        size -= min(size, sizeof(buffer));
    }
    return (crc & 0xFFFF) == crc16;
}

static void rar_update_entry_name(ar_archive *ar, struct rar_header *header, struct rar_entry *entry)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);

    free(ar_rar->entry.name);
    ar_rar->entry.name = NULL;
    free(ar_rar->entry.name_w);
    ar_rar->entry.name_w = NULL;

    if (!entry->name) {
        log("OOM when allocating memory for the filename");
    }
    else if (!(header->flags & LHD_UNICODE)) {
#ifdef _WIN32
        int res = MultiByteToWideChar(CP_ACP, 0, entry->name, -1, NULL, 0);
        ar_rar->entry.name_w = malloc(res * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, entry->name, -1, ar_rar->entry.name_w, res);
        res = WideCharToMultiByte(CP_UTF8, 0, ar_rar->entry.name_w, -1, NULL, 0, NULL, NULL);
        ar_rar->entry.name = malloc(res);
        WideCharToMultiByte(CP_UTF8, 0, ar_rar->entry.name_w, -1, ar_rar->entry.name, res, NULL, NULL);
#else
#error need conversion from CP_ACP to CP_UTF8
#endif
    }
    else if (entry->namelen == strlen(entry->name)) {
        ar_rar->entry.name = entry->name;
        entry->name = NULL;
    }
    else {
#ifdef _WIN32
        int res = WideCharToMultiByte(CP_UTF8, 0, (const WCHAR *)entry->name, -1, NULL, 0, NULL, NULL);
        ar_rar->entry.name = malloc(res);
        WideCharToMultiByte(CP_UTF8, 0, (const WCHAR *)entry->name, -1, ar_rar->entry.name, res, NULL, NULL);
        ar_rar->entry.name_w = (WCHAR *)entry->name;
        entry->name = NULL;
#else
#error need conversion from CP_UTF16LE to CP_UTF8
#endif
    }

    /* normalize path separators */
    if (ar_rar->entry.name) {
        char *p = ar_rar->entry.name;
        while ((p = strchr(p, '\\')) != NULL) {
            *p = '/';
        }
    }
    if (ar_rar->entry.name_w) {
        WCHAR *pw;
        for (pw = ar_rar->entry.name_w; *pw; pw++) {
            if (*pw == '\\')
                *pw = '/';
        }
    }

    free(entry->name);
    entry->name = NULL;
}

bool rar_parse_header_entry(ar_archive *ar, struct rar_header *header, struct rar_entry *entry)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);

    unsigned char data[21];
    if (ar_read(ar->stream, data, sizeof(data)) != sizeof(data))
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
        if (ar_read(ar->stream, more_data, sizeof(more_data)) != sizeof(more_data))
            return false;
        header->datasize += (uint64_t)uint32le(more_data + 0);
        entry->size += (uint64_t)uint32le(more_data + 4);
    }
    entry->name = malloc(entry->namelen + 2);
    if (entry->name) {
        if (ar_read(ar->stream, entry->name, entry->namelen) != entry->namelen) {
            free(entry->name);
            return false;
        }
        entry->name[entry->namelen] = entry->name[entry->namelen + 1] = '\0';
    }
    if ((header->flags & LHD_SALT)) {
        log("Skipping LHD_SALT");
        ar_skip(ar->stream, 8);
    }
    if ((header->flags & LHD_SOLID)) {
        log("TODO: support LHD_SOLID");
    }
    if (entry->method != METHOD_STORE && entry->version != 29) {
        log("Unsupported compression version: %d", entry->version);
        free(entry->name);
        entry->name = NULL;
        return false;
    }

    ar_rar->entry.method = entry->method;
    ar_rar->entry.crc = entry->crc;
    rar_update_entry_name(ar, header, entry);
    ar_rar->uncomp.offset = header->size;
    ar_rar->uncomp.crc = 0;

    ar->entry_size_block = header->size + (size_t)header->datasize;
    ar->entry_size_uncompressed = (size_t)entry->size;
    return true;
}
