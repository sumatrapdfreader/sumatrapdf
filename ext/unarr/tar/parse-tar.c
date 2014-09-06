/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "tar.h"

static uint64_t tar_parse_number(const char *data, size_t size)
{
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < size; i++) {
        if (data[i] == ' ' || data[i] == '\0')
            continue;
        if (data[i] < '0' || '7' < data[i]) {
            warn("Invalid character in octal number: %c", data[i]);
            return 0;
        }
        value = value * 8 + (data[i] - '0');
    }

    return value;
}

static bool tar_is_zeroed_block(const char *data)
{
    size_t i;
    for (i = 0; i < TAR_BLOCK_SIZE; i++) {
        if (data[i] != 0)
            return false;
    }
    return true;
}

bool tar_parse_header(ar_archive_tar *tar)
{
    char data[TAR_BLOCK_SIZE];
    uint32_t checksum;
    int32_t checksum2;
    size_t i;

    if (ar_read(tar->super.stream, data, sizeof(data)) != sizeof(data))
        return false;

    if (tar_is_zeroed_block(data)) {
        free(tar->entry.name);
        memset(&tar->entry, 0, sizeof(tar->entry));
        return true;
    }

    tar->entry.filesize = (size_t)tar_parse_number(data + 124, 12);
    tar->entry.mtime = (tar_parse_number(data + 136, 12) + 11644473600) * 10000000;
    tar->entry.checksum = (uint32_t)tar_parse_number(data + 148, 8);
    tar->entry.filetype = data[156];
    free(tar->entry.name);
    tar->entry.name = NULL;

    if (tar->entry.filetype == TYPE_FILE_OLD) {
        i = 100;
        while (--i > 0 && data[i] == '\0');
        if (data[i] == '/')
            tar->entry.filetype = TYPE_DIRECTORY;
    }
    tar->entry.is_ustar = !memcmp(data + 257, "ustar\x0000", 8) && memcmp(data + 508, "tar\0", 4) != 0;

    if (tar->entry.filesize > (size_t)-1 - tar->super.entry_offset - 2 * TAR_BLOCK_SIZE)
        return false;

    checksum = 0;
    checksum2 = 0;
    memset(data + 148, ' ', 8);
    for (i = 0; i < sizeof(data); i++) {
        checksum += (unsigned char)data[i];
        checksum2 += (signed char)data[i];
    }

    if (checksum != (uint32_t)checksum2 && tar->entry.checksum == (uint32_t)checksum2) {
        log("Checksum was calculated using signed data");
        tar->entry.checksum = checksum;
    }
    return tar->entry.checksum == checksum;
}

bool ar_is_valid_utf8(const char *string)
{
    const unsigned char *s;
    for (s = (const unsigned char *)string; *s; s++) {
        int skip = *s < 0x80 ? 0 :
                   *s < 0xC0 ? -1 :
                   *s < 0xE0 ? 1 :
                   *s < 0xF0 ? 2 :
                   *s < 0xF5 ? 3 : -1;
        if (skip < 0)
            return false;
        while (skip-- > 0) {
            if ((*++s & 0xC0) != 0x80)
                return false;
        }
    }
    return true;
}

const char *tar_get_name(ar_archive *ar)
{
    ar_archive_tar *tar = (ar_archive_tar *)ar;
    if (!tar->entry.name) {
        char *name;

        if (!ar_seek(ar->stream, ar->entry_offset, SEEK_SET))
            return NULL;

        name = malloc(100 + 1);
        if (!name || ar_read(ar->stream, name, 100) != 100) {
            free(name);
            ar_seek(ar->stream, ar->entry_offset + TAR_BLOCK_SIZE, SEEK_SET);
            return NULL;
        }
        name[100] = '\0';

        if (tar->entry.is_ustar) {
            char *prefixed = malloc(256 + 1);
            if (!ar_skip(ar->stream, 245) || ar_read(ar->stream, prefixed, 167) != 167) {
                free(name);
                free(prefixed);
                ar_seek(ar->stream, ar->entry_offset + TAR_BLOCK_SIZE, SEEK_SET);
                return NULL;
            }
            if (prefixed[0] != '\0') {
                prefixed[156] = '\0';
                strcat(prefixed, "/");
                strcat(prefixed, name);
                free(name);
                name = prefixed;
                prefixed = NULL;
            }
            free(prefixed);
        }
        else
            ar_skip(ar->stream, TAR_BLOCK_SIZE - 100);

        /* name could be in any encoding, assume UTF-8 or whatever (DOS) */
        if (ar_is_valid_utf8(name)) {
            tar->entry.name = name;
        }
        else {
            tar->entry.name = ar_conv_dos_to_utf8(name);
            free(name);
        }
        /* normalize path separators */
        if (tar->entry.name) {
            char *p = tar->entry.name;
            while ((p = strchr(p, '\\')) != NULL) {
                *p = '/';
            }
        }
    }
    return tar->entry.name;
}
