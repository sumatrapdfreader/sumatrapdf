#ifndef DFLTCC_DEFLATE_H
#define DFLTCC_DEFLATE_H

#include "dfltcc_common.h"

int Z_INTERNAL dfltcc_can_deflate(PREFIX3(streamp) strm);
int Z_INTERNAL dfltcc_deflate(PREFIX3(streamp) strm, int flush, block_state *result);
int Z_INTERNAL dfltcc_deflate_params(PREFIX3(streamp) strm, int level, int strategy, int *flush);
int Z_INTERNAL dfltcc_deflate_done(PREFIX3(streamp) strm, int flush);
int Z_INTERNAL dfltcc_can_set_reproducible(PREFIX3(streamp) strm, int reproducible);
int Z_INTERNAL dfltcc_deflate_set_dictionary(PREFIX3(streamp) strm,
                                                const unsigned char *dictionary, uInt dict_length);
int Z_INTERNAL dfltcc_deflate_get_dictionary(PREFIX3(streamp) strm, unsigned char *dictionary, uInt* dict_length);

#define DEFLATE_SET_DICTIONARY_HOOK(strm, dict, dict_len) \
    do { \
        if (dfltcc_can_deflate((strm))) \
            return dfltcc_deflate_set_dictionary((strm), (dict), (dict_len)); \
    } while (0)

#define DEFLATE_GET_DICTIONARY_HOOK(strm, dict, dict_len) \
    do { \
        if (dfltcc_can_deflate((strm))) \
            return dfltcc_deflate_get_dictionary((strm), (dict), (dict_len)); \
    } while (0)

#define DEFLATE_RESET_KEEP_HOOK(strm) \
    dfltcc_reset((strm), sizeof(deflate_state))

#define DEFLATE_PARAMS_HOOK(strm, level, strategy, hook_flush) \
    do { \
        int err; \
\
        err = dfltcc_deflate_params((strm), (level), (strategy), (hook_flush)); \
        if (err == Z_STREAM_ERROR) \
            return err; \
    } while (0)

#define DEFLATE_DONE dfltcc_deflate_done

#define DEFLATE_BOUND_ADJUST_COMPLEN(strm, complen, source_len) \
    do { \
        if (dfltcc_can_deflate((strm))) \
            (complen) = DEFLATE_BOUND_COMPLEN(source_len); \
    } while (0)

#define DEFLATE_NEED_CONSERVATIVE_BOUND(strm) (dfltcc_can_deflate((strm)))

#define DEFLATE_HOOK dfltcc_deflate

#define DEFLATE_NEED_CHECKSUM(strm) (!dfltcc_can_deflate((strm)))

#define DEFLATE_CAN_SET_REPRODUCIBLE dfltcc_can_set_reproducible

#endif
