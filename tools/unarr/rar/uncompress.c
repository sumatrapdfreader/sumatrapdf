/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "rar.h"

static bool rar_copy_stored(ar_archive *ar, void *buffer, size_t count)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);

    if (count > ar->entry_size_block - ar_rar->uncomp.offset) {
        log("Requesting too much data (%Iu < %Iu)", ar->entry_size_block - ar_rar->uncomp.offset, count);
        return false;
    }
    if (ar_read(ar->stream, buffer, count) != count) {
        log("Unexpected EOF in stored data");
        return false;
    }
    ar_rar->uncomp.offset += count;
    return true;
}

bool rar_uncompress_part(ar_archive *ar, void *buffer, size_t count)
{
    struct rar_data *ar_rar = (struct rar_data *)(ar + 1);

    switch (ar_rar->entry.method) {
    case METHOD_STORE:
        if (!rar_copy_stored(ar, buffer, count))
            return false;
        break;

    default:
        log("Unsupported compression method %02x", ar_rar->entry.method);
        return false;
    }

    ar_rar->uncomp.crc = crc32(ar_rar->uncomp.crc, buffer, count);
    if (ar_rar->uncomp.offset == ar->entry_size_block && ar_rar->uncomp.crc != ar_rar->entry.crc) {
        log("Checksum of extracted data doesn't match");
        return false;
    }

    return true;
}
