/* insert_string_sse -- insert_string variant using SSE4.2's CRC instructions
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "../../zbuild.h"
#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif
#include "../../deflate.h"

#ifdef X86_SSE42_CRC_INTRIN
#  ifdef _MSC_VER
#    define UPDATE_HASH(s, h, val)\
        h = _mm_crc32_u32(h, val)
#  else
#    define UPDATE_HASH(s, h, val)\
        h = __builtin_ia32_crc32si(h, val)
#  endif
#else
#  ifdef _MSC_VER
#    define UPDATE_HASH(s, h, val) {\
        __asm mov edx, h\
        __asm mov eax, val\
        __asm crc32 eax, edx\
        __asm mov val, eax\
    }
#  else
#    define UPDATE_HASH(s, h, val) \
        __asm__ __volatile__ (\
            "crc32 %1,%0\n\t"\
            : "+r" (h)\
            : "r" (val)\
        );
#  endif
#endif

#define INSERT_STRING       insert_string_sse4
#define QUICK_INSERT_STRING quick_insert_string_sse4

#ifdef X86_SSE42_CRC_HASH
#  include "../../insert_string_tpl.h"
#endif
