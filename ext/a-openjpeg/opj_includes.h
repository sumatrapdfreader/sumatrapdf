#ifndef OPJ_INCLUDES_H
#define OPJ_INCLUDES_H

#include "opj_config_private.h"

#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#if defined(OPJ_HAVE_FSEEKO) && !defined(fseek)
#  define fseek  fseeko
#  define ftell  ftello
#endif

#if defined(WIN32) && !defined(Windows95) && !defined(__BORLANDC__) && \
  !(defined(_MSC_VER) && _MSC_VER < 1400) && \
  !(defined(__MINGW32__) && __MSVCRT_VERSION__ < 0x800)

#  define OPJ_FSEEK(stream,offset,whence) _fseeki64(stream, offset,whence)
#  define OPJ_FSTAT(fildes,stat_buff) _fstati64(fildes, stat_buff)
#  define OPJ_FTELL(stream)  _ftelli64(stream)
#  define OPJ_STAT_STRUCT_T struct _stati64
#  define OPJ_STAT(path,stat_buff) _stati64(path, stat_buff)
#else
#  define OPJ_FSEEK(stream,offset,whence) fseek(stream,offset,whence)
#  define OPJ_FSTAT(fildes,stat_buff) fstat(fildes,stat_buff)
#  define OPJ_FTELL(stream) ftell(stream)
#  define OPJ_STAT_STRUCT_T struct stat
#  define OPJ_STAT(path,stat_buff) stat(path,stat_buff)
#endif

#include "openjpeg.h"

#if (__STDC_VERSION__ >= 199901L)
#define OPJ_RESTRICT restrict
#else

#if defined(__GNUC__)
#define OPJ_RESTRICT __restrict__

#else
#define OPJ_RESTRICT
#endif
#endif

#ifdef __has_attribute
#if __has_attribute(no_sanitize)
#define OPJ_NOSANITIZE(kind) __attribute__((no_sanitize(kind)))
#endif
#endif
#ifndef OPJ_NOSANITIZE
#define OPJ_NOSANITIZE(kind)
#endif

#if defined(_MSC_VER)
#include <intrin.h>
static INLINE long opj_lrintf(float f)
{
#ifdef _M_X64
    return _mm_cvt_ss2si(_mm_load_ss(&f));

#elif defined(_M_IX86)
    int i;
    _asm{
        fld f
        fistp i
    };

    return i;
#else
    return (long)((f>0.0f) ? (f + 0.5f) : (f - 0.5f));
#endif
}
#elif defined(__BORLANDC__)
static INLINE long opj_lrintf(float f)
{
#ifdef _M_X64
    return (long)((f > 0.0f) ? (f + 0.5f) : (f - 0.5f));
#else
    int i;

    _asm {
        fld f
        fistp i
    };

    return i;
#endif
}
#else
static INLINE long opj_lrintf(float f)
{
    return lrintf(f);
}
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1400)
#define vsnprintf _vsnprintf
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__INTEL_COMPILER) && defined(_M_IX86)
#   include <intrin.h>
#   pragma intrinsic(__emul)
#endif

#if defined(_M_X64)

#   ifndef __SSE__
#       define __SSE__ 1
#   endif
#   ifndef __SSE2__
#       define __SSE2__ 1
#   endif
#endif

#if defined(_M_IX86_FP)
#   if _M_IX86_FP >= 1
#       ifndef __SSE__
#           define __SSE__ 1
#       endif
#   endif
#   if _M_IX86_FP >= 2
#       ifndef __SSE2__
#           define __SSE2__ 1
#       endif
#   endif
#endif

typedef unsigned int OPJ_BITFIELD;

#define OPJ_UNUSED(x) (void)x

#include "opj_clock.h"
#include "opj_malloc.h"
#include "event.h"
#include "function_list.h"
#include "bio.h"
#include "cio.h"

#include "thread.h"
#include "tls_keys.h"

#include "image.h"
#include "invert.h"
#include "j2k.h"
#include "jp2.h"

#include "mqc.h"
#include "bio.h"

#include "pi.h"
#include "tgt.h"
#include "tcd.h"
#include "t1.h"
#include "dwt.h"
#include "t2.h"
#include "mct.h"
#include "opj_intmath.h"
#include "sparse_array.h"

#ifdef USE_JPIP
#include "cidx_manager.h"
#include "indexbox_manager.h"
#endif

#ifdef USE_JPWL
#include "openjpwl/jpwl.h"
#endif

#include "opj_codec.h"

#endif
