#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef HAVE_SYS_SDT_H
#include <sys/sdt.h>
#endif

/*
   Tuning parameters.
 */
#ifndef DFLTCC_LEVEL_MASK
#define DFLTCC_LEVEL_MASK 0x2
#endif
#ifndef DFLTCC_BLOCK_SIZE
#define DFLTCC_BLOCK_SIZE 1048576
#endif
#ifndef DFLTCC_FIRST_FHT_BLOCK_SIZE
#define DFLTCC_FIRST_FHT_BLOCK_SIZE 4096
#endif
#ifndef DFLTCC_DHT_MIN_SAMPLE_SIZE
#define DFLTCC_DHT_MIN_SAMPLE_SIZE 4096
#endif
#ifndef DFLTCC_RIBM
#define DFLTCC_RIBM 0
#endif

/*
   C wrapper for the DEFLATE CONVERSION CALL instruction.
 */
typedef enum {
    DFLTCC_CC_OK = 0,
    DFLTCC_CC_OP1_TOO_SHORT = 1,
    DFLTCC_CC_OP2_TOO_SHORT = 2,
    DFLTCC_CC_OP2_CORRUPT = 2,
    DFLTCC_CC_AGAIN = 3,
} dfltcc_cc;

#define DFLTCC_QAF 0
#define DFLTCC_GDHT 1
#define DFLTCC_CMPR 2
#define DFLTCC_XPND 4
#define HBT_CIRCULAR (1 << 7)
#define HB_BITS 15
#define HB_SIZE (1 << HB_BITS)
#define DFLTCC_FACILITY 151

static inline dfltcc_cc dfltcc(int fn, void *param,
                               unsigned char **op1, size_t *len1, z_const unsigned char **op2, size_t *len2, void *hist) {
    unsigned char *t2 = op1 ? *op1 : NULL;
    size_t t3 = len1 ? *len1 : 0;
    z_const unsigned char *t4 = op2 ? *op2 : NULL;
    size_t t5 = len2 ? *len2 : 0;
    Z_REGISTER int r0 __asm__("r0") = fn;
    Z_REGISTER void *r1 __asm__("r1") = param;
    Z_REGISTER unsigned char *r2 __asm__("r2") = t2;
    Z_REGISTER size_t r3 __asm__("r3") = t3;
    Z_REGISTER z_const unsigned char *r4 __asm__("r4") = t4;
    Z_REGISTER size_t r5 __asm__("r5") = t5;
    int cc;

    __asm__ volatile(
#ifdef HAVE_SYS_SDT_H
                     STAP_PROBE_ASM(zlib, dfltcc_entry, STAP_PROBE_ASM_TEMPLATE(5))
#endif
                     ".insn rrf,0xb9390000,%[r2],%[r4],%[hist],0\n"
#ifdef HAVE_SYS_SDT_H
                     STAP_PROBE_ASM(zlib, dfltcc_exit, STAP_PROBE_ASM_TEMPLATE(5))
#endif
                     "ipm %[cc]\n"
                     : [r2] "+r" (r2)
                     , [r3] "+r" (r3)
                     , [r4] "+r" (r4)
                     , [r5] "+r" (r5)
                     , [cc] "=r" (cc)
                     : [r0] "r" (r0)
                     , [r1] "r" (r1)
                     , [hist] "r" (hist)
#ifdef HAVE_SYS_SDT_H
                     , STAP_PROBE_ASM_OPERANDS(5, r2, r3, r4, r5, hist)
#endif
                     : "cc", "memory");
    t2 = r2; t3 = r3; t4 = r4; t5 = r5;

    if (op1)
        *op1 = t2;
    if (len1)
        *len1 = t3;
    if (op2)
        *op2 = t4;
    if (len2)
        *len2 = t5;
    return (cc >> 28) & 3;
}

/*
   Parameter Block for Query Available Functions.
 */
#define static_assert(c, msg) __attribute__((unused)) static char static_assert_failed_ ## msg[c ? 1 : -1]

struct dfltcc_qaf_param {
    char fns[16];
    char reserved1[8];
    char fmts[2];
    char reserved2[6];
};

