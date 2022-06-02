/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "../../adler32_p.h"

#include <immintrin.h>

#ifdef X86_AVX2_ADLER32

Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const unsigned char *buf, size_t len) {
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

    uint32_t ALIGNED_(32) s1[8], s2[8];

    memset(s1, 0, sizeof(s1)); s1[7] = adler; // TODO: would a masked load be faster?
    memset(s2, 0, sizeof(s2)); s2[7] = sum2;

    char ALIGNED_(32) dot1[32] = \
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    __m256i dot1v = _mm256_load_si256((__m256i*)dot1);
    char ALIGNED_(32) dot2[32] = \
        {32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
         16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    __m256i dot2v = _mm256_load_si256((__m256i*)dot2);
    short ALIGNED_(32) dot3[16] = \
        {1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1};
    __m256i dot3v = _mm256_load_si256((__m256i*)dot3);

    // We will need to multiply by
    char ALIGNED_(32) shift[16] = {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    __m128i shiftv = _mm_load_si128((__m128i*)shift);

    while (len >= 32) {
       __m256i vs1 = _mm256_load_si256((__m256i*)s1);
       __m256i vs2 = _mm256_load_si256((__m256i*)s2);
       __m256i vs1_0 = vs1;

       int k = (len < NMAX ? (int)len : NMAX);
       k -= k % 32;
       len -= k;

       while (k >= 32) {
           /*
              vs1 = adler + sum(c[i])
              vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )
           */
           __m256i vbuf = _mm256_loadu_si256((__m256i*)buf);
           buf += 32;
           k -= 32;

           __m256i v_short_sum1 = _mm256_maddubs_epi16(vbuf, dot1v); // multiply-add, resulting in 8 shorts.
           __m256i vsum1 = _mm256_madd_epi16(v_short_sum1, dot3v);   // sum 8 shorts to 4 int32_t;
           __m256i v_short_sum2 = _mm256_maddubs_epi16(vbuf, dot2v);
           vs1 = _mm256_add_epi32(vsum1, vs1);
           __m256i vsum2 = _mm256_madd_epi16(v_short_sum2, dot3v);
           vs1_0 = _mm256_sll_epi32(vs1_0, shiftv);
           vsum2 = _mm256_add_epi32(vsum2, vs2);
           vs2   = _mm256_add_epi32(vsum2, vs1_0);
           vs1_0 = vs1;
       }

       // At this point, we have partial sums stored in vs1 and vs2.  There are AVX512 instructions that
       // would allow us to sum these quickly (VP4DPWSSD).  For now, just unpack and move on.
       uint32_t ALIGNED_(32) s1_unpack[8];
       uint32_t ALIGNED_(32) s2_unpack[8];

       _mm256_store_si256((__m256i*)s1_unpack, vs1);
       _mm256_store_si256((__m256i*)s2_unpack, vs2);

       adler = (s1_unpack[0] % BASE) + (s1_unpack[1] % BASE) + (s1_unpack[2] % BASE) + (s1_unpack[3] % BASE) +
               (s1_unpack[4] % BASE) + (s1_unpack[5] % BASE) + (s1_unpack[6] % BASE) + (s1_unpack[7] % BASE);
       adler %= BASE;
       s1[7] = adler;

       sum2 = (s2_unpack[0] % BASE) + (s2_unpack[1] % BASE) + (s2_unpack[2] % BASE) + (s2_unpack[3] % BASE) +
              (s2_unpack[4] % BASE) + (s2_unpack[5] % BASE) + (s2_unpack[6] % BASE) + (s2_unpack[7] % BASE);
       sum2 %= BASE;
       s2[7] = sum2;
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
