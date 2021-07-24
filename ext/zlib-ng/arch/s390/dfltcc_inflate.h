#ifndef DFLTCC_INFLATE_H
#define DFLTCC_INFLATE_H

#include "dfltcc_common.h"

int Z_INTERNAL dfltcc_can_inflate(PREFIX3(streamp) strm);
typedef enum {
    DFLTCC_INFLATE_CONTINUE,
    DFLTCC_INFLATE_BREAK,
    DFLTCC_INFLATE_SOFTWARE,
} dfltcc_inflate_action;
dfltcc_inflate_action Z_INTERNAL dfltcc_inflate(PREFIX3(streamp) strm, int flush, int *ret);
int Z_INTERNAL dfltcc_was_inflate_used(PREFIX3(streamp) strm);
int Z_INTERNAL dfltcc_inflate_disable(PREFIX3(streamp) strm);

#define INFLATE_RESET_KEEP_HOOK(strm) \
    dfltcc_reset((strm), sizeof(struct inflate_state))

#define INFLATE_PRIME_HOOK(strm, bits, value) \
    do { if (dfltcc_inflate_disable((strm))) return Z_STREAM_ERROR; } while (0)

#define INFLATE_TYPEDO_HOOK(strm, flush) \
    if (dfltcc_can_inflate((strm))) { \
        dfltcc_inflate_action action; \
\
        RESTORE(); \
        action = dfltcc_inflate((strm), (flush), &ret); \
        LOAD(); \
        if (action == DFLTCC_INFLATE_CONTINUE) \
            break; \
        else if (action == DFLTCC_INFLATE_BREAK) \
            goto inf_leave; \
    }

#define INFLATE_NEED_CHECKSUM(strm) (!dfltcc_can_inflate((strm)))

#define INFLATE_NEED_UPDATEWINDOW(strm) (!dfltcc_can_inflate((strm)))

#define INFLATE_MARK_HOOK(strm) \
    do { \
        if (dfltcc_was_inflate_used((strm))) return -(1L << 16); \
    } while (0)

#define INFLATE_SYNC_POINT_HOOK(strm) \
    do { \
        if (dfltcc_was_inflate_used((strm))) return Z_STREAM_ERROR; \
    } while (0)

#endif
