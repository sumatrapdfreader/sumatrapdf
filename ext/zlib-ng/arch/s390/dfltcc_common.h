#ifndef DFLTCC_COMMON_H
#define DFLTCC_COMMON_H

#ifdef ZLIB_COMPAT
#include "../../zlib.h"
#else
#include "../../zlib-ng.h"
#endif
#include "../../zutil.h"

void Z_INTERNAL *dfltcc_alloc_state(PREFIX3(streamp) strm, uInt items, uInt size);
void Z_INTERNAL dfltcc_copy_state(void *dst, const void *src, uInt size);
void Z_INTERNAL dfltcc_reset(PREFIX3(streamp) strm, uInt size);
void Z_INTERNAL *dfltcc_alloc_window(PREFIX3(streamp) strm, uInt items, uInt size);
void Z_INTERNAL dfltcc_free_window(PREFIX3(streamp) strm, void *w);

#define ZALLOC_STATE dfltcc_alloc_state

#define ZFREE_STATE ZFREE

#define ZCOPY_STATE dfltcc_copy_state

#define ZALLOC_WINDOW dfltcc_alloc_window

#define ZFREE_WINDOW dfltcc_free_window

#define TRY_FREE_WINDOW dfltcc_free_window

#endif
