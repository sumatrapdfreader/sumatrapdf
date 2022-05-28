/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
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

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "alloc_fail.h"

static int fail_probability;

void dav1d_setup_alloc_fail(unsigned seed, unsigned probability) {
    srand(seed);

    while (probability >= RAND_MAX)
        probability >>= 1;

    fail_probability = probability;
}

void * __wrap_malloc(size_t);

void * __wrap_malloc(size_t sz) {
    if (rand() < fail_probability)
        return NULL;
    return malloc(sz);
}

#if defined(HAVE_POSIX_MEMALIGN)
int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size);

int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size) {
    if (rand() < fail_probability)
        return ENOMEM;
    return posix_memalign(memptr, alignment, size);
}
#else
#error "HAVE_POSIX_MEMALIGN required"
#endif

int __wrap_pthread_create(pthread_t *, const pthread_attr_t *,
                          void *(*) (void *), void *);

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
    if (rand() < (fail_probability + RAND_MAX/16))
        return EAGAIN;

    return pthread_create(thread, attr, start_routine, arg);
}

int __wrap_pthread_mutex_init(pthread_mutex_t *,
                              const pthread_mutexattr_t *);

int __wrap_pthread_mutex_init(pthread_mutex_t *restrict mutex,
                              const pthread_mutexattr_t *restrict attr)
{
    if (rand() < (fail_probability + RAND_MAX/8))
        return ENOMEM;

    return pthread_mutex_init(mutex, attr);
}

int __wrap_pthread_cond_init(pthread_cond_t *,
                             const pthread_condattr_t *);

int __wrap_pthread_cond_init(pthread_cond_t *restrict cond,
                             const pthread_condattr_t *restrict attr)
{
    if (rand() < (fail_probability + RAND_MAX/16))
        return ENOMEM;

    return pthread_cond_init(cond, attr);
}
