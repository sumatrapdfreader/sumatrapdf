/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "../../adler32_p.h"

#ifdef X86_SSSE3_ADLER32

#include <immintrin.h>

Z_INTERNAL uint32_t adler32_ssse3(uint32_t adler, const unsigned char *buf, size_t len) {
    uint32_t sum2;

     /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_len_1(adler, buf, sum2);

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (UNLIKELY(buf == NULL))
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_len_16(adler, buf, len, sum2);

    uint32_t ALIGNED_(16) s1[4], s2[4];

    s1[0] = s1[1] = s1[2] = 0; s1[3] = adler;
    s2[0] = s2[1] = s2[2] = 0; s2[3] = sum2;

    char ALIGNED_(16) dot1[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    __m128i dot1v = _mm_load_si128((__m128i*)dot1);
    char ALIGNED_(16) dot2[16] = {16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    __m128i dot2v = _mm_load_si128((__m128i*)dot2);
    short ALIGNED_(16) dot3[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    __m128i dot3v = _mm_load_si128((__m128i*)dot3);

    // We will need to multiply by
    //char ALIGNED_(16) shift[4] = {0, 0, 0, 4}; //{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4};

    char ALIGNED_(16) shift[16] = {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    __m128i shiftv = _mm_load_si128((__m128i*)shift);

    while (len >= 16) {
       __m128i vs1 = _mm_load_si128((__m128i*)s1);
       __m128i vs2 = _mm_load_si128((__m128i*)s2);
       __m128i vs1_0 = vs1;

       int k = (len < NMAX ? (int)len : NMAX);
       k -= k % 16;
       len -= k;

       while (k >= 16) {
           /*
              vs1 = adler + sum(c[i])
              vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )

              NOTE: 256-bit equivalents are:
                _mm256_maddubs_epi16 <- operates on 32 bytes to 16 shorts
                _mm256_madd_epi16    <- Sums 16 shorts to 8 int32_t.
              We could rewrite the below to use 256-bit instructions instead of 128-bit.
           */
           __m128i vbuf = _mm_loadu_si128((__m128i*)buf);
           buf += 16;
           k -= 16;

           __m128i v_short_sum1 = _mm_maddubs_epi16(vbuf, dot1v); // multiply-add, resulting in 8 shorts.
           __m128i vsum1 = _mm_madd_epi16(v_short_sum1, dot3v);  // sum 8 shorts to 4 int32_t;
           __m128i v_short_sum2 = _mm_maddubs_epi16(vbuf, dot2v);
           vs1 = _mm_add_epi32(vsum1, vs1);
           __m128i vsum2 = _mm_madd_epi16(v_short_sum2, dot3v);
           vs1_0 = _mm_sll_epi32(vs1_0, shiftv);
           vsum2 = _mm_add_epi32(vsum2, vs2);
           vs2   = _mm_add_epi32(vsum2, vs1_0);
           vs1_0 = vs1;
       }

       // At this point, we have partial sums stored in vs1 and vs2.  There are AVX512 instructions that
       // would allow us to sum these quickly (VP4DPWSSD).  For now, just unpack and move on.

       uint32_t ALIGNED_(16) s1_unpack[4];
       uint32_t ALIGNED_(16) s2_unpack[4];

       _mm_store_si128((__m128i*)s1_unpack, vs1);
       _mm_store_si128((__m128i*)s2_unpack, vs2);

       adler = (s1_unpack[0] % BASE) + (s1_unpack[1] % BASE) + (s1_unpack[2] % BASE) + (s1_unpack[3] % BASE);
       adler %= BASE;
       s1[3] = adler;

       sum2 = (s2_unpack[0] % BASE) + (s2_unpack[1] % BASE) + (s2_unpack[2] % BASE) + (s2_unpack[3] % BASE);
       sum2 %= BASE;
       s2[3] = sum2;
    }

    while (len) {
        len--;
        adler += *buf++;
        sum2 += adler;
    }
    adler %= BASE;
    sum2 %= BASE;

    /* return recombined sums */
    return adler | (sum2 << 16);
}

#endif
