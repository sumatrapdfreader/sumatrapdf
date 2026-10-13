/*  filemem.c - write to file/memory abstraction */
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h>
#endif

#include "filemem.h"
#include "output.h"

#define FM_PAGE_SIZE    0x8000 /* 32k */

#ifndef EOVERFLOW
#define EOVERFLOW   EINVAL
#endif

#if defined(_MSC_VER) && _MSC_VER < 1800 /* `va_copy()` not before MSVC 2013 (C++ 12.0) */
#  define va_copy(dest, src) (dest = src)
#else
#  if defined(ZINT_IS_C89) && !defined(va_copy)
#    ifdef __GNUC__
#      define va_copy __va_copy /* Available with clang as well */
#    else
#      define va_copy(dest, src) (dest = src) /* Will fail if array (need `*dest = *src`) or something else */
#    endif
#  endif
#endif

/* Helper to set `err` only if not already set, returning 0 always for convenience */
static int fm_seterr(struct filemem *restrict const fmp, const int err) {
    if (fmp->err == 0) {
        fmp->err = err;
    }
    return 0;
}

/* Helper to set position, syncing end marker */
static void fm_setpos(struct filemem *restrict const fmp, const size_t pos) {
    assert(pos <= fmp->memsize);
    fmp->mempos = pos;
    if (fmp->mempos > fmp->memend) {
        fmp->memend = fmp->mempos;
    }
}

/* Helper to clear memory buffer and associates */
static void fm_clear_mem(struct filemem *restrict const fmp) {
    if (fmp->mem) {
        free(fmp->mem);
        fmp->mem = NULL;
    }
    fmp->memsize = fmp->mempos = fmp->memend = 0;
#ifdef FM_NO_VSNPRINTF
    if (fmp->fp_null) {
        (void) fclose(fmp->fp_null);
        fmp->fp_null = NULL;
    }
#endif
}

/* `fopen()` if file, setup memory buffer if BARCODE_MEMORY_FILE, returning 1 on success, 0 on failure */
INTERNAL int fm_open(struct filemem *restrict const fmp, struct zint_symbol *symbol, const char *mode) {
    assert(fmp && symbol && mode);
    fmp->fp = NULL;
    fmp->mem = NULL;
    fmp->memsize = fmp->mempos = fmp->memend = 0;
    fmp->flags = symbol->output_options & (BARCODE_STDOUT | BARCODE_MEMORY_FILE);
    fmp->err = 0;
#ifdef FM_NO_VSNPRINTF
    fmp->fp_null = NULL;
#endif

    if (fmp->flags & BARCODE_MEMORY_FILE) {
        if (!(fmp->mem = (unsigned char *) malloc(FM_PAGE_SIZE))) {
            return fm_seterr(fmp, ENOMEM);
        }
        fmp->memsize = FM_PAGE_SIZE;
        if (symbol->memfile) {
            free(symbol->memfile);
            symbol->memfile = NULL;
        }
        symbol->memfile_size = 0;
        return 1;
    }
    if (fmp->flags & BARCODE_STDOUT) {
#ifdef _MSC_VER
        if (strchr(mode, 'b') != NULL && _setmode(_fileno(stdout), _O_BINARY) == -1) {
            return fm_seterr(fmp, errno);
        }
#endif
        fmp->fp = stdout;
        return 1;
    }
    if (!(fmp->fp = out_fopen(symbol->outfile, mode))) {
        return fm_seterr(fmp, errno);
    }
    return 1;
}

/* Expand memory buffer, returning 1 on success, 0 on failure */
static int fm_mem_expand(struct filemem *restrict const fmp, const size_t size) {
    unsigned char *new_mem;
    size_t new_size;

    assert(fmp);
    if (!fmp->mem) {
        return fm_seterr(fmp, EINVAL);
    }
    if (size == 0) {
        return 1;
    }
    if (fmp->mempos + size < fmp->memsize) { /* Fits? */
        if (fmp->mempos + size <= fmp->mempos) { /* Check for overflow */
            fm_clear_mem(fmp);
            return fm_seterr(fmp, EOVERFLOW);
        }
        return 1;
    }
    new_size = fmp->memsize + (size < FM_PAGE_SIZE ? FM_PAGE_SIZE : size);
    if (new_size <= fmp->memsize) { /* Check for overflow */
        fm_clear_mem(fmp);
        return fm_seterr(fmp, EOVERFLOW);
    }
    /* Protect against very large files & (Linux) OOM killer - cf `raster_malloc()` in "raster.c" */
    if (new_size > 0x40000000 /*1GB*/ || !(new_mem = (unsigned char *) realloc(fmp->mem, new_size))) {
        fm_clear_mem(fmp);
        return fm_seterr(fmp, new_size > 0x40000000 ? EINVAL : ENOMEM);
    }
    fmp->mem = new_mem;
    fmp->memsize = new_size;
    return 1;
}

