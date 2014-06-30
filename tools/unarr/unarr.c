/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "unarr-internals.h"

ar_archive *ar_open_archive(ar_stream *stream, size_t struct_size, ar_archive_close_fn close, ar_parse_entry_fn parse_entry,
                            ar_entry_get_name_fn get_name, ar_entry_get_name_w_fn get_name_w, ar_entry_uncompress_fn uncompress)
{
    ar_archive *ar = malloc(struct_size);
    if (!ar)
        return NULL;
    memset(ar, 0, struct_size);
    ar->close = close;
    ar->parse_entry = parse_entry;
    ar->get_name = get_name;
    ar->get_name_w = get_name_w;
    ar->uncompress = uncompress;
    ar->stream = stream;
    return ar;
}

void ar_close_archive(ar_archive *ar)
{
    ar->close(ar);
    free(ar);
}

bool ar_at_eof(ar_archive *ar)
{
    return ar->at_eof;
}

bool ar_parse_entry(ar_archive *ar)
{
    return ar->parse_entry(ar);
}

bool ar_parse_entry_at(ar_archive *ar, size_t offset)
{
    if (!ar_seek(ar->stream, offset, SEEK_SET))
        return false;
    ar->entry_offset = 0;
    ar->entry_size_block = offset;
    return ar->parse_entry(ar);
}

const char *ar_entry_get_name(ar_archive *ar)
{
    return ar->get_name(ar);
}

const WCHAR *ar_entry_get_name_w(ar_archive *ar)
{
    return ar->get_name_w(ar);
}

size_t ar_entry_get_offset(ar_archive *ar)
{
    return ar->entry_offset;
}

size_t ar_entry_get_size(ar_archive *ar)
{
    return ar->entry_size_uncompressed;
}

size_t ar_entry_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    return ar->uncompress(ar, buffer, count);
}
