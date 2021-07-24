/* chunkset_avx.c -- AVX inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "zbuild.h"
#include "zutil.h"

#ifdef X86_AVX_CHUNKSET
#include <immintrin.h>

typedef __m256i chunk_t;

#define CHUNK_SIZE 32

#define HAVE_CHUNKMEMSET_1
#define HAVE_CHUNKMEMSET_2
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8

static inline void chunkmemset_1(uint8_t *from, chunk_t *chunk) {
    *chunk = _mm256_set1_epi8(*(int8_t *)from);
}

static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    *chunk = _mm256_set1_epi16(*(int16_t *)from);
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    *chunk = _mm256_set1_epi32(*(int32_t *)from);
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
    *chunk = _mm256_set1_epi64x(*(int64_t *)from);
}

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    *chunk = _mm256_loadu_si256((__m256i *)s);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    _mm256_storeu_si256((__m256i *)out, *chunk);
}

#define CHUNKSIZE        chunksize_avx
#define CHUNKCOPY        chunkcopy_avx
#define CHUNKCOPY_SAFE   chunkcopy_safe_avx
#define CHUNKUNROLL      chunkunroll_avx
#define CHUNKMEMSET      chunkmemset_avx
#define CHUNKMEMSET_SAFE chunkmemset_safe_avx

#include "chunkset_tpl.h"

#endif
