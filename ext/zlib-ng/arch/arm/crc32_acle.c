/* crc32_acle.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
*/

#ifdef ARM_ACLE_CRC_HASH
#ifndef _MSC_VER
#  include <arm_acle.h>
#endif
#include "../../zutil.h"

uint32_t crc32_acle(uint32_t crc, const unsigned char *buf, uint64_t len) {
    Z_REGISTER uint32_t c;
    Z_REGISTER const uint16_t *buf2;
    Z_REGISTER const uint32_t *buf4;

    c = ~crc;
    if (len && ((ptrdiff_t)buf & 1)) {
        c = __crc32b(c, *buf++);
        len--;
    }

    if ((len > sizeof(uint16_t)) && ((ptrdiff_t)buf & sizeof(uint16_t))) {
        buf2 = (const uint16_t *) buf;
        c = __crc32h(c, *buf2++);
        len -= sizeof(uint16_t);
        buf4 = (const uint32_t *) buf2;
    } else {
        buf4 = (const uint32_t *) buf;
    }

#if defined(__aarch64__)
    if ((len > sizeof(uint32_t)) && ((ptrdiff_t)buf & sizeof(uint32_t))) {
        c = __crc32w(c, *buf4++);
        len -= sizeof(uint32_t);
    }

    const uint64_t *buf8 = (const uint64_t *) buf4;

#ifdef UNROLL_MORE
    while (len >= 4 * sizeof(uint64_t)) {
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        len -= 4 * sizeof(uint64_t);
    }
#endif

    while (len >= sizeof(uint64_t)) {
        c = __crc32d(c, *buf8++);
        len -= sizeof(uint64_t);
    }

    if (len >= sizeof(uint32_t)) {
        buf4 = (const uint32_t *) buf8;
        c = __crc32w(c, *buf4++);
        len -= sizeof(uint32_t);
        buf2 = (const uint16_t *) buf4;
    } else {
        buf2 = (const uint16_t *) buf8;
    }

    if (len >= sizeof(uint16_t)) {
        c = __crc32h(c, *buf2++);
        len -= sizeof(uint16_t);
    }

    buf = (const unsigned char *) buf2;
#else /* __aarch64__ */

#  ifdef UNROLL_MORE
    while (len >= 8 * sizeof(uint32_t)) {
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        len -= 8 * sizeof(uint32_t);
    }
#  endif

    while (len >= sizeof(uint32_t)) {
        c = __crc32w(c, *buf4++);
        len -= sizeof(uint32_t);
    }

    if (len >= sizeof(uint16_t)) {
        buf2 = (const uint16_t *) buf4;
        c = __crc32h(c, *buf2++);
        len -= sizeof(uint16_t);
        buf = (const unsigned char *) buf2;
    } else {
        buf = (const unsigned char *) buf4;
    }
#endif /* __aarch64__ */

    if (len) {
        c = __crc32b(c, *buf);
    }

    c = ~c;
    return c;
}
#endif
