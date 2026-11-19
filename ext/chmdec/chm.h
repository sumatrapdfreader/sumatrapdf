/* chmdec -- plain-C CHM reader. https://github.com/kjk/chmdec
 *
 * Derived from CHMLib by Jed Wing <jedwin@ugcs.caltech.edu>. The LZX
 * decompressor derives from cabextract by Stuart Caie, who granted permission
 * to re-license it under the LGPL for CHMLib.
 * Changes Copyright (c) 2026 Krzysztof Kowalczyk.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version. See LICENSE.md.
 */

/* chm.h -- plain-C CHM archive reader (in the style of djvudec / jbig2dec).
 *
 * Read-only access to .chm (and ITSS) archives. The caller supplies the
 * entire file up-front as an in-memory buffer that must outlive the chm.
 *
 * Simplified / cleaned API compared to CHMLib's chm_lib.h (not
 * source-compatible with it).
 */

#ifndef CHM_H
#define CHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- allocator + diagnostics (djvudec / jbig2dec style) ----- */

/* ctx identifies the chm_ctx the allocation belongs to. NULL only for
   bootstrap allocation/free of the chm_ctx struct itself. */
typedef void *(*chm_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*chm_free_cb)(void *user, void *ctx, void *ptr);

/* msg is a NUL-terminated, already-formatted message. */
typedef void (*chm_error_cb)(void *user, int severity, const char *msg);

typedef struct chm_ctx chm_ctx;

/* Pass NULL for both alloc and free_cb to use the default malloc/free.
   Set both or neither: each NULL is replaced by its default on its own, so a
   custom alloc with a NULL free_cb would free() memory it didn't malloc().
   Pass NULL for error to silently ignore diagnostics. */
chm_ctx *chm_ctx_new(chm_alloc_cb alloc, chm_free_cb free_cb,
                     chm_error_cb error, void *user);
void chm_ctx_free(chm_ctx *ctx);

/* ----- archives ----- */

/* Open an archive over an in-memory buffer (NOT copied; must remain valid
   until chm_close). The archive state lives inside the ctx.
   Returns true on success, false on failure (diagnostics via error cb if set).
   This is the only open entrypoint (in-memory only). */
bool chm_open(chm_ctx *ctx, const uint8_t *data, size_t len);
void chm_close(chm_ctx *ctx);

/* ----- entries (files/directories inside the archive) ----- */

/* content-section index stored in a directory entry's "space" field */
#define CHM_UNCOMPRESSED 0 /* the uncompressed section */
#define CHM_COMPRESSED 1   /* the MSCompressed (LZX) section */

/* chm_entry describes one entry inside the CHM.
   The 'path' string is allocated and owned by the chm_ctx; it is valid
   until chm_close and must not be freed by the caller. */
struct chm_entry {
    uint64_t start;
    uint64_t length;
    uint32_t space;        /* raw content-section index (see CHM_* above) */
    bool is_compressed;    /* space == CHM_COMPRESSED (i.e. exactly 1) */
    bool is_dir;
    bool is_file;
    bool is_normal;
    bool is_meta;
    bool is_special;
    char *path;
};

/* retrieve an entire entry from the archive.
   The caller must provide a buffer of at least entry->length bytes.
   Returns the number of bytes read. The read succeeded only if the result
   == entry->length: on corrupt data a compressed entry can return a short
   count (bytes before the failure), and failure otherwise returns 0.
   A zero-length entry returns 0. */
int64_t chm_read_entry(chm_ctx *ctx, struct chm_entry *entry, uint8_t *buf);

/* Return the number of entries and set *outEntries to an internal array of
   pointers to chm_entry (the array and all strings are owned by ctx
   and are freed by chm_close). Returns 0 on error or if nothing is open.
   Entries are collected during chm_open; if that runs out of memory,
   chm_open still succeeds and this returns 0. */
int chm_get_entries(chm_ctx *ctx, struct chm_entry ***outEntries);

#ifdef __cplusplus
}
#endif

#endif /* CHM_H */
