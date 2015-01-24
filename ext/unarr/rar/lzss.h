/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/LZSS.h */

#ifndef rar_lzss_h
#define rar_lzss_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif

typedef struct {
    uint8_t *window;
    int mask;
    int64_t position;
} LZSS;

static inline int64_t lzss_position(LZSS *self) { return self->position; }

static inline int lzss_mask(LZSS *self) { return self->mask; }

static inline int lzss_size(LZSS *self) { return self->mask + 1; }

static inline uint8_t *lzss_window_pointer(LZSS *self) { return self->window; }

static inline int lzss_offset_for_position(LZSS *self, int64_t pos) { return (int)(pos & self->mask); }

static inline uint8_t *lzss_window_pointer_for_position(LZSS *self, int64_t pos) { return &self->window[lzss_offset_for_position(self, pos)]; }

static inline int lzss_current_window_offset(LZSS *self) { return lzss_offset_for_position(self, self->position); }

static inline uint8_t *lzss_current_window_pointer(LZSS *self) { return lzss_window_pointer_for_position(self, self->position); }

static inline int64_t lzss_next_window_edge_after_position(LZSS *self, int64_t pos) { return (pos + lzss_size(self)) & ~(int64_t)lzss_mask(self); }

static inline int64_t lzss_next_window_edge(LZSS *self) { return lzss_next_window_edge_after_position(self, self->position); }

static inline uint8_t lzss_get_byte_from_window(LZSS *self, int64_t pos) { return *lzss_window_pointer_for_position(self, pos); }

static inline void lzss_emit_literal(LZSS *self, uint8_t literal) {
    /* self->window[(self->position & self->mask)] = literal; */
    *lzss_current_window_pointer(self) = literal;
    self->position++;
}

static inline void lzss_emit_match(LZSS *self, int offset, int length) {
    int windowoffs = lzss_current_window_offset(self);
    int i;
    for (i = 0; i < length; i++) {
        self->window[(windowoffs + i) & lzss_mask(self)] = self->window[(windowoffs + i - offset) & lzss_mask(self)];
    }
    self->position += length;
}

static inline void lzss_copy_bytes_from_window(LZSS *self, uint8_t *buffer, int64_t startpos, int length) {
    int windowoffs = lzss_offset_for_position(self, startpos);
    int firstpart = lzss_size(self) - windowoffs;
    if (length <= firstpart) {
        /* Request fits inside window */
        memcpy(buffer, &self->window[windowoffs], length);
    }
    else {
        /* Request wraps around window */
        memcpy(buffer, &self->window[windowoffs], firstpart);
        memcpy(buffer + firstpart, &self->window[0], length - firstpart);
    }
}

static inline bool lzss_initialize(LZSS *self, int windowsize) {
    self->window = malloc(windowsize);
    if (!self->window)
        return false;

    self->mask = windowsize - 1; /* Assume windows are power-of-two sized! */
    memset(self->window, 0, lzss_size(self));
    self->position = 0;
    return true;
}

static inline void lzss_cleanup(LZSS *self) { free(self->window); }

#endif
