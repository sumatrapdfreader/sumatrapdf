/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#ifndef tar_tar_h
#define tar_tar_h

#include "../common/unarr-imp.h"

typedef struct ar_archive_tar_s ar_archive_tar;

/***** parse-tar *****/

#define TAR_BLOCK_SIZE 512

enum tar_filetype {
    TYPE_FILE = '0', TYPE_FILE_OLD = '\0',
    TYPE_HARD_LINK = '1', TYPE_SOFT_LINK = '2',
    TYPE_DIRECTORY = '5',
    TYPE_LONGNAME = 'L',
};

struct tar_entry {
    char *name;
    size_t filesize;
    time64_t mtime;
    uint32_t checksum;
    char filetype;
    bool is_ustar;
};

bool tar_parse_header(ar_archive_tar *tar);
bool ar_is_valid_utf8(const char *string);
const char *tar_get_name(ar_archive *ar);

/***** tar *****/

struct ar_archive_tar_s {
    ar_archive super;
    struct tar_entry entry;
    size_t bytes_done;
};

#endif
