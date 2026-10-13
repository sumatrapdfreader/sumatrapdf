/*  filemem.h - write to file/memory abstraction */
/*
    libzint - the open source barcode library
    Copyright (C) 2023-2024 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef Z_FILEMEM_H
#define Z_FILEMEM_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include "common.h"

/* Whether `vsnprintf()` available */
#if (defined(_MSC_VER) && _MSC_VER < 1900) || defined(ZINT_IS_C89) /* Pre-MSVC 2015 (C++ 14.0) or C89 */
#define FM_NO_VSNPRINTF
#endif

struct filemem {
    FILE *fp;
    unsigned char *mem;
    size_t memsize;     /* Size of `mem` buffer (capacity) */
    size_t mempos;      /* Current position */
    size_t memend;      /* For use by `fm_seek()`, points to highest `mempos` reached */
    int flags;          /* BARCODE_MEMORY_FILE or BARCODE_STDOUT */
    int err;            /* `errno` values, reset only on `fm_open()` */
#ifdef FM_NO_VSNPRINTF
    FILE *fp_null;      /* Only used for BARCODE_MEMORY_FILE */
#endif
};

/* `fopen()` if file, setup memory buffer if BARCODE_MEMORY_FILE, returning 1 on success, 0 on failure */
INTERNAL int fm_open(struct filemem *restrict const fmp, struct zint_symbol *symbol, const char *mode);

/* `fwrite()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_write(const void *restrict ptr, const size_t size, const size_t nitems,
                    struct filemem *restrict const fmp);

/* `fputc()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_putc(const int ch, struct filemem *restrict const fmp);

/* `fputs()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_puts(const char *str, struct filemem *restrict const fmp);

/* `fprintf()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_printf(struct filemem *restrict const fmp, const char *format, ...) ZINT_FORMAT_PRINTF(2, 3);

/* Output float without trailing zeroes to `fmp` with decimal pts `dp` (precision), returning 1 on success, 0 on
   failure */
INTERNAL int fm_putsf(const char *prefix, const int dp, const float arg, struct filemem *restrict const fmp);

/* `fclose()` if file, set `symbol->memfile` & `symbol->memfile_size` if memory, returning 1 on success, 0 on
   failure */
INTERNAL int fm_close(struct filemem *restrict const fmp, struct zint_symbol *symbol);

/* `fseek()` to file/memory offset, returning 1 on success, 0 on failure */
INTERNAL int fm_seek(struct filemem *restrict const fmp, const long offset, const int whence);

/* `ftell()` returns current file/memory offset if successful, -1 on failure */
INTERNAL long fm_tell(struct filemem *restrict const fmp);

/* Return `err`, which uses `errno` values; if file and `err` not set, test `ferror()` also */
INTERNAL int fm_error(struct filemem *restrict const fmp);

/* `fflush()` if file, no-op if memory, returning 1 on success, 0 on failure
   NOTE: don't use, included only for libpng compatibility */
INTERNAL int fm_flush(struct filemem *restrict const fmp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_FILEMEM_H */
