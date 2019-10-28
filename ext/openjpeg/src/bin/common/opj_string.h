/*
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2015, Matthieu Darbois
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OPJ_STRING_H
#define OPJ_STRING_H

#include <errno.h>
#include <string.h>

/* strnlen is not standard, strlen_s is C11... */
/* keep in mind there still is a buffer read overflow possible */
static size_t opj_strnlen_s(const char *src, size_t max_len)
{
    size_t len;

    if (src == NULL) {
        return 0U;
    }
    for (len = 0U; (*src != '\0') && (len < max_len); src++, len++);
    return len;
}

/* should be equivalent to C11 function except for the handler */
/* keep in mind there still is a buffer read overflow possible */
static int opj_strcpy_s(char* dst, size_t dst_size, const char* src)
{
    size_t src_len = 0U;
    if ((dst == NULL) || (dst_size == 0U)) {
        return EINVAL;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return EINVAL;
    }
    src_len = opj_strnlen_s(src, dst_size);
    if (src_len >= dst_size) {
        return ERANGE;
    }
    memcpy(dst, src, src_len);
    dst[src_len] = '\0';
    return 0;
}

#endif /* OPJ_STRING_H */
