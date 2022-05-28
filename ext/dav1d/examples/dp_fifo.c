/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <SDL.h>
#include <assert.h>

#include "dp_fifo.h"

// FIFO structure
struct dp_fifo
{
    SDL_mutex *lock;
    SDL_cond *cond_change;
    size_t capacity;
    size_t count;
    void **entries;
    int push_wait;
    int flush;
};


Dav1dPlayPtrFifo *dp_fifo_create(size_t capacity)
{
    Dav1dPlayPtrFifo *fifo;

    assert(capacity > 0);
    if (capacity <= 0)
        return NULL;

    fifo = malloc(sizeof(*fifo));
    if (fifo == NULL)
        return NULL;

    fifo->capacity = capacity;
    fifo->count = 0;
    fifo->push_wait = 0;
    fifo->flush = 0;

    fifo->lock = SDL_CreateMutex();
    if (fifo->lock == NULL) {
        free(fifo);
        return NULL;
    }
    fifo->cond_change = SDL_CreateCond();
    if (fifo->cond_change == NULL) {
        SDL_DestroyMutex(fifo->lock);
        free(fifo);
        return NULL;
    }

    fifo->entries = calloc(capacity, sizeof(void*));
    if (fifo->entries == NULL) {
        dp_fifo_destroy(fifo);
        return NULL;
    }

    return fifo;
}

// Destroy FIFO
void dp_fifo_destroy(Dav1dPlayPtrFifo *fifo)
{
    assert(fifo->count == 0);
    SDL_DestroyMutex(fifo->lock);
    SDL_DestroyCond(fifo->cond_change);
    free(fifo->entries);
    free(fifo);
}

// Push to FIFO
void dp_fifo_push(Dav1dPlayPtrFifo *fifo, void *element)
{
    SDL_LockMutex(fifo->lock);
    while (fifo->count == fifo->capacity) {
        fifo->push_wait = 1;
        SDL_CondWait(fifo->cond_change, fifo->lock);
        fifo->push_wait = 0;
        if (fifo->flush) {
            SDL_CondSignal(fifo->cond_change);
            SDL_UnlockMutex(fifo->lock);
            return;
        }
    }
    fifo->entries[fifo->count++] = element;
    if (fifo->count == 1)
        SDL_CondSignal(fifo->cond_change);
    SDL_UnlockMutex(fifo->lock);
}

// Helper that shifts the FIFO array
static void *dp_fifo_array_shift(void **arr, size_t len)
{
    void *shifted_element = arr[0];
    for (size_t i = 1; i < len; ++i)
        arr[i-1] = arr[i];
    return shifted_element;
}

// Get item from FIFO
void *dp_fifo_shift(Dav1dPlayPtrFifo *fifo)
{
    SDL_LockMutex(fifo->lock);
    while (fifo->count == 0)
        SDL_CondWait(fifo->cond_change, fifo->lock);
    void *res = dp_fifo_array_shift(fifo->entries, fifo->count--);
    if (fifo->count == fifo->capacity - 1)
        SDL_CondSignal(fifo->cond_change);
    SDL_UnlockMutex(fifo->lock);
    return res;
}

void dp_fifo_flush(Dav1dPlayPtrFifo *fifo, void (*destroy_elem)(void *))
{
    SDL_LockMutex(fifo->lock);
    fifo->flush = 1;
    if (fifo->push_wait) {
        SDL_CondSignal(fifo->cond_change);
        SDL_CondWait(fifo->cond_change, fifo->lock);
    }
    while (fifo->count)
        destroy_elem(fifo->entries[--fifo->count]);
    fifo->flush = 0;
    SDL_UnlockMutex(fifo->lock);
}
