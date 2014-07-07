/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "zip.h"

#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif

inline uint16_t uint16le(unsigned char *data) { return data[0] | data[1] << 8; }
inline uint32_t uint32le(unsigned char *data) { return data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24; }
inline uint64_t uint64le(unsigned char *data) { return (uint64_t)uint32le(data) | (uint64_t)uint32le(data + 4) << 32; }

bool zip_seek_to_compressed_data(ar_archive_zip *zip)
{
    uint8_t data[30];

    if (!ar_seek(zip->super.stream, zip->entry.offset, SEEK_SET))
        return false;
    if (ar_read(zip->super.stream, data, sizeof(data)) != sizeof(data))
        return false;
    if (uint32le(data + 0) != SIG_LOCAL_FILE_HEADER)
        return false;
    if (uint16le(data + 8) != zip->entry.method) {
        warn("Compression methods don't match: %d != %d", zip->entry.method, uint16le(data + 8));
        if (!zip->entry.method)
            zip->entry.method = uint16le(data + 8);
    }
    if (uint32le(data + 10) != zip->entry.dosdate) {
        warn("Timestamps don't match");
        if (!zip->entry.dosdate) {
            zip->entry.dosdate = uint32le(data + 10);
            zip->super.entry_filetime = ar_conv_dosdate_to_filetime(zip->entry.dosdate);
        }
    }
    /* skip filename and extra field */
    if (!ar_skip(zip->super.stream, uint16le(data + 26) + uint16le(data + 28)))
        return false;

    return true;
}

bool zip_parse_directory_entry(ar_archive_zip *zip, struct zip_entry *entry)
{
    uint8_t data[ZIP_DIR_ENTRY_FIXED_SIZE];
    uint8_t *extra;
    uint16_t idx;

    if (ar_read(zip->super.stream, data, sizeof(data)) != sizeof(data))
        return false;

    entry->signature = uint32le(data + 0);
    entry->version = uint16le(data + 4);
    entry->min_version = uint16le(data + 6);
    entry->flags = uint16le(data + 8);
    entry->method = uint16le(data + 10);
    entry->dosdate = uint32le(data + 12);
    entry->crc = uint32le(data + 16);
    entry->datasize = uint32le(data + 20);
    entry->uncompressed = uint32le(data + 24);
    entry->namelen = uint16le(data + 28);
    entry->extralen = uint16le(data + 30);
    entry->commentlen = uint16le(data + 32);
    entry->disk = uint16le(data + 34);
    entry->attr_internal = uint16le(data + 36);
    entry->attr_external = uint32le(data + 38);
    entry->header_offset = uint32le(data + 42);

    if (entry->signature != SIG_CENTRAL_DIRECTORY)
        return false;
    if (!entry->extralen)
        return true;

    /* read ZIP64 values where needed */
    if (!ar_skip(zip->super.stream, entry->namelen))
        return false;
    extra = malloc(entry->extralen);
    if (!extra || ar_read(zip->super.stream, extra, entry->extralen) != entry->extralen) {
        free(extra);
        return false;
    }
    for (idx = 0; idx + 4 < entry->extralen; idx += 4 + uint16le(&extra[idx + 2])) {
        if (uint16le(&extra[idx]) == 0x0001) {
            uint16_t size = uint16le(&extra[idx + 2]);
            uint16_t offset = 0;
            if (entry->uncompressed == UINT32_MAX && offset + 8 <= size) {
                entry->uncompressed = uint64le(&extra[idx + 4 + offset]);
                offset += 8;
            }
            if (entry->datasize == UINT32_MAX && offset + 8 <= size) {
                entry->datasize = uint64le(&extra[idx + 4 + offset]);
                offset += 8;
            }
            if (entry->header_offset == UINT32_MAX && offset + 8 <= size) {
                entry->header_offset = (off64_t)uint64le(&extra[idx + 4 + offset]);
                offset += 8;
            }
            if (entry->disk == UINT16_MAX && offset + 4 <= size) {
                entry->disk = uint32le(&extra[idx + 4 + offset]);
                offset += 4;
            }
            break;
        }
    }
    free(extra);

    return true;
}

