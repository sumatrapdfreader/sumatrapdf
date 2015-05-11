/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#ifndef _7z_7z_h
#define _7z_7z_h

#include "../common/unarr-imp.h"

#include "../lzmasdk/7zTypes.h"
#ifdef HAVE_7Z
#include <7z.h>
#endif

typedef struct ar_archive_7z_s ar_archive_7z;

struct CSeekStream {
    ISeekInStream super;
    ar_stream *stream;
};

struct ar_archive_7z_uncomp {
    bool initialized;

    UInt32 folder_index;
    Byte *buffer;
    size_t buffer_size;

    size_t offset;
    size_t bytes_left;
};

struct ar_archive_7z_s {
    ar_archive super;
    struct CSeekStream in_stream;
#ifdef HAVE_7Z
    CLookToRead look_stream;
    CSzArEx data;
#endif
    char *entry_name;
    struct ar_archive_7z_uncomp uncomp;
};

#endif
