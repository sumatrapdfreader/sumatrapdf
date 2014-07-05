/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#ifndef unarr_h
#define unarr_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef int64_t off64_t;
#ifdef _WIN32
typedef wchar_t wchar16_t;
#else
typedef unsigned short wchar16_t;
#endif

/***** stream *****/

typedef struct ar_stream_s ar_stream;

void ar_close(ar_stream *stream);
size_t ar_read(ar_stream *stream, void *buffer, size_t count);
bool ar_seek(ar_stream *stream, off64_t offset, int origin);
bool ar_skip(ar_stream *stream, off64_t count);
off64_t ar_tell(ar_stream *stream);

ar_stream *ar_open_file(const char *path);
ar_stream *ar_open_file_w(const wchar16_t *path);
ar_stream *ar_open_memory(const void *data, size_t datalen);
#ifdef _WIN32
typedef struct IStream IStream;
ar_stream *ar_open_istream(IStream *stream);
#endif

/***** unarr *****/

typedef struct ar_archive_s ar_archive;

void ar_close_archive(ar_archive *ar);
bool ar_at_eof(ar_archive *ar);
bool ar_parse_entry(ar_archive *ar);
bool ar_parse_entry_at(ar_archive *ar, off64_t offset);

const char *ar_entry_get_name(ar_archive *ar);
const wchar16_t *ar_entry_get_name_w(ar_archive *ar);
off64_t ar_entry_get_offset(ar_archive *ar);
size_t ar_entry_get_size(ar_archive *ar);
uint32_t ar_entry_get_dosdate(ar_archive *ar);
bool ar_entry_uncompress(ar_archive *ar, void *buffer, size_t count);

/***** rar/rar *****/

ar_archive *ar_open_rar_archive(ar_stream *stream);

#endif
