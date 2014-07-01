/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef unarr_h
#define unarr_h

#ifdef _WIN32
#include <windows.h>
#else
typedef unsigned short WCHAR;
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

/***** stream *****/

typedef struct ar_stream_s ar_stream;

void ar_close(ar_stream *stream);
size_t ar_read(ar_stream *stream, void *buffer, size_t count);
bool ar_seek(ar_stream *stream, ptrdiff_t offset, int origin);
bool ar_skip(ar_stream *stream, ptrdiff_t count);
size_t ar_tell(ar_stream *stream);

ar_stream *ar_open_file(const char *path);
ar_stream *ar_open_file_w(const WCHAR *path);
#ifdef _WIN32
ar_stream *ar_open_istream(IStream *stream);
#endif

/***** unarr *****/

typedef struct ar_archive_s ar_archive;

void ar_close_archive(ar_archive *ar);
bool ar_at_eof(ar_archive *ar);
bool ar_parse_entry(ar_archive *ar);
bool ar_parse_entry_at(ar_archive *ar, size_t offset);

const char *ar_entry_get_name(ar_archive *ar);
const WCHAR *ar_entry_get_name_w(ar_archive *ar);
size_t ar_entry_get_offset(ar_archive *ar);
size_t ar_entry_get_size(ar_archive *ar);
size_t ar_entry_uncompress(ar_archive *ar, void *buffer, size_t count);

/***** rar/rar *****/

ar_archive *ar_open_rar_archive(ar_stream *stream);

#endif