static_assert(sizeof(struct dfltcc_qaf_param) == 32, sizeof_struct_dfltcc_qaf_param_is_32);

static inline int is_bit_set(const char *bits, int n) {
    return bits[n / 8] & (1 << (7 - (n % 8)));
}

static inline void clear_bit(char *bits, int n) {
    bits[n / 8] &= ~(1 << (7 - (n % 8)));
}

#define DFLTCC_FMT0 0

/*
   Parameter Block for Generate Dynamic-Huffman Table, Compress and Expand.
 */
#define CVT_CRC32 0
#define CVT_ADLER32 1
#define HTT_FIXED 0
#define HTT_DYNAMIC 1

struct dfltcc_param_v0 {
    uint16_t pbvn;                     /* Parameter-Block-Version Number */
    uint8_t mvn;                       /* Model-Version Number */
    uint8_t ribm;                      /* Reserved for IBM use */
    uint32_t reserved32 : 31;
    uint32_t cf : 1;                   /* Continuation Flag */
    uint8_t reserved64[8];
    uint32_t nt : 1;                   /* New Task */
    uint32_t reserved129 : 1;
    uint32_t cvt : 1;                  /* Check Value Type */
    uint32_t reserved131 : 1;
    uint32_t htt : 1;                  /* Huffman-Table Type */
    uint32_t bcf : 1;                  /* Block-Continuation Flag */
    uint32_t bcc : 1;                  /* Block Closing Control */
    uint32_t bhf : 1;                  /* Block Header Final */
    uint32_t reserved136 : 1;
    uint32_t reserved137 : 1;
    uint32_t dhtgc : 1;                /* DHT Generation Control */
    uint32_t reserved139 : 5;
    uint32_t reserved144 : 5;
    uint32_t sbb : 3;                  /* Sub-Byte Boundary */
    uint8_t oesc;                      /* Operation-Ending-Supplemental Code */
    uint32_t reserved160 : 12;
    uint32_t ifs : 4;                  /* Incomplete-Function Status */
    uint16_t ifl;                      /* Incomplete-Function Length */
    uint8_t reserved192[8];
    uint8_t reserved256[8];
    uint8_t reserved320[4];
    uint16_t hl;                       /* History Length */
    uint32_t reserved368 : 1;
    uint16_t ho : 15;                  /* History Offset */
    uint32_t cv;                       /* Check Value */
    uint32_t eobs : 15;                /* End-of-block Symbol */
    uint32_t reserved431: 1;
    uint8_t eobl : 4;                  /* End-of-block Length */
    uint32_t reserved436 : 12;
    uint32_t reserved448 : 4;
    uint16_t cdhtl : 12;               /* Compressed-Dynamic-Huffman Table
                                          Length */
    uint8_t reserved464[6];
    uint8_t cdht[288];
    uint8_t reserved[32];
    uint8_t csb[1152];
};

static_assert(sizeof(struct dfltcc_param_v0) == 1536, sizeof_struct_dfltcc_param_v0_is_1536);

static inline z_const char *oesc_msg(char *buf, int oesc) {
    if (oesc == 0x00)
        return NULL; /* Successful completion */
    else {
        sprintf(buf, "Operation-Ending-Supplemental Code is 0x%.2X", oesc);
        return buf;
    }
}

/*
   Extension of inflate_state and deflate_state. Must be doubleword-aligned.
*/
struct dfltcc_state {
    struct dfltcc_param_v0 param;      /* Parameter block. */
    struct dfltcc_qaf_param af;        /* Available functions. */
    uint16_t level_mask;               /* Levels on which to use DFLTCC */
    uint32_t block_size;               /* New block each X bytes */
    size_t block_threshold;            /* New block after total_in > X */
    uint32_t dht_threshold;            /* New block only if avail_in >= X */
    char msg[64];                      /* Buffer for strm->msg */
};

#define ALIGN_UP(p, size) (__typeof__(p))(((uintptr_t)(p) + ((size) - 1)) & ~((size) - 1))

#define GET_DFLTCC_STATE(state) ((struct dfltcc_state *)((char *)(state) + ALIGN_UP(sizeof(*state), 8)))
