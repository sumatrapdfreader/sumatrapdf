/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr-imp.h"

ar_archive *ar_open_archive(ar_stream *stream, size_t struct_size, ar_archive_close_fn close, ar_parse_entry_fn parse_entry,
                            ar_entry_get_name_fn get_name, ar_entry_uncompress_fn uncompress, ar_get_global_comment_fn get_comment,
                            off64_t first_entry_offset)
{
    ar_archive *ar = malloc(struct_size);
    if (!ar)
        return NULL;
    memset(ar, 0, struct_size);
    ar->close = close;
    ar->parse_entry = parse_entry;
    ar->get_name = get_name;
    ar->uncompress = uncompress;
    ar->get_comment = get_comment;
    ar->stream = stream;
    ar->entry_offset_first = first_entry_offset;
    ar->entry_offset_next = first_entry_offset;
    return ar;
}

void ar_close_archive(ar_archive *ar)
{
    if (ar)
        ar->close(ar);
    free(ar);
}

bool ar_at_eof(ar_archive *ar)
{
    return ar->at_eof;
}

bool ar_parse_entry(ar_archive *ar)
{
    return ar->parse_entry(ar, ar->entry_offset_next);
}

bool ar_parse_entry_at(ar_archive *ar, off64_t offset)
{
    ar->at_eof = false;
    return ar->parse_entry(ar, offset ? offset : ar->entry_offset_first);
}

bool ar_parse_entry_for(ar_archive *ar, const char *entry_name)
{
    ar->at_eof = false;
    if (!entry_name)
        return false;
    if (!ar_parse_entry_at(ar, ar->entry_offset_first))
        return false;
    do {
        const char *name = ar_entry_get_name(ar);
        if (name && strcmp(name, entry_name) == 0)
            return true;
    } while (ar_parse_entry(ar));
    return false;
}

const char *ar_entry_get_name(ar_archive *ar)
{
    return ar->get_name(ar);
}

off64_t ar_entry_get_offset(ar_archive *ar)
{
    return ar->entry_offset;
}

size_t ar_entry_get_size(ar_archive *ar)
{
    return ar->entry_size_uncompressed;
}

time64_t ar_entry_get_filetime(ar_archive *ar)
{
    return ar->entry_filetime;
}

bool ar_entry_uncompress(ar_archive *ar, void *buffer, size_t count)
{
    return ar->uncompress(ar, buffer, count);
}

size_t ar_get_global_comment(ar_archive *ar, void *buffer, size_t count)
{
    if (!ar->get_comment)
        return 0;
    return ar->get_comment(ar, buffer, count);
}

void ar_log(const char *prefix, const char *file, int line, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    if (prefix)
        fprintf(stderr, "%s ", prefix);
    if (strrchr(file, '/'))
        file = strrchr(file, '/') + 1;
    if (strrchr(file, '\\'))
        file = strrchr(file, '\\') + 1;
    fprintf(stderr, "%s:%d: ", file, line);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
}