/* `fwrite()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_write(const void *restrict ptr, const size_t size, const size_t nitems,
                    struct filemem *restrict const fmp) {
    assert(fmp && ptr);
    if (fmp->err) {
        return 0;
    }
    if (size == 0 || nitems == 0) {
        return 1;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        const size_t tot_size = size * nitems;
        if (tot_size / size != nitems) {
            return fm_seterr(fmp, EOVERFLOW);
        }
        if (!fm_mem_expand(fmp, tot_size)) {
            return 0;
        }
        memcpy(fmp->mem + fmp->mempos, ptr, tot_size);
        fm_setpos(fmp, fmp->mempos + tot_size);
        return 1;
    }
    if (fwrite(ptr, size, nitems, fmp->fp) != nitems) {
        return fm_seterr(fmp, errno);
    }
    return 1;
}

/* `fputc()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_putc(const int ch, struct filemem *restrict const fmp) {
    assert(fmp);
    if (fmp->err) {
        return 0;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        if (!fm_mem_expand(fmp, 1)) {
            return 0;
        }
        fmp->mem[fmp->mempos] = (unsigned char) ch;
        fm_setpos(fmp, fmp->mempos + 1);
        return 1;
    }
    if (fputc(ch, fmp->fp) == EOF) {
        return fm_seterr(fmp, errno);
    }
    return 1;
}

/* `fputs()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_puts(const char *str, struct filemem *restrict const fmp) {
    assert(fmp);
    if (fmp->err) {
        return 0;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        const size_t len = strlen(str);
        if (!fm_mem_expand(fmp, len)) {
            return 0;
        }
        memcpy(fmp->mem + fmp->mempos, str, len);
        fm_setpos(fmp, fmp->mempos + len);
        return 1;
    }
    if (fputs(str, fmp->fp) == EOF) {
        return fm_seterr(fmp, errno);
    }
    return 1;
}

#ifdef FM_NO_VSNPRINTF
#  ifdef _WIN32
#    define DEV_NULL "NUL"
#  else
#    define DEV_NULL "/dev/null"
#  endif
#endif

/* Helper to `printf()` into mem buffer */
static int fm_vprintf(struct filemem *restrict const fmp, const char *fmt, va_list ap) {
    va_list cpy;
    int size, check;

    /* Adapted from https://stackoverflow.com/a/52558247/664741 */
#ifdef FM_NO_VSNPRINTF
    if (!fmp->fp_null && !(fmp->fp_null = fopen(DEV_NULL, "wb"))) {
        return fm_seterr(fmp, errno);
    }
#endif

    va_copy(cpy, ap);
    /* The clang-tidy warning is a bug https://github.com/llvm/llvm-project/issues/40656 */
#ifdef FM_NO_VSNPRINTF
    size = vfprintf(fmp->fp_null, fmt, cpy); /* NOLINT(clang-analyzer-valist.Uninitialized) */
#else
    size = vsnprintf(NULL, 0, fmt, cpy); /* NOLINT(clang-analyzer-valist.Uninitialized) */
#endif
    va_end(cpy);

    if (size < 0) {
        return fm_seterr(fmp, errno);
    }

    if (!fm_mem_expand(fmp, size + 1)) {
        return 0;
    }

#ifdef FM_NO_VSNPRINTF
    /* NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized) - see above */
    check = vsprintf((char *) fmp->mem + fmp->mempos, fmt, ap);
#else
    /* NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized) - see above */
    check = vsnprintf((char *) fmp->mem + fmp->mempos, size + 1, fmt, ap);
#endif

    (void)check;
    assert(check == size);

    fm_setpos(fmp, fmp->mempos + size);

    return 1;
}

/* `fprintf()` to file or memory, returning 1 on success, 0 on failure */
INTERNAL int fm_printf(struct filemem *restrict const fmp, const char *fmt, ...) {
    va_list ap;
    int ret;

    assert(fmp && fmt);
    if (fmp->err) {
        return 0;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        va_start(ap, fmt);
        ret = fm_vprintf(fmp, fmt, ap);
        va_end(ap);
        return ret;
    }
    va_start(ap, fmt);
    ret = vfprintf(fmp->fp, fmt, ap) >= 0; /* NOLINT(clang-analyzer-valist.Uninitialized) - see above */
    va_end(ap);
    return ret ? 1 : fm_seterr(fmp, errno);
}

/* Output float without trailing zeroes to `fmp` with decimal pts `dp` (precision), returning 1 on success, 0 on
   failure */
