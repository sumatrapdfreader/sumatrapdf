/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef rar_h
#define rar_h

#include "../unarr-internals.h"

struct rar_data {
    uint16_t flags;
    struct {
        uint8_t method;
        uint32_t crc;
        char *name;
        WCHAR *name_w;
    } entry;
    struct {
        size_t offset;
        uint32_t crc;
    } uncomp;
};

#ifdef DEBUG
#define log(msg, ...) fprintf(stderr, "Log: " msg "\n", __VA_ARGS__)
#else
#define log(msg, ...) ((void)0)
#endif

/***** parser *****/

enum archive_flags {
    MHD_VOLUME = 1 << 0, MHD_COMMENT = 1 << 1, MHD_LOCK = 1 << 2,
    MHD_SOLID = 1 << 3, MHD_PACK_COMMENT = 1 << 4, MHD_AV = 1 << 5,
    MHD_PROTECT = 1 << 6, MHD_PASSWORD = 1 << 7, MHD_FIRSTVOLUME = 1 << 8,
    MHD_ENCRYPTVER = 1 << 9,
    MHD_LONG_BLOCK = 1 << 15,
};

enum entry_flags {
    LHD_SPLIT_BEFORE = 1 << 0, LHD_SPLIT_AFTER = 1 << 1, LHD_PASSWORD = 1 << 2,
    LHD_COMMENT = 1 << 3, LHD_SOLID = 1 << 4,
    LHD_LARGE = 1 << 8, LHD_UNICODE = 1 << 9, LHD_SALT = 1 << 10,
    LHD_VERSION = 1 << 11, LHD_EXTTIME = 1 << 12, LHD_EXTFLAGS = 1 << 13,
    LHD_LONG_BLOCK = 1 << 15,
};

struct rar_header {
    uint16_t crc;
    uint8_t type;
    uint16_t flags;
    uint16_t size;
    uint64_t datasize;
};

struct rar_entry {
    uint64_t size;
    uint8_t os;
    uint32_t crc;
    uint32_t dostime;
    uint8_t version;
    uint8_t method;
    uint16_t namelen;
    uint32_t attrs;
    char *name;
};

bool rar_parse_header(ar_archive *ar, struct rar_header *header);
bool rar_check_header_crc(ar_stream *stream, size_t offset);
bool rar_parse_header_entry(ar_archive *ar, struct rar_header *header, struct rar_entry *entry);

/***** uncompress *****/

enum compression_method {
    METHOD_STORE = 0x30,
    METHOD_FASTEST = 0x31, METHOD_FAST = 0x32, METHOD_NORMAL = 0x33,
    METHOD_GOOD = 0x34, METHOD_BEST = 0x35,
};

bool rar_uncompress_part(ar_archive *ar, void *buffer, size_t count);

#endif
