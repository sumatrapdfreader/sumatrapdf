/* chunkset_power8.c -- VSX inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef POWER8_VSX_CHUNKSET
#include <altivec.h>
#include "zbuild.h"
#include "zutil.h"

typedef vector unsigned char chunk_t;

#define CHUNK_SIZE 16

#define HAVE_CHUNKMEMSET_1
#define HAVE_CHUNKMEMSET_2
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8

static inline void chunkmemset_1(uint8_t *from, chunk_t *chunk) {
    *chunk = vec_splats(*from);
}

static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    uint16_t tmp;
    memcpy(&tmp, from, 2);
    *chunk = (vector unsigned char)vec_splats(tmp);
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    uint32_t tmp;
    memcpy(&tmp, from, 4);
    *chunk = (vector unsigned char)vec_splats(tmp);
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
    uint64_t tmp;
    memcpy(&tmp, from, 8);
    *chunk = (vector unsigned char)vec_splats(tmp);
}

#define CHUNKSIZE        chunksize_power8
#define CHUNKCOPY        chunkcopy_power8
#define CHUNKCOPY_SAFE   chunkcopy_safe_power8
#define CHUNKUNROLL      chunkunroll_power8
#define CHUNKMEMSET      chunkmemset_power8
#define CHUNKMEMSET_SAFE chunkmemset_safe_power8

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    *chunk = vec_xl(0, s);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    vec_xst(*chunk, 0, out);
}

#include "chunkset_tpl.h"

#endif