INTERNAL int fm_putsf(const char *prefix, const int dp, const float arg, struct filemem *restrict const fmp) {
    int i, end;
    char buf[256]; /* Assuming `dp` reasonable */
    const int len = sprintf(buf, "%.*f", dp, arg);

    assert(fmp);
    if (fmp->err) {
        return 0;
    }
    if (prefix && *prefix) {
        if (!fm_puts(prefix, fmp)) {
            return 0;
        }
    }

    /* Adapted from https://stackoverflow.com/a/36202854/664741 */
    for (i = len - 1, end = len; i >= 0; i--) {
        if (buf[i] == '0') {
            if (end == i + 1) {
                end = i;
            }
        } else if (!z_isdigit(buf[i]) && buf[i] != '-') { /* If not digit or minus then decimal point */
            if (end == i + 1) {
                end = i;
            } else {
                buf[i] = '.'; /* Overwrite any locale-specific setting for decimal point */
            }
            buf[end] = '\0';
            break;
        }
    }

    return fm_puts(buf, fmp);
}

/* `fclose()` if file, set `symbol->memfile` & `symbol->memfile_size` if memory, returning 1 on success, 0 on
   failure */
INTERNAL int fm_close(struct filemem *restrict const fmp, struct zint_symbol *symbol) {
    assert(fmp && symbol);
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        if (fmp->err || !fmp->mem) {
            fm_clear_mem(fmp);
            return fm_seterr(fmp, EINVAL);
        }
        symbol->memfile_size = (int) fmp->mempos;
        if ((size_t) symbol->memfile_size != fmp->mempos) {
            fm_clear_mem(fmp);
            symbol->memfile_size = 0;
            return fm_seterr(fmp, EINVAL);
        }
        symbol->memfile = fmp->mem;
        fmp->mem = NULL; /* Now belongs to `symbol` */
        fm_clear_mem(fmp);
        return 1;
    }
    if (fmp->err || !fmp->fp) {
        if (!(fmp->flags & BARCODE_STDOUT) && fmp->fp) {
            (void) fclose(fmp->fp);
        }
        return fm_seterr(fmp, EINVAL);
    }
    if (fmp->flags & BARCODE_STDOUT) {
        if (fflush(fmp->fp) != 0) {
            fmp->fp = NULL;
            return fm_seterr(fmp, errno);
        }
    } else {
        if (fclose(fmp->fp) != 0) {
            fmp->fp = NULL;
            return fm_seterr(fmp, errno);
        }
    }
    fmp->fp = NULL;
    return 1;
}

/* `fseek()` to file/memory offset, returning 1 if successful, 0 on failure */
INTERNAL int fm_seek(struct filemem *restrict const fmp, const long offset, const int whence) {
    assert(fmp);
    if (fmp->err) {
        return 0;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        const size_t start = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? fmp->mempos : fmp->memend;
        const size_t new_pos = start + offset;
        if ((offset > 0 && new_pos <= start) || (offset < 0 && new_pos >= start)) { /* Check for over/underflow */
            return fm_seterr(fmp, EINVAL);
        }
        if (!fm_mem_expand(fmp, new_pos)) {
            return 0;
        }
        fm_setpos(fmp, new_pos);
        return 1;
    }
    if (fseek(fmp->fp, offset, whence) != 0) {
        return fm_seterr(fmp, errno);
    }
    return 1;
}

/* `ftell()` returns current file/memory offset if successful, -1 on failure */
INTERNAL long fm_tell(struct filemem *restrict const fmp) {
    long ret;
    assert(fmp);
    if (fmp->err) {
        return -1;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        if (!fmp->mem) {
            (void) fm_seterr(fmp, ENOMEM);
            return -1;
        }
        return (long) fmp->mempos;
    }
    ret = ftell(fmp->fp);
    /* On many Linux distros `ftell()` returns LONG_MAX not -1 on error */
    if (ret < 0 || ret == LONG_MAX) {
        (void) fm_seterr(fmp, errno);
        return -1;
    }
    return ret;
}

/* Return `err`, which uses `errno` values; if file and `err` not set, test `ferror()` also */
INTERNAL int fm_error(struct filemem *restrict const fmp) {
    assert(fmp);
    if (fmp->err == 0 && !(fmp->flags & BARCODE_MEMORY_FILE) && ferror(fmp->fp)) {
        (void) fm_seterr(fmp, EIO);
    }
    return fmp->err;
}

/* `fflush()` if file, no-op (apart from error checking) if memory, returning 1 on success, 0 on failure
   NOTE: don't use, included only for libpng compatibility */
INTERNAL int fm_flush(struct filemem *restrict const fmp) {
    assert(fmp);
    if (fmp->err) {
        return 0;
    }
    if (fmp->flags & BARCODE_MEMORY_FILE) {
        if (!fmp->mem) {
            return fm_seterr(fmp, EINVAL);
        }
        return 1;
    }
    if (fflush(fmp->fp) == EOF) {
        return fm_seterr(fmp, errno);
    }
    return 1;
}

/* vim: set ts=4 sw=4 et : */
