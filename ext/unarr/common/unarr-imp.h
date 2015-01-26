/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* this is the common private/implementation API of unarr which should only be used by unarr code */

#ifndef common_unarr_imp_h
#define common_unarr_imp_h

#include "../unarr.h"
#include "allocator.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

/***** conv ****/

size_t ar_conv_rune_to_utf8(wchar_t rune, char *out, size_t size);
char *ar_conv_dos_to_utf8(const char *astr);
time64_t ar_conv_dosdate_to_filetime(uint32_t dosdate);

/***** crc32 *****/

uint32_t ar_crc32(uint32_t crc32, const unsigned char *data, size_t data_len);

/***** stream *****/

typedef void (* ar_stream_close_fn)(void *data);
typedef size_t (* ar_stream_read_fn)(void *data, void *buffer, size_t count);
typedef bool (* ar_stream_seek_fn)(void *data, off64_t offset, int origin);
typedef off64_t (* ar_stream_tell_fn)(void *data);

struct ar_stream_s {
    ar_stream_close_fn close;
    ar_stream_read_fn read;
    ar_stream_seek_fn seek;
    ar_stream_tell_fn tell;
    void *data;
};

ar_stream *ar_open_stream(void *data, ar_stream_close_fn close, ar_stream_read_fn read, ar_stream_seek_fn seek, ar_stream_tell_fn tell);

/***** unarr *****/

#define warn(...) ar_log("!", __FILE__, __LINE__, __VA_ARGS__)
#ifndef NDEBUG
#define log(...) ar_log("-", __FILE__, __LINE__, __VA_ARGS__)
#else
#define log(...) ((void)0)
#endif
void ar_log(const char *prefix, const char *file, int line, const char *msg, ...);

typedef void (* ar_archive_close_fn)(ar_archive *ar);
typedef bool (* ar_parse_entry_fn)(ar_archive *ar, off64_t offset);
typedef const char *(* ar_entry_get_name_fn)(ar_archive *ar);
typedef bool (* ar_entry_uncompress_fn)(ar_archive *ar, void *buffer, size_t count);
typedef size_t (* ar_get_global_comment_fn)(ar_archive *ar, void *buffer, size_t count);

struct ar_archive_s {
    ar_archive_close_fn close;
    ar_parse_entry_fn parse_entry;
    ar_entry_get_name_fn get_name;
    ar_entry_uncompress_fn uncompress;
    ar_get_global_comment_fn get_comment;

    ar_stream *stream;
    bool at_eof;
    off64_t entry_offset;
    off64_t entry_offset_first;
    off64_t entry_offset_next;
    size_t entry_size_uncompressed;
    time64_t entry_filetime;
};

ar_archive *ar_open_archive(ar_stream *stream, size_t struct_size, ar_archive_close_fn close, ar_parse_entry_fn parse_entry,
                            ar_entry_get_name_fn get_name, ar_entry_uncompress_fn uncompress, ar_get_global_comment_fn get_comment,
                            off64_t first_entry_offset);

#endif