bool zip_parse_end_of_central_directory(ar_stream *stream, struct zip_eocd64 *eocd)
{
    uint8_t data[56];
    if (ar_read(stream, data, 22) != 22)
        return false;

    eocd->signature = uint32le(data + 0);
    eocd->diskno = uint16le(data + 4);
    eocd->diskno_dir = uint16le(data + 6);
    eocd->numentries_disk = uint16le(data + 8);
    eocd->numentries = uint16le(data + 10);
    eocd->dir_size = uint32le(data + 12);
    eocd->dir_offset = uint32le(data + 16);
    eocd->commentlen = uint16le(data + 20);

    if (eocd->signature != SIG_END_OF_CENTRAL_DIRECTORY)
        return false;

    /* try to locate the ZIP64 end of central directory */
    if (!ar_skip(stream, -42))
        return eocd->dir_size < 20;
    if (ar_read(stream, data, 20) != 20)
        return false;
    if (uint32le(data + 0) != SIG_END_OF_CENTRAL_DIRECTORY_64_LOCATOR)
        return true;
    if ((eocd->diskno != UINT16_MAX && uint32le(data + 4) != eocd->diskno) || uint32le(data + 16) != 1) {
        warn("Archive spanning isn't supported");
        return false;
    }
    if (!ar_seek(stream, (off64_t)uint64le(data + 8), SEEK_SET))
        return false;
    if (ar_read(stream, data, 56) != 56)
        return false;

    /* use data from ZIP64 end of central directory (when necessary) */
    eocd->signature = uint32le(data + 0);
    eocd->version = uint16le(data + 12);
    eocd->min_version = uint16le(data + 14);
    if (eocd->diskno == UINT16_MAX)
        eocd->diskno = uint32le(data + 16);
    if (eocd->diskno_dir == UINT16_MAX)
        eocd->diskno_dir = uint32le(data + 20);
    if (eocd->numentries_disk == UINT16_MAX)
        eocd->numentries_disk = uint64le(data + 24);
    if (eocd->numentries == UINT16_MAX)
        eocd->numentries = uint64le(data + 32);
    if (eocd->dir_size == UINT32_MAX)
        eocd->dir_size = uint64le(data + 40);
    if (eocd->dir_offset == UINT32_MAX)
        eocd->dir_offset = (off64_t)uint64le(data + 48);

    if (eocd->signature != SIG_END_OF_CENTRAL_DIRECTORY_64)
        return false;
    if (eocd->diskno != eocd->diskno_dir || eocd->numentries != eocd->numentries_disk) {
        warn("Archive spanning isn't supported");
        return false;
    }
    if (uint64le(data + 4) > 44)
        log("ZIP64 extensible data sector present @" PRIi64, ar_tell(stream));

    return true;
}

off64_t zip_find_end_of_central_directory(ar_stream *stream)
{
    uint8_t data[512];
    off64_t filesize;
    int fromend = 0;
    int count, i;

    if (!ar_seek(stream, 0, SEEK_END))
        return -1;
    filesize = ar_tell(stream);

    while (fromend < UINT16_MAX + 22 && fromend < filesize) {
        count = (int)(filesize - fromend < sizeof(data) ? filesize - fromend : sizeof(data));
        fromend += count;
        if (!ar_seek(stream, -fromend, SEEK_END))
            return -1;
        if (ar_read(stream, data, count) != (size_t)count)
            return -1;
        for (i = count - 22; i >= 0; i--) {
            if (uint32le(data + i) == SIG_END_OF_CENTRAL_DIRECTORY)
                return filesize - fromend + i;
        }
        fromend += 3;
    }

    return -1;
}

const char *zip_get_name(ar_archive *ar)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    if (!zip->entry.name) {
        struct zip_entry entry;
        char *name;

        if (!ar_seek(ar->stream, ar->entry_offset, SEEK_SET))
            return NULL;
        if (!zip_parse_directory_entry(zip, &entry))
            return NULL;
        if (!ar_seek(ar->stream, ar->entry_offset + ZIP_DIR_ENTRY_FIXED_SIZE, SEEK_SET))
            return NULL;

        name = malloc(entry.namelen + 1);
        if (!name || ar_read(ar->stream, name, entry.namelen) != entry.namelen) {
            free(name);
            return NULL;
        }
        name[entry.namelen] = '\0';

        if ((entry.flags & (1 << 11))) {
            zip->entry.name = name;
            zip->entry.name_w = NULL;
        }
        else {
            zip->entry.name = ar_conv_dos_to_utf8_utf16(name, &zip->entry.name_w);
            free(name);
        }
        /* normalize path separators */
        if (zip->entry.name) {
            char *p = zip->entry.name;
            while ((p = strchr(p, '\\')) != NULL) {
                *p = '/';
            }
        }
        if (zip->entry.name_w) {
            wchar16_t *pw;
            for (pw = zip->entry.name_w; *pw; pw++) {
                if (*pw == '\\')
                    *pw = '/';
            }
        }
    }
    return zip->entry.name;
}

const wchar16_t *zip_get_name_w(ar_archive *ar)
{
    ar_archive_zip *zip = (ar_archive_zip *)ar;
    if (!zip->entry.name_w && zip_get_name(ar) && !zip->entry.name_w)
        zip->entry.name_w = ar_conv_utf8_to_utf16(zip->entry.name);
    return zip->entry.name_w;
}
