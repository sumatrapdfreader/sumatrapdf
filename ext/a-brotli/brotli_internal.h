#include "brotli.h"
#ifndef BROTLI_COMMON_CONSTANTS_H_
#define BROTLI_COMMON_CONSTANTS_H_

#ifndef BROTLI_COMMON_PLATFORM_H_
#define BROTLI_COMMON_PLATFORM_H_

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#if BROTLI_MSVC_VERSION_CHECK(18, 0, 0)
#include <intrin.h>
#endif

#if defined(BROTLI_ENABLE_LOG) || defined(BROTLI_DEBUG)
#include <assert.h>
#include <stdio.h>
#endif

#if BROTLI_GNUC_HAS_BUILTIN(__builtin_expect, 3, 0, 0) || \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0) ||               \
    BROTLI_SUNPRO_VERSION_CHECK(5, 15, 0) ||              \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) ||                  \
    BROTLI_IBM_VERSION_CHECK(10, 1, 0) ||                 \
    BROTLI_TI_VERSION_CHECK(7, 3, 0) ||                   \
    BROTLI_TINYC_VERSION_CHECK(0, 9, 27)
#define BROTLI_PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))
#define BROTLI_PREDICT_FALSE(x) (__builtin_expect(x, 0))
#else
#define BROTLI_PREDICT_FALSE(x) (x)
#define BROTLI_PREDICT_TRUE(x) (x)
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
    !defined(__cplusplus)
#define BROTLI_RESTRICT restrict
#elif BROTLI_GNUC_VERSION_CHECK(3, 1, 0) ||                         \
    BROTLI_MSVC_VERSION_CHECK(14, 0, 0) ||                          \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0) ||                         \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) ||                            \
    BROTLI_IBM_VERSION_CHECK(10, 1, 0) ||                           \
    BROTLI_PGI_VERSION_CHECK(17, 10, 0) ||                          \
    BROTLI_TI_VERSION_CHECK(8, 0, 0) ||                             \
    BROTLI_IAR_VERSION_CHECK(8, 0, 0) ||                            \
    (BROTLI_SUNPRO_VERSION_CHECK(5, 14, 0) && defined(__cplusplus))
#define BROTLI_RESTRICT __restrict
#elif BROTLI_SUNPRO_VERSION_CHECK(5, 3, 0) && !defined(__cplusplus)
#define BROTLI_RESTRICT _Restrict
#else
#define BROTLI_RESTRICT
#endif

#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
    (defined(__cplusplus) && (__cplusplus >= 199711L))
#define BROTLI_MAYBE_INLINE inline
#elif defined(__GNUC_STDC_INLINE__) || defined(__GNUC_GNU_INLINE__) || \
    BROTLI_ARM_VERSION_CHECK(6, 2, 0)
#define BROTLI_MAYBE_INLINE __inline__
#elif BROTLI_MSVC_VERSION_CHECK(12, 0, 0) || \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) || BROTLI_TI_VERSION_CHECK(8, 0, 0)
#define BROTLI_MAYBE_INLINE __inline
#else
#define BROTLI_MAYBE_INLINE
#endif

#if BROTLI_GNUC_HAS_ATTRIBUTE(always_inline, 4, 0, 0) ||                       \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0) ||                                    \
    BROTLI_SUNPRO_VERSION_CHECK(5, 11, 0) ||                                   \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) ||                                       \
    BROTLI_IBM_VERSION_CHECK(10, 1, 0) ||                                      \
    BROTLI_TI_VERSION_CHECK(8, 0, 0) ||                                        \
    (BROTLI_TI_VERSION_CHECK(7, 3, 0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__))
#define BROTLI_INLINE BROTLI_MAYBE_INLINE __attribute__((__always_inline__))
#elif BROTLI_MSVC_VERSION_CHECK(12, 0, 0)
#define BROTLI_INLINE BROTLI_MAYBE_INLINE __forceinline
#elif BROTLI_TI_VERSION_CHECK(7, 0, 0) && defined(__cplusplus)
#define BROTLI_INLINE BROTLI_MAYBE_INLINE _Pragma("FUNC_ALWAYS_INLINE;")
#elif BROTLI_IAR_VERSION_CHECK(8, 0, 0)
#define BROTLI_INLINE BROTLI_MAYBE_INLINE _Pragma("inline=forced")
#else
#define BROTLI_INLINE BROTLI_MAYBE_INLINE
#endif

#if BROTLI_GNUC_HAS_ATTRIBUTE(noinline, 4, 0, 0) ||                            \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0) ||                                    \
    BROTLI_SUNPRO_VERSION_CHECK(5, 11, 0) ||                                   \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) ||                                       \
    BROTLI_IBM_VERSION_CHECK(10, 1, 0) ||                                      \
    BROTLI_TI_VERSION_CHECK(8, 0, 0) ||                                        \
    (BROTLI_TI_VERSION_CHECK(7, 3, 0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__))
#define BROTLI_NOINLINE __attribute__((__noinline__))
#elif BROTLI_MSVC_VERSION_CHECK(13, 10, 0)
#define BROTLI_NOINLINE __declspec(noinline)
#elif BROTLI_PGI_VERSION_CHECK(10, 2, 0)
#define BROTLI_NOINLINE _Pragma("noinline")
#elif BROTLI_TI_VERSION_CHECK(6, 0, 0) && defined(__cplusplus)
#define BROTLI_NOINLINE _Pragma("FUNC_CANNOT_INLINE;")
#elif BROTLI_IAR_VERSION_CHECK(8, 0, 0)
#define BROTLI_NOINLINE _Pragma("inline=never")
#else
#define BROTLI_NOINLINE
#endif

#if BROTLI_GNUC_HAS_ATTRIBUTE(unused, 2, 7, 0) || \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0)
#define BROTLI_UNUSED_FUNCTION static BROTLI_INLINE __attribute__ ((unused))
#else
#define BROTLI_UNUSED_FUNCTION static BROTLI_INLINE
#endif

#if BROTLI_GNUC_HAS_ATTRIBUTE(aligned, 2, 7, 0)
#define BROTLI_ALIGNED(N) __attribute__((aligned(N)))
#else
#define BROTLI_ALIGNED(N)
#endif

#if (defined(__ARM_ARCH) && (__ARM_ARCH == 7)) || \
    (defined(M_ARM) && (M_ARM == 7))
#define BROTLI_TARGET_ARMV7
#endif

#if (defined(__ARM_ARCH) && (__ARM_ARCH == 8)) || \
    defined(__aarch64__) || defined(__ARM64_ARCH_8__)
#define BROTLI_TARGET_ARMV8_ANY

#if defined(__ARM_32BIT_STATE)
#define BROTLI_TARGET_ARMV8_32
#elif defined(__ARM_64BIT_STATE)
#define BROTLI_TARGET_ARMV8_64
#endif

#endif

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#define BROTLI_TARGET_NEON
#endif

#if defined(__i386) || defined(_M_IX86)
#define BROTLI_TARGET_X86
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define BROTLI_TARGET_X64
#endif

#if defined(__PPC64__)
#define BROTLI_TARGET_POWERPC64
#endif

#if defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
#define BROTLI_TARGET_RISCV64
#endif

#if defined(__loongarch_lp64)
#define BROTLI_TARGET_LOONGARCH64
#endif

#if defined(__s390x__)
#define BROTLI_TARGET_S390X
#endif

#if defined(__mips64)
#define BROTLI_TARGET_MIPS64
#endif

#if defined(__ia64__) || defined(_M_IA64)
#define BROTLI_TARGET_IA64
#endif

#if defined(BROTLI_TARGET_X64) || defined(BROTLI_TARGET_ARMV8_64) || \
    defined(BROTLI_TARGET_POWERPC64) || defined(BROTLI_TARGET_RISCV64) || \
    defined(BROTLI_TARGET_LOONGARCH64) || defined(BROTLI_TARGET_MIPS64)
#define BROTLI_TARGET_64_BITS 1
#else
#define BROTLI_TARGET_64_BITS 0
#endif

#if defined(BROTLI_BUILD_64_BIT)
#define BROTLI_64_BITS 1
#elif defined(BROTLI_BUILD_32_BIT)
#define BROTLI_64_BITS 0
#else
#define BROTLI_64_BITS BROTLI_TARGET_64_BITS
#endif

#if (BROTLI_64_BITS)
#define brotli_reg_t uint64_t
#else
#define brotli_reg_t uint32_t
#endif

#if defined(BROTLI_BUILD_BIG_ENDIAN)
#define BROTLI_BIG_ENDIAN 1
#elif defined(BROTLI_BUILD_LITTLE_ENDIAN)
#define BROTLI_LITTLE_ENDIAN 1
#elif defined(BROTLI_BUILD_ENDIAN_NEUTRAL)

#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define BROTLI_LITTLE_ENDIAN 1
#elif defined(_WIN32) || defined(BROTLI_TARGET_X64)

#define BROTLI_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BROTLI_BIG_ENDIAN 1

#elif defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
#define BROTLI_LITTLE_ENDIAN 1
#elif defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
#define BROTLI_BIG_ENDIAN 1
#endif

#if !defined(BROTLI_LITTLE_ENDIAN)
#define BROTLI_LITTLE_ENDIAN 0
#endif

#if !defined(BROTLI_BIG_ENDIAN)
#define BROTLI_BIG_ENDIAN 0
#endif

#if defined(BROTLI_BUILD_NO_UNALIGNED_READ_FAST)
#define BROTLI_UNALIGNED_READ_FAST (!!0)
#elif defined(BROTLI_TARGET_X86) || defined(BROTLI_TARGET_X64) ||       \
    defined(BROTLI_TARGET_ARMV7) || defined(BROTLI_TARGET_ARMV8_ANY) || \
    defined(BROTLI_TARGET_RISCV64) || defined(BROTLI_TARGET_LOONGARCH64)

#define BROTLI_UNALIGNED_READ_FAST (!!1)
#else
#define BROTLI_UNALIGNED_READ_FAST (!!0)
#endif

#if !defined(BROTLI_USE_PACKED_FOR_UNALIGNED)
#if defined(__mips__) && (!defined(__mips_isa_rev) || __mips_isa_rev < 6)
#define BROTLI_USE_PACKED_FOR_UNALIGNED 1
#else
#define BROTLI_USE_PACKED_FOR_UNALIGNED 0
#endif
#endif

#if BROTLI_USE_PACKED_FOR_UNALIGNED

typedef union BrotliPackedValue {
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  size_t szt;
} __attribute__ ((packed)) BrotliPackedValue;

static BROTLI_INLINE uint16_t BrotliUnalignedRead16(const void* p) {
  const BrotliPackedValue* address = (const BrotliPackedValue*)p;
  return address->u16;
}
static BROTLI_INLINE uint32_t BrotliUnalignedRead32(const void* p) {
  const BrotliPackedValue* address = (const BrotliPackedValue*)p;
  return address->u32;
}
static BROTLI_INLINE uint64_t BrotliUnalignedRead64(const void* p) {
  const BrotliPackedValue* address = (const BrotliPackedValue*)p;
  return address->u64;
}
static BROTLI_INLINE size_t BrotliUnalignedReadSizeT(const void* p) {
  const BrotliPackedValue* address = (const BrotliPackedValue*)p;
  return address->szt;
}
static BROTLI_INLINE void BrotliUnalignedWrite64(void* p, uint64_t v) {
  BrotliPackedValue* address = (BrotliPackedValue*)p;
  address->u64 = v;
}

#else

static BROTLI_INLINE uint16_t BrotliUnalignedRead16(const void* p) {
  uint16_t t;
  memcpy(&t, p, sizeof t);
  return t;
}
static BROTLI_INLINE uint32_t BrotliUnalignedRead32(const void* p) {
  uint32_t t;
  memcpy(&t, p, sizeof t);
  return t;
}
static BROTLI_INLINE uint64_t BrotliUnalignedRead64(const void* p) {
  uint64_t t;
  memcpy(&t, p, sizeof t);
  return t;
}
static BROTLI_INLINE size_t BrotliUnalignedReadSizeT(const void* p) {
  size_t t;
  memcpy(&t, p, sizeof t);
  return t;
}
static BROTLI_INLINE void BrotliUnalignedWrite64(void* p, uint64_t v) {
  memcpy(p, &v, sizeof v);
}

#endif

#if BROTLI_GNUC_HAS_BUILTIN(__builtin_bswap16, 4, 3, 0)
#define BROTLI_BSWAP16(V) ((uint16_t)__builtin_bswap16(V))
#else
#define BROTLI_BSWAP16(V) ((uint16_t)( \
  (((V) & 0xFFU) << 8) | \
  (((V) >> 8) & 0xFFU)))
#endif

#if BROTLI_GNUC_HAS_BUILTIN(__builtin_bswap32, 4, 3, 0)
#define BROTLI_BSWAP32(V) ((uint32_t)__builtin_bswap32(V))
#else
#define BROTLI_BSWAP32(V) ((uint32_t)( \
  (((V) & 0xFFU) << 24) | (((V) & 0xFF00U) << 8) | \
  (((V) >> 8) & 0xFF00U) | (((V) >> 24) & 0xFFU)))
#endif

#if BROTLI_GNUC_HAS_BUILTIN(__builtin_bswap64, 4, 3, 0)
#define BROTLI_BSWAP64(V) ((uint64_t)__builtin_bswap64(V))
#else
#define BROTLI_BSWAP64(V) ((uint64_t)( \
  (((V) & 0xFFU) << 56) | (((V) & 0xFF00U) << 40) | \
  (((V) & 0xFF0000U) << 24) | (((V) & 0xFF000000U) << 8) | \
  (((V) >> 8) & 0xFF000000U) | (((V) >> 24) & 0xFF0000U) | \
  (((V) >> 40) & 0xFF00U) | (((V) >> 56) & 0xFFU)))
#endif

#if BROTLI_LITTLE_ENDIAN

#define BROTLI_UNALIGNED_LOAD16LE BrotliUnalignedRead16
#define BROTLI_UNALIGNED_LOAD32LE BrotliUnalignedRead32
#define BROTLI_UNALIGNED_LOAD64LE BrotliUnalignedRead64
#define BROTLI_UNALIGNED_STORE64LE BrotliUnalignedWrite64
#elif BROTLI_BIG_ENDIAN
static BROTLI_INLINE uint16_t BROTLI_UNALIGNED_LOAD16LE(const void* p) {
  uint16_t value = BrotliUnalignedRead16(p);
  return BROTLI_BSWAP16(value);
}
static BROTLI_INLINE uint32_t BROTLI_UNALIGNED_LOAD32LE(const void* p) {
  uint32_t value = BrotliUnalignedRead32(p);
  return BROTLI_BSWAP32(value);
}
static BROTLI_INLINE uint64_t BROTLI_UNALIGNED_LOAD64LE(const void* p) {
  uint64_t value = BrotliUnalignedRead64(p);
  return BROTLI_BSWAP64(value);
}
static BROTLI_INLINE void BROTLI_UNALIGNED_STORE64LE(void* p, uint64_t v) {
  uint64_t value = BROTLI_BSWAP64(v);
  BrotliUnalignedWrite64(p, value);
}
#else

static BROTLI_INLINE uint16_t BROTLI_UNALIGNED_LOAD16LE(const void* p) {
  const uint8_t* in = (const uint8_t*)p;
  return (uint16_t)(in[0] | (in[1] << 8));
}
static BROTLI_INLINE uint32_t BROTLI_UNALIGNED_LOAD32LE(const void* p) {
  const uint8_t* in = (const uint8_t*)p;
  uint32_t value = (uint32_t)(in[0]);
  value |= (uint32_t)(in[1]) << 8;
  value |= (uint32_t)(in[2]) << 16;
  value |= (uint32_t)(in[3]) << 24;
  return value;
}
static BROTLI_INLINE uint64_t BROTLI_UNALIGNED_LOAD64LE(const void* p) {
  const uint8_t* in = (const uint8_t*)p;
  uint64_t value = (uint64_t)(in[0]);
  value |= (uint64_t)(in[1]) << 8;
  value |= (uint64_t)(in[2]) << 16;
  value |= (uint64_t)(in[3]) << 24;
  value |= (uint64_t)(in[4]) << 32;
  value |= (uint64_t)(in[5]) << 40;
  value |= (uint64_t)(in[6]) << 48;
  value |= (uint64_t)(in[7]) << 56;
  return value;
}
static BROTLI_INLINE void BROTLI_UNALIGNED_STORE64LE(void* p, uint64_t v) {
  uint8_t* out = (uint8_t*)p;
  out[0] = (uint8_t)v;
  out[1] = (uint8_t)(v >> 8);
  out[2] = (uint8_t)(v >> 16);
  out[3] = (uint8_t)(v >> 24);
  out[4] = (uint8_t)(v >> 32);
  out[5] = (uint8_t)(v >> 40);
  out[6] = (uint8_t)(v >> 48);
  out[7] = (uint8_t)(v >> 56);
}
#endif

static BROTLI_INLINE void* BROTLI_UNALIGNED_LOAD_PTR(const void* p) {
  void* v;
  memcpy(&v, p, sizeof(void*));
  return v;
}

static BROTLI_INLINE void BROTLI_UNALIGNED_STORE_PTR(void* p, const void* v) {
  memcpy(p, &v, sizeof(void*));
}

#if BROTLI_GNUC_HAS_BUILTIN(__builtin_constant_p, 3, 0, 1) || \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0)
#define BROTLI_IS_CONSTANT(x) (!!__builtin_constant_p(x))
#else
#define BROTLI_IS_CONSTANT(x) (!!0)
#endif

#if defined(BROTLI_TARGET_ARMV7) || defined(BROTLI_TARGET_ARMV8_ANY)
#define BROTLI_HAS_UBFX (!!1)
#else
#define BROTLI_HAS_UBFX (!!0)
#endif

#if defined(BROTLI_ENABLE_LOG)
#define BROTLI_LOG(x) printf x
#else
#define BROTLI_LOG(x)
#endif

#if defined(BROTLI_DEBUG) || defined(BROTLI_ENABLE_LOG)
#define BROTLI_ENABLE_DUMP_DEFAULT 1
#define BROTLI_DCHECK(x) assert(x)
#else
#define BROTLI_ENABLE_DUMP_DEFAULT 0
#define BROTLI_DCHECK(x)
#endif

#if !defined(BROTLI_ENABLE_DUMP)
#define BROTLI_ENABLE_DUMP BROTLI_ENABLE_DUMP_DEFAULT
#endif

#if BROTLI_ENABLE_DUMP
static BROTLI_INLINE void BrotliDump(const char* f, int l, const char* fn) {
  fprintf(stderr, "%s:%d (%s)\n", f, l, fn);
  fflush(stderr);
}
#define BROTLI_DUMP() BrotliDump(__FILE__, __LINE__, __FUNCTION__)
#else
#define BROTLI_DUMP() (void)(0)
#endif

#if (BROTLI_64_BITS == BROTLI_TARGET_64_BITS)

#if (BROTLI_GNUC_VERSION_CHECK(3, 0, 0) || defined(__llvm__)) && \
    !defined(BROTLI_BUILD_NO_RBIT)
#if defined(BROTLI_TARGET_ARMV7) || defined(BROTLI_TARGET_ARMV8_ANY)

static BROTLI_INLINE brotli_reg_t BrotliRBit(brotli_reg_t input) {
  brotli_reg_t output;
  __asm__("rbit %0, %1\n" : "=r"(output) : "r"(input));
  return output;
}
#define BROTLI_RBIT(x) BrotliRBit(x)
#endif
#endif
#endif
#if !defined(BROTLI_RBIT)
static BROTLI_INLINE void BrotliRBit(void) {  }
#endif

#define BROTLI_REPEAT_4(X) {X; X; X; X;}
#define BROTLI_REPEAT_5(X) {X; X; X; X; X;}
#define BROTLI_REPEAT_6(X) {X; X; X; X; X; X;}

#define BROTLI_UNUSED(X) (void)(X)

#define BROTLI_MIN_MAX(T)                                                      \
  static BROTLI_INLINE T brotli_min_ ## T (T a, T b) { return a < b ? a : b; } \
  static BROTLI_INLINE T brotli_max_ ## T (T a, T b) { return a > b ? a : b; }
BROTLI_MIN_MAX(double) BROTLI_MIN_MAX(float) BROTLI_MIN_MAX(int)
BROTLI_MIN_MAX(size_t) BROTLI_MIN_MAX(uint32_t) BROTLI_MIN_MAX(uint8_t)
#undef BROTLI_MIN_MAX
#define BROTLI_MIN(T, A, B) (brotli_min_ ## T((A), (B)))
#define BROTLI_MAX(T, A, B) (brotli_max_ ## T((A), (B)))

static BROTLI_INLINE int brotli_safe_add_int(int a, int b, int* result) {
  if (b > 0 && a > INT_MAX - b) return 0;
  if (b < 0 && a < INT_MIN - b) return 0;
  *result = a + b;
  return 1;
}
#define BROTLI_SAFE_ADD(T, A, B, R) (brotli_safe_add_ ## T((A), (B), (R)))

#define BROTLI_SWAP(T, A, I, J) { \
  T __brotli_swap_tmp = (A)[(I)]; \
  (A)[(I)] = (A)[(J)];            \
  (A)[(J)] = __brotli_swap_tmp;   \
}

#if BROTLI_64_BITS
#if BROTLI_GNUC_HAS_BUILTIN(__builtin_ctzll, 3, 4, 0) || \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0)
#define BROTLI_TZCNT64 __builtin_ctzll
#elif BROTLI_MSVC_VERSION_CHECK(18, 0, 0)
#if defined(BROTLI_TARGET_X64) && !defined(_M_ARM64EC)
#define BROTLI_TZCNT64 _tzcnt_u64
#else
static BROTLI_INLINE uint32_t BrotliBsf64Msvc(uint64_t x) {
  uint32_t lsb;
  _BitScanForward64(&lsb, x);
  return lsb;
}
#define BROTLI_TZCNT64 BrotliBsf64Msvc
#endif
#endif
#endif

#if BROTLI_GNUC_HAS_BUILTIN(__builtin_clz, 3, 4, 0) || \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0)
#define BROTLI_BSR32(x) (31u ^ (uint32_t)__builtin_clz(x))
#elif BROTLI_MSVC_VERSION_CHECK(18, 0, 0)
static BROTLI_INLINE uint32_t BrotliBsr32Msvc(uint32_t x) {
  unsigned long msb;
  _BitScanReverse(&msb, x);
  return (uint32_t)msb;
}
#define BROTLI_BSR32 BrotliBsr32Msvc
#endif

BROTLI_COMMON_API void* BrotliDefaultAllocFunc(void* opaque, size_t size);

BROTLI_COMMON_API void BrotliDefaultFreeFunc(void* opaque, void* address);

static BROTLI_INLINE uint16_t BrotliRotateRight16(uint16_t const value,
                                             size_t count) {
  count &= 0x0F;
  return (value >> count) | (uint16_t)(value << ((0U - count) & 0x0F));
}
static BROTLI_INLINE uint32_t BrotliRotateRight32(uint32_t const value,
                                             size_t count) {
  count &= 0x1F;
  return (value >> count) | (uint32_t)(value << ((0U - count) & 0x1F));
}
static BROTLI_INLINE uint64_t BrotliRotateRight64(uint64_t const value,
                                             size_t count) {
  count &= 0x3F;
  return (value >> count) | (uint64_t)(value << ((0U - count) & 0x3F));
}

BROTLI_UNUSED_FUNCTION void BrotliSuppressUnusedFunctions(void) {
  BROTLI_UNUSED(&BrotliSuppressUnusedFunctions);
  BROTLI_UNUSED(&BrotliUnalignedRead16);
  BROTLI_UNUSED(&BrotliUnalignedRead32);
  BROTLI_UNUSED(&BrotliUnalignedRead64);
  BROTLI_UNUSED(&BrotliUnalignedReadSizeT);
  BROTLI_UNUSED(&BrotliUnalignedWrite64);
  BROTLI_UNUSED(&BROTLI_UNALIGNED_LOAD16LE);
  BROTLI_UNUSED(&BROTLI_UNALIGNED_LOAD32LE);
  BROTLI_UNUSED(&BROTLI_UNALIGNED_LOAD64LE);
  BROTLI_UNUSED(&BROTLI_UNALIGNED_STORE64LE);
  BROTLI_UNUSED(&BROTLI_UNALIGNED_LOAD_PTR);
  BROTLI_UNUSED(&BROTLI_UNALIGNED_STORE_PTR);
  BROTLI_UNUSED(&BrotliRBit);
  BROTLI_UNUSED(&brotli_min_double);
  BROTLI_UNUSED(&brotli_max_double);
  BROTLI_UNUSED(&brotli_min_float);
  BROTLI_UNUSED(&brotli_max_float);
  BROTLI_UNUSED(&brotli_min_int);
  BROTLI_UNUSED(&brotli_max_int);
  BROTLI_UNUSED(&brotli_min_size_t);
  BROTLI_UNUSED(&brotli_max_size_t);
  BROTLI_UNUSED(&brotli_min_uint32_t);
  BROTLI_UNUSED(&brotli_max_uint32_t);
  BROTLI_UNUSED(&brotli_min_uint8_t);
  BROTLI_UNUSED(&brotli_max_uint8_t);
  BROTLI_UNUSED(&brotli_safe_add_int);
  BROTLI_UNUSED(&BrotliDefaultAllocFunc);
  BROTLI_UNUSED(&BrotliDefaultFreeFunc);
  BROTLI_UNUSED(&BrotliRotateRight16);
  BROTLI_UNUSED(&BrotliRotateRight32);
  BROTLI_UNUSED(&BrotliRotateRight64);
#if BROTLI_ENABLE_DUMP
  BROTLI_UNUSED(&BrotliDump);
#endif
}

#if defined(_MSC_VER) && (_MSC_VER >= 1800) &&(defined(_M_X64) || defined(_M_I86)) && \
    !defined(_M_ARM64EC)

#if 0 && _MSC_VER >= 1800
#include <mmintrin.h>
#else
#undef SUPPORTS_SSE2
#endif
#define PREFETCH_L1(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#define PREFETCH_L2(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T1)
#elif BROTLI_GNUC_HAS_BUILTIN(__builtin_prefetch, 3, 1, 0)
#define PREFETCH_L1(ptr) \
  __builtin_prefetch((ptr), 0 , 3 )
#define PREFETCH_L2(ptr) \
  __builtin_prefetch((ptr), 0 , 2 )
#elif defined(__aarch64__)
#define PREFETCH_L1(ptr)                                      \
  do {                                                        \
    __asm__ __volatile__("prfm pldl1keep, %0" ::"Q"(*(ptr))); \
  } while (0)
#define PREFETCH_L2(ptr)                                      \
  do {                                                        \
    __asm__ __volatile__("prfm pldl2keep, %0" ::"Q"(*(ptr))); \
  } while (0)
#else
#define PREFETCH_L1(ptr) \
  do {                   \
    (void)(ptr);         \
  } while (0)
#define PREFETCH_L2(ptr) \
  do {                   \
    (void)(ptr);         \
  } while (0)
#endif

#if defined(_M_X64) && defined(BROTLI_TZCNT64)
#define BROTLI_MAX_RECOMMENDED_SIMD_QUALITY 7
#define BROTLI_MAX_SIMD_QUALITY 7
#elif defined(BROTLI_TZCNT64)
#define BROTLI_MAX_RECOMMENDED_SIMD_QUALITY 6
#define BROTLI_MAX_SIMD_QUALITY 7
#endif

#if defined(_MSC_VER)
#define BROTLI_CRASH() __debugbreak(), (void)abort()
#elif BROTLI_GNUC_HAS_BUILTIN(__builtin_trap, 3, 0, 0)
#define BROTLI_CRASH() (void)__builtin_trap()
#else
#define BROTLI_CRASH() (void)abort()
#endif

#if defined(BROTLI_TEST) && ((1-BROTLI_TEST-1) == 0)
#undef BROTLI_TEST
#endif

#if !defined(BROTLI_MODEL) && BROTLI_GNUC_HAS_ATTRIBUTE(model, 3, 0, 3) && \
    !defined(BROTLI_TARGET_IA64) && !defined(BROTLI_TARGET_LOONGARCH64)
#define BROTLI_MODEL(M) __attribute__((model(M)))
#else
#define BROTLI_MODEL(M)
#endif

#if !defined(BROTLI_COLD) && BROTLI_GNUC_HAS_ATTRIBUTE(cold, 4, 3, 0)
#define BROTLI_COLD __attribute__((cold))
#else
#define BROTLI_COLD
#endif

#endif

#define BROTLI_CONTEXT_MAP_MAX_RLE 16

#define BROTLI_MAX_NUMBER_OF_BLOCK_TYPES 256

#define BROTLI_NUM_LITERAL_SYMBOLS 256
#define BROTLI_NUM_COMMAND_SYMBOLS 704
#define BROTLI_NUM_BLOCK_LEN_SYMBOLS 26
#define BROTLI_MAX_CONTEXT_MAP_SYMBOLS (BROTLI_MAX_NUMBER_OF_BLOCK_TYPES + \
                                        BROTLI_CONTEXT_MAP_MAX_RLE)
#define BROTLI_MAX_BLOCK_TYPE_SYMBOLS (BROTLI_MAX_NUMBER_OF_BLOCK_TYPES + 2)

#define BROTLI_REPEAT_PREVIOUS_CODE_LENGTH 16
#define BROTLI_REPEAT_ZERO_CODE_LENGTH 17
#define BROTLI_CODE_LENGTH_CODES (BROTLI_REPEAT_ZERO_CODE_LENGTH + 1)

#define BROTLI_INITIAL_REPEATED_CODE_LENGTH 8

#define BROTLI_LARGE_MAX_DISTANCE_BITS 62U
#define BROTLI_LARGE_MIN_WBITS 10

#define BROTLI_LARGE_MAX_WBITS 30

#define BROTLI_NUM_DISTANCE_SHORT_CODES 16

#define BROTLI_MAX_NPOSTFIX 3
#define BROTLI_MAX_NDIRECT 120
#define BROTLI_MAX_DISTANCE_BITS 24U
#define BROTLI_DISTANCE_ALPHABET_SIZE(NPOSTFIX, NDIRECT, MAXNBITS) ( \
    BROTLI_NUM_DISTANCE_SHORT_CODES + (NDIRECT) +                    \
    ((MAXNBITS) << ((NPOSTFIX) + 1)))

#define BROTLI_NUM_DISTANCE_SYMBOLS \
    BROTLI_DISTANCE_ALPHABET_SIZE(  \
        BROTLI_MAX_NDIRECT, BROTLI_MAX_NPOSTFIX, BROTLI_LARGE_MAX_DISTANCE_BITS)

#define BROTLI_MAX_DISTANCE 0x3FFFFFC

#define BROTLI_MAX_ALLOWED_DISTANCE 0x7FFFFFFC

#define BROTLI_NUM_INS_COPY_CODES 24

#define BROTLI_LITERAL_CONTEXT_BITS 6

#define BROTLI_DISTANCE_CONTEXT_BITS 2

#define BROTLI_WINDOW_GAP 16
#define BROTLI_MAX_BACKWARD_LIMIT(W) (((size_t)1 << (W)) - BROTLI_WINDOW_GAP)

typedef struct BrotliDistanceCodeLimit {
  uint32_t max_alphabet_size;
  uint32_t max_distance;
} BrotliDistanceCodeLimit;

BROTLI_UNUSED_FUNCTION BrotliDistanceCodeLimit BrotliCalculateDistanceCodeLimit(
    uint32_t max_distance, uint32_t npostfix, uint32_t ndirect) {
  BrotliDistanceCodeLimit result;

  BROTLI_UNUSED(&BrotliCalculateDistanceCodeLimit);
  if (max_distance <= ndirect) {

    result.max_alphabet_size = max_distance + BROTLI_NUM_DISTANCE_SHORT_CODES;
    result.max_distance = max_distance;
    return result;
  } else {

    uint32_t forbidden_distance = max_distance + 1;

    uint32_t offset = forbidden_distance - ndirect - 1;
    uint32_t ndistbits = 0;
    uint32_t tmp;
    uint32_t half;
    uint32_t group;

    uint32_t postfix = (1u << npostfix) - 1;
    uint32_t extra;
    uint32_t start;

    offset = (offset >> npostfix) + 4;

    tmp = offset / 2;

    while (tmp != 0) {ndistbits++; tmp = tmp >> 1;}

    ndistbits--;

    half = (offset >> ndistbits) & 1;

    group = ((ndistbits - 1) << 1) | half;

    if (group == 0) {

      result.max_alphabet_size = ndirect + BROTLI_NUM_DISTANCE_SHORT_CODES;
      result.max_distance = ndirect;
      return result;
    }

    group--;

    ndistbits = (group >> 1) + 1;

    extra = (1u << ndistbits) - 1;

    start = (1u << (ndistbits + 1)) - 4;

    start += (group & 1) << ndistbits;

    result.max_alphabet_size = ((group << npostfix) | postfix) + ndirect +
        BROTLI_NUM_DISTANCE_SHORT_CODES + 1;

    result.max_distance = ((start + extra) << npostfix) + postfix + ndirect + 1;
    return result;
  }
}

typedef struct {
  uint16_t offset;
  uint8_t nbits;
} BrotliPrefixCodeRange;

BROTLI_COMMON_API extern const BROTLI_MODEL("small")
BrotliPrefixCodeRange _kBrotliPrefixCodeRanges[BROTLI_NUM_BLOCK_LEN_SYMBOLS];

#endif

#ifndef BROTLI_COMMON_CONTEXT_H_
#define BROTLI_COMMON_CONTEXT_H_

typedef enum ContextType {
  CONTEXT_LSB6 = 0,
  CONTEXT_MSB6 = 1,
  CONTEXT_UTF8 = 2,
  CONTEXT_SIGNED = 3
} ContextType;

BROTLI_COMMON_API extern const uint8_t _kBrotliContextLookupTable[2048];

typedef const uint8_t* ContextLut;

#define BROTLI_CONTEXT_LUT(MODE) (&_kBrotliContextLookupTable[(MODE) << 9])

#define BROTLI_CONTEXT(P1, P2, LUT) ((LUT)[P1] | ((LUT) + 256)[P2])

#endif

#ifndef BROTLI_COMMON_DICTIONARY_H_
#define BROTLI_COMMON_DICTIONARY_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct BrotliDictionary {

  uint8_t size_bits_by_length[32];

  uint32_t offsets_by_length[32];

  size_t data_size;

  const uint8_t* data;
} BrotliDictionary;

BROTLI_COMMON_API const BrotliDictionary* BrotliGetDictionary(void);

BROTLI_COMMON_API void BrotliSetDictionaryData(const uint8_t* data);

#define BROTLI_MIN_DICTIONARY_WORD_LENGTH 4
#define BROTLI_MAX_DICTIONARY_WORD_LENGTH 24

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_COMMON_SHARED_DICTIONARY_INTERNAL_H_
#define BROTLI_COMMON_SHARED_DICTIONARY_INTERNAL_H_

#ifndef BROTLI_COMMON_TRANSFORM_H_
#define BROTLI_COMMON_TRANSFORM_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

enum BrotliWordTransformType {
  BROTLI_TRANSFORM_IDENTITY = 0,
  BROTLI_TRANSFORM_OMIT_LAST_1 = 1,
  BROTLI_TRANSFORM_OMIT_LAST_2 = 2,
  BROTLI_TRANSFORM_OMIT_LAST_3 = 3,
  BROTLI_TRANSFORM_OMIT_LAST_4 = 4,
  BROTLI_TRANSFORM_OMIT_LAST_5 = 5,
  BROTLI_TRANSFORM_OMIT_LAST_6 = 6,
  BROTLI_TRANSFORM_OMIT_LAST_7 = 7,
  BROTLI_TRANSFORM_OMIT_LAST_8 = 8,
  BROTLI_TRANSFORM_OMIT_LAST_9 = 9,
  BROTLI_TRANSFORM_UPPERCASE_FIRST = 10,
  BROTLI_TRANSFORM_UPPERCASE_ALL = 11,
  BROTLI_TRANSFORM_OMIT_FIRST_1 = 12,
  BROTLI_TRANSFORM_OMIT_FIRST_2 = 13,
  BROTLI_TRANSFORM_OMIT_FIRST_3 = 14,
  BROTLI_TRANSFORM_OMIT_FIRST_4 = 15,
  BROTLI_TRANSFORM_OMIT_FIRST_5 = 16,
  BROTLI_TRANSFORM_OMIT_FIRST_6 = 17,
  BROTLI_TRANSFORM_OMIT_FIRST_7 = 18,
  BROTLI_TRANSFORM_OMIT_FIRST_8 = 19,
  BROTLI_TRANSFORM_OMIT_FIRST_9 = 20,
  BROTLI_TRANSFORM_SHIFT_FIRST = 21,
  BROTLI_TRANSFORM_SHIFT_ALL = 22,
  BROTLI_NUM_TRANSFORM_TYPES
};

#define BROTLI_TRANSFORMS_MAX_CUT_OFF BROTLI_TRANSFORM_OMIT_LAST_9

typedef struct BrotliTransforms {
  uint16_t prefix_suffix_size;

  const uint8_t* prefix_suffix;
  const uint16_t* prefix_suffix_map;
  uint32_t num_transforms;

  const uint8_t* transforms;

  const uint8_t* params;

  int16_t cutOffTransforms[BROTLI_TRANSFORMS_MAX_CUT_OFF + 1];
} BrotliTransforms;

#define BROTLI_TRANSFORM_PREFIX_ID(T, I) ((T)->transforms[((I) * 3) + 0])
#define BROTLI_TRANSFORM_TYPE(T, I)      ((T)->transforms[((I) * 3) + 1])
#define BROTLI_TRANSFORM_SUFFIX_ID(T, I) ((T)->transforms[((I) * 3) + 2])

#define BROTLI_TRANSFORM_PREFIX(T, I) (&(T)->prefix_suffix[ \
    (T)->prefix_suffix_map[BROTLI_TRANSFORM_PREFIX_ID(T, I)]])
#define BROTLI_TRANSFORM_SUFFIX(T, I) (&(T)->prefix_suffix[ \
    (T)->prefix_suffix_map[BROTLI_TRANSFORM_SUFFIX_ID(T, I)]])

BROTLI_COMMON_API const BrotliTransforms* BrotliGetTransforms(void);

BROTLI_COMMON_API int BrotliTransformDictionaryWord(
    uint8_t* dst, const uint8_t* word, int len,
    const BrotliTransforms* transforms, int transform_idx);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

struct BrotliSharedDictionaryStruct {

  uint32_t num_prefix;
  size_t prefix_size[SHARED_BROTLI_MAX_COMPOUND_DICTS];
  const uint8_t* prefix[SHARED_BROTLI_MAX_COMPOUND_DICTS];

  BROTLI_BOOL context_based;

  uint8_t context_map[SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS];

  uint8_t num_dictionaries;

  const BrotliDictionary* words[SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS];

  const BrotliTransforms* transforms[SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS];

  uint8_t num_word_lists;

  BrotliDictionary* words_instances;

  uint8_t num_transform_lists;

  BrotliTransforms* transforms_instances;

  uint16_t* prefix_suffix_maps;

  brotli_alloc_func alloc_func;
  brotli_free_func free_func;
  void* memory_manager_opaque;
};

typedef struct BrotliSharedDictionaryStruct BrotliSharedDictionaryInternal;
#define BrotliSharedDictionary BrotliSharedDictionaryInternal

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_DEC_BIT_READER_H_
#define BROTLI_DEC_BIT_READER_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_SHORT_FILL_BIT_WINDOW_READ (sizeof(brotli_reg_t) >> 1)

#define BROTLI_FAST_INPUT_SLACK 28

BROTLI_INTERNAL extern const brotli_reg_t kBrotliBitMask[33];

static BROTLI_INLINE brotli_reg_t BitMask(brotli_reg_t n) {
  if (BROTLI_IS_CONSTANT(n) || BROTLI_HAS_UBFX) {

    return ~(~((brotli_reg_t)0) << n);
  } else {
    return kBrotliBitMask[n];
  }
}

typedef struct {
  brotli_reg_t val_;
  brotli_reg_t bit_pos_;
  const uint8_t* next_in;
  const uint8_t* guard_in;
  const uint8_t* last_in;
} BrotliBitReader;

typedef struct {
  brotli_reg_t val_;
  brotli_reg_t bit_pos_;
  const uint8_t* next_in;
  size_t avail_in;
} BrotliBitReaderState;

BROTLI_INTERNAL void BrotliInitBitReader(BrotliBitReader* br);

BROTLI_INTERNAL BROTLI_BOOL BrotliWarmupBitReader(BrotliBitReader* br);

BROTLI_INTERNAL BROTLI_NOINLINE BROTLI_BOOL BrotliSafeReadBits32Slow(
    BrotliBitReader* br, brotli_reg_t n_bits, brotli_reg_t* val);

static BROTLI_INLINE size_t
BrotliBitReaderGetAvailIn(BrotliBitReader* const br) {
  return (size_t)(br->last_in - br->next_in);
}

static BROTLI_INLINE void BrotliBitReaderSaveState(
    BrotliBitReader* const from, BrotliBitReaderState* to) {
  to->val_ = from->val_;
  to->bit_pos_ = from->bit_pos_;
  to->next_in = from->next_in;
  to->avail_in = BrotliBitReaderGetAvailIn(from);
}

static BROTLI_INLINE void BrotliBitReaderSetInput(
    BrotliBitReader* const br, const uint8_t* next_in, size_t avail_in) {
  br->next_in = next_in;
  br->last_in = (avail_in == 0) ? next_in : (next_in + avail_in);
  if (avail_in + 1 > BROTLI_FAST_INPUT_SLACK) {
    br->guard_in = next_in + (avail_in + 1 - BROTLI_FAST_INPUT_SLACK);
  } else {
    br->guard_in = next_in;
  }
}

static BROTLI_INLINE void BrotliBitReaderRestoreState(
    BrotliBitReader* const to, BrotliBitReaderState* from) {
  to->val_ = from->val_;
  to->bit_pos_ = from->bit_pos_;
  to->next_in = from->next_in;
  BrotliBitReaderSetInput(to, from->next_in, from->avail_in);
}

static BROTLI_INLINE brotli_reg_t BrotliGetAvailableBits(
    const BrotliBitReader* br) {
  return br->bit_pos_;
}

static BROTLI_INLINE size_t BrotliGetRemainingBytes(BrotliBitReader* br) {
  static const size_t kCap = (size_t)1 << BROTLI_LARGE_MAX_WBITS;
  size_t avail_in = BrotliBitReaderGetAvailIn(br);
  if (avail_in > kCap) return kCap;
  return avail_in + (BrotliGetAvailableBits(br) >> 3);
}

static BROTLI_INLINE BROTLI_BOOL BrotliCheckInputAmount(
    BrotliBitReader* const br) {
  return TO_BROTLI_BOOL(br->next_in < br->guard_in);
}

static BROTLI_INLINE brotli_reg_t BrotliBitReaderLoadBits(brotli_reg_t val,
                                                          brotli_reg_t new_bits,
                                                          brotli_reg_t count,
                                                          brotli_reg_t offset) {
  BROTLI_DCHECK(
      !((val >> offset) & ~new_bits & ~(~((brotli_reg_t)0) << count)));
  (void)count;
  return val | (new_bits << offset);
}

static BROTLI_INLINE void BrotliFillBitWindow(
    BrotliBitReader* const br, brotli_reg_t n_bits) {
#if (BROTLI_64_BITS)
  if (BROTLI_UNALIGNED_READ_FAST && BROTLI_IS_CONSTANT(n_bits) &&
      (n_bits <= 8)) {
    brotli_reg_t bit_pos = br->bit_pos_;
    if (bit_pos <= 8) {
      br->val_ = BrotliBitReaderLoadBits(br->val_,
          BROTLI_UNALIGNED_LOAD64LE(br->next_in), 56, bit_pos);
      br->bit_pos_ = bit_pos + 56;
      br->next_in += 7;
    }
  } else if (BROTLI_UNALIGNED_READ_FAST && BROTLI_IS_CONSTANT(n_bits) &&
             (n_bits <= 16)) {
    brotli_reg_t bit_pos = br->bit_pos_;
    if (bit_pos <= 16) {
      br->val_ = BrotliBitReaderLoadBits(br->val_,
          BROTLI_UNALIGNED_LOAD64LE(br->next_in), 48, bit_pos);
      br->bit_pos_ = bit_pos + 48;
      br->next_in += 6;
    }
  } else {
    brotli_reg_t bit_pos = br->bit_pos_;
    if (bit_pos <= 32) {
      br->val_ = BrotliBitReaderLoadBits(br->val_,
          (uint64_t)BROTLI_UNALIGNED_LOAD32LE(br->next_in), 32, bit_pos);
      br->bit_pos_ = bit_pos + 32;
      br->next_in += BROTLI_SHORT_FILL_BIT_WINDOW_READ;
    }
  }
#else
  if (BROTLI_UNALIGNED_READ_FAST && BROTLI_IS_CONSTANT(n_bits) &&
      (n_bits <= 8)) {
    brotli_reg_t bit_pos = br->bit_pos_;
    if (bit_pos <= 8) {
      br->val_ = BrotliBitReaderLoadBits(br->val_,
          BROTLI_UNALIGNED_LOAD32LE(br->next_in), 24, bit_pos);
      br->bit_pos_ = bit_pos + 24;
      br->next_in += 3;
    }
  } else {
    brotli_reg_t bit_pos = br->bit_pos_;
    if (bit_pos <= 16) {
      br->val_ = BrotliBitReaderLoadBits(br->val_,
          (uint32_t)BROTLI_UNALIGNED_LOAD16LE(br->next_in), 16, bit_pos);
      br->bit_pos_ = bit_pos + 16;
      br->next_in += BROTLI_SHORT_FILL_BIT_WINDOW_READ;
    }
  }
#endif
}

static BROTLI_INLINE void BrotliFillBitWindow16(BrotliBitReader* const br) {
  BrotliFillBitWindow(br, 17);
}

static BROTLI_INLINE BROTLI_BOOL BrotliPullByte(BrotliBitReader* const br) {
  if (br->next_in == br->last_in) {
    return BROTLI_FALSE;
  }
  br->val_ = BrotliBitReaderLoadBits(br->val_,
      (brotli_reg_t)*br->next_in, 8, br->bit_pos_);
  br->bit_pos_ += 8;
  ++br->next_in;
  return BROTLI_TRUE;
}

static BROTLI_INLINE brotli_reg_t BrotliGetBitsUnmasked(
    BrotliBitReader* const br) {
  return br->val_;
}

static BROTLI_INLINE brotli_reg_t BrotliGet16BitsUnmasked(
    BrotliBitReader* const br) {
  BrotliFillBitWindow(br, 16);
  return (brotli_reg_t)BrotliGetBitsUnmasked(br);
}

static BROTLI_INLINE brotli_reg_t BrotliGetBits(
    BrotliBitReader* const br, brotli_reg_t n_bits) {
  BrotliFillBitWindow(br, n_bits);
  return BrotliGetBitsUnmasked(br) & BitMask(n_bits);
}

static BROTLI_INLINE BROTLI_BOOL BrotliSafeGetBits(
    BrotliBitReader* const br, brotli_reg_t n_bits, brotli_reg_t* val) {
  while (BrotliGetAvailableBits(br) < n_bits) {
    if (!BrotliPullByte(br)) {
      return BROTLI_FALSE;
    }
  }
  *val = BrotliGetBitsUnmasked(br) & BitMask(n_bits);
  return BROTLI_TRUE;
}

static BROTLI_INLINE void BrotliDropBits(
    BrotliBitReader* const br, brotli_reg_t n_bits) {
  br->bit_pos_ -= n_bits;
  br->val_ >>= n_bits;
}

static BROTLI_INLINE void BrotliBitReaderNormalize(BrotliBitReader* br) {

  if (br->bit_pos_ < (sizeof(brotli_reg_t) << 3u)) {
    br->val_ &= (((brotli_reg_t)1) << br->bit_pos_) - 1;
  }
}

static BROTLI_INLINE void BrotliBitReaderUnload(BrotliBitReader* br) {
  brotli_reg_t unused_bytes = BrotliGetAvailableBits(br) >> 3;
  brotli_reg_t unused_bits = unused_bytes << 3;
  br->next_in =
      (unused_bytes == 0) ? br->next_in : (br->next_in - unused_bytes);
  br->bit_pos_ -= unused_bits;
  BrotliBitReaderNormalize(br);
}

static BROTLI_INLINE void BrotliTakeBits(BrotliBitReader* const br,
                                         brotli_reg_t n_bits,
                                         brotli_reg_t* val) {
  *val = BrotliGetBitsUnmasked(br) & BitMask(n_bits);
  BROTLI_LOG(("[BrotliTakeBits]  %d %d %d val: %6x\n",
              (int)BrotliBitReaderGetAvailIn(br), (int)br->bit_pos_,
              (int)n_bits, (int)*val));
  BrotliDropBits(br, n_bits);
}

static BROTLI_INLINE brotli_reg_t BrotliReadBits24(
    BrotliBitReader* const br, brotli_reg_t n_bits) {
  BROTLI_DCHECK(n_bits <= 24);
  if (BROTLI_64_BITS || (n_bits <= 16)) {
    brotli_reg_t val;
    BrotliFillBitWindow(br, n_bits);
    BrotliTakeBits(br, n_bits, &val);
    return val;
  } else {
    brotli_reg_t low_val;
    brotli_reg_t high_val;
    BrotliFillBitWindow(br, 16);
    BrotliTakeBits(br, 16, &low_val);
    BrotliFillBitWindow(br, 8);
    BrotliTakeBits(br, n_bits - 16, &high_val);
    return low_val | (high_val << 16);
  }
}

static BROTLI_INLINE brotli_reg_t BrotliReadBits32(
    BrotliBitReader* const br, brotli_reg_t n_bits) {
  BROTLI_DCHECK(n_bits <= 32);
  if (BROTLI_64_BITS || (n_bits <= 16)) {
    brotli_reg_t val;
    BrotliFillBitWindow(br, n_bits);
    BrotliTakeBits(br, n_bits, &val);
    return val;
  } else {
    brotli_reg_t low_val;
    brotli_reg_t high_val;
    BrotliFillBitWindow(br, 16);
    BrotliTakeBits(br, 16, &low_val);
    BrotliFillBitWindow(br, 16);
    BrotliTakeBits(br, n_bits - 16, &high_val);
    return low_val | (high_val << 16);
  }
}

static BROTLI_INLINE BROTLI_BOOL BrotliSafeReadBits(
    BrotliBitReader* const br, brotli_reg_t n_bits, brotli_reg_t* val) {
  BROTLI_DCHECK(n_bits <= 24);
  while (BrotliGetAvailableBits(br) < n_bits) {
    if (!BrotliPullByte(br)) {
      return BROTLI_FALSE;
    }
  }
  BrotliTakeBits(br, n_bits, val);
  return BROTLI_TRUE;
}

static BROTLI_INLINE BROTLI_BOOL BrotliSafeReadBits32(
    BrotliBitReader* const br, brotli_reg_t n_bits, brotli_reg_t* val) {
  BROTLI_DCHECK(n_bits <= 32);
  if (BROTLI_64_BITS || (n_bits <= 24)) {
    while (BrotliGetAvailableBits(br) < n_bits) {
      if (!BrotliPullByte(br)) {
        return BROTLI_FALSE;
      }
    }
    BrotliTakeBits(br, n_bits, val);
    return BROTLI_TRUE;
  } else {
    return BrotliSafeReadBits32Slow(br, n_bits, val);
  }
}

static BROTLI_INLINE BROTLI_BOOL BrotliJumpToByteBoundary(BrotliBitReader* br) {
  brotli_reg_t pad_bits_count = BrotliGetAvailableBits(br) & 0x7;
  brotli_reg_t pad_bits = 0;
  if (pad_bits_count != 0) {
    BrotliTakeBits(br, pad_bits_count, &pad_bits);
  }
  BrotliBitReaderNormalize(br);
  return TO_BROTLI_BOOL(pad_bits == 0);
}

static BROTLI_INLINE void BrotliDropBytes(BrotliBitReader* br, size_t num) {

  BROTLI_DCHECK(br->bit_pos_ == 0);
  BROTLI_DCHECK(br->val_ == 0);
  br->next_in += num;
}

static BROTLI_INLINE void BrotliCopyBytes(uint8_t* dest,
                                          BrotliBitReader* br, size_t num) {
  while (BrotliGetAvailableBits(br) >= 8 && num > 0) {
    *dest = (uint8_t)BrotliGetBitsUnmasked(br);
    BrotliDropBits(br, 8);
    ++dest;
    --num;
  }
  BrotliBitReaderNormalize(br);
  if (num > 0) {
    memcpy(dest, br->next_in, num);
    BrotliDropBytes(br, num);
  }
}

BROTLI_UNUSED_FUNCTION void BrotliBitReaderSuppressUnusedFunctions(void) {
  BROTLI_UNUSED(&BrotliBitReaderSuppressUnusedFunctions);

  BROTLI_UNUSED(&BrotliBitReaderGetAvailIn);
  BROTLI_UNUSED(&BrotliBitReaderLoadBits);
  BROTLI_UNUSED(&BrotliBitReaderRestoreState);
  BROTLI_UNUSED(&BrotliBitReaderSaveState);
  BROTLI_UNUSED(&BrotliBitReaderSetInput);
  BROTLI_UNUSED(&BrotliBitReaderUnload);
  BROTLI_UNUSED(&BrotliCheckInputAmount);
  BROTLI_UNUSED(&BrotliCopyBytes);
  BROTLI_UNUSED(&BrotliFillBitWindow16);
  BROTLI_UNUSED(&BrotliGet16BitsUnmasked);
  BROTLI_UNUSED(&BrotliGetBits);
  BROTLI_UNUSED(&BrotliGetRemainingBytes);
  BROTLI_UNUSED(&BrotliJumpToByteBoundary);
  BROTLI_UNUSED(&BrotliReadBits24);
  BROTLI_UNUSED(&BrotliReadBits32);
  BROTLI_UNUSED(&BrotliSafeGetBits);
  BROTLI_UNUSED(&BrotliSafeReadBits);
  BROTLI_UNUSED(&BrotliSafeReadBits32);
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_COMMON_VERSION_H_
#define BROTLI_COMMON_VERSION_H_

#define BROTLI_MAKE_HEX_VERSION(A, B, C) ((A << 24) | (B << 12) | C)

#define BROTLI_VERSION_MAJOR 1
#define BROTLI_VERSION_MINOR 2
#define BROTLI_VERSION_PATCH 0

#define BROTLI_VERSION BROTLI_MAKE_HEX_VERSION(                     \
  BROTLI_VERSION_MAJOR, BROTLI_VERSION_MINOR, BROTLI_VERSION_PATCH)

#define BROTLI_ABI_CURRENT  3
#define BROTLI_ABI_REVISION 0
#define BROTLI_ABI_AGE      2

#if BROTLI_VERSION_MAJOR != (BROTLI_ABI_CURRENT - BROTLI_ABI_AGE)
#error ABI/API version inconsistency
#endif

#if BROTLI_VERSION_MINOR != BROTLI_ABI_AGE
#error ABI/API version inconsistency
#endif

#if BROTLI_VERSION_PATCH != BROTLI_ABI_REVISION
#error ABI/API version inconsistency
#endif

#endif

#ifndef BROTLI_DEC_HUFFMAN_H_
#define BROTLI_DEC_HUFFMAN_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_HUFFMAN_MAX_CODE_LENGTH 15

#define BROTLI_HUFFMAN_MAX_SIZE_26 396

#define BROTLI_HUFFMAN_MAX_SIZE_258 632

#define BROTLI_HUFFMAN_MAX_SIZE_272 646

#define BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH 5

#if ((defined(BROTLI_TARGET_ARMV7) || defined(BROTLI_TARGET_ARMV8_32)) && \
  BROTLI_GNUC_HAS_ATTRIBUTE(aligned, 2, 7, 0))
#define BROTLI_HUFFMAN_CODE_FAST_LOAD
#endif

#if !defined(BROTLI_HUFFMAN_CODE_FAST_LOAD)

typedef struct {
  uint8_t bits;
  uint16_t value;
} HuffmanCode;

static BROTLI_INLINE HuffmanCode ConstructHuffmanCode(const uint8_t bits,
    const uint16_t value) {
  HuffmanCode h;
  h.bits = bits;
  h.value = value;
  return h;
}

#define BROTLI_HC_MARK_TABLE_FOR_FAST_LOAD(H)
#define BROTLI_HC_ADJUST_TABLE_INDEX(H, V) H += (V)

#define BROTLI_HC_FAST_LOAD_BITS(H) (H->bits)
#define BROTLI_HC_FAST_LOAD_VALUE(H) (H->value)

#else

typedef BROTLI_ALIGNED(4) uint32_t HuffmanCode;

static BROTLI_INLINE HuffmanCode ConstructHuffmanCode(const uint8_t bits,
    const uint16_t value) {
  return (HuffmanCode) ((value & 0xFFFF) << 16) | (bits & 0xFF);
}

#define BROTLI_HC_MARK_TABLE_FOR_FAST_LOAD(H) uint32_t __fastload_##H = (*H)
#define BROTLI_HC_ADJUST_TABLE_INDEX(H, V) H += (V); __fastload_##H = (*H)

#define BROTLI_HC_FAST_LOAD_BITS(H) ((__fastload_##H) & 0xFF)
#define BROTLI_HC_FAST_LOAD_VALUE(H) ((__fastload_##H) >> 16)
#endif

BROTLI_INTERNAL void BrotliBuildCodeLengthsHuffmanTable(HuffmanCode* root_table,
    const uint8_t* const code_lengths, uint16_t* count);

BROTLI_INTERNAL uint32_t BrotliBuildHuffmanTable(HuffmanCode* root_table,
    int root_bits, const uint16_t* const symbol_lists, uint16_t* count);

BROTLI_INTERNAL uint32_t BrotliBuildSimpleHuffmanTable(HuffmanCode* table,
    int root_bits, uint16_t* symbols, uint32_t num_symbols);

typedef struct {
  HuffmanCode** htrees;
  HuffmanCode* codes;
  uint16_t alphabet_size_max;
  uint16_t alphabet_size_limit;
  uint16_t num_htrees;
} HuffmanTreeGroup;

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_DEC_PREFIX_H_
#define BROTLI_DEC_PREFIX_H_

#ifndef THIRD_PARTY_BROTLI_COMMON_STATIC_INIT_H_
#define THIRD_PARTY_BROTLI_COMMON_STATIC_INIT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_STATIC_INIT_NONE 0

#define BROTLI_STATIC_INIT_EARLY 1

#define BROTLI_STATIC_INIT_LAZY 2

#define BROTLI_STATIC_INIT_DEFAULT BROTLI_STATIC_INIT_NONE

#if !defined(BROTLI_STATIC_INIT)
#define BROTLI_STATIC_INIT BROTLI_STATIC_INIT_DEFAULT
#endif

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE) && \
    (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_EARLY) && \
    (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_LAZY)
#error Invalid value for BROTLI_STATIC_INIT
#endif

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_EARLY)
#if defined(BROTLI_EXTERNAL_DICTIONARY_DATA)
#error BROTLI_STATIC_INIT_EARLY will fail with BROTLI_EXTERNAL_DICTIONARY_DATA
#endif
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct CmdLutElement {
  uint8_t insert_len_extra_bits;
  uint8_t copy_len_extra_bits;
  int8_t distance_code;
  uint8_t context;
  uint16_t insert_len_offset;
  uint16_t copy_len_offset;
} CmdLutElement;

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_NONE)
BROTLI_INTERNAL extern const BROTLI_MODEL("small")
    CmdLutElement kCmdLut[BROTLI_NUM_COMMAND_SYMBOLS];
#else
BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderInitCmdLut(CmdLutElement* items);
BROTLI_INTERNAL extern BROTLI_MODEL("small")
    CmdLutElement kCmdLut[BROTLI_NUM_COMMAND_SYMBOLS];
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_DEC_STATE_H_
#define BROTLI_DEC_STATE_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef enum {
  BROTLI_STATE_UNINITED,
  BROTLI_STATE_LARGE_WINDOW_BITS,
  BROTLI_STATE_INITIALIZE,
  BROTLI_STATE_METABLOCK_BEGIN,
  BROTLI_STATE_METABLOCK_HEADER,
  BROTLI_STATE_METABLOCK_HEADER_2,
  BROTLI_STATE_CONTEXT_MODES,
  BROTLI_STATE_COMMAND_BEGIN,
  BROTLI_STATE_COMMAND_INNER,
  BROTLI_STATE_COMMAND_POST_DECODE_LITERALS,
  BROTLI_STATE_COMMAND_POST_WRAP_COPY,
  BROTLI_STATE_UNCOMPRESSED,
  BROTLI_STATE_METADATA,
  BROTLI_STATE_COMMAND_INNER_WRITE,
  BROTLI_STATE_METABLOCK_DONE,
  BROTLI_STATE_COMMAND_POST_WRITE_1,
  BROTLI_STATE_COMMAND_POST_WRITE_2,
  BROTLI_STATE_BEFORE_COMPRESSED_METABLOCK_HEADER,
  BROTLI_STATE_HUFFMAN_CODE_0,
  BROTLI_STATE_HUFFMAN_CODE_1,
  BROTLI_STATE_HUFFMAN_CODE_2,
  BROTLI_STATE_HUFFMAN_CODE_3,
  BROTLI_STATE_CONTEXT_MAP_1,
  BROTLI_STATE_CONTEXT_MAP_2,
  BROTLI_STATE_TREE_GROUP,
  BROTLI_STATE_BEFORE_COMPRESSED_METABLOCK_BODY,
  BROTLI_STATE_DONE
} BrotliRunningState;

typedef enum {
  BROTLI_STATE_METABLOCK_HEADER_NONE,
  BROTLI_STATE_METABLOCK_HEADER_EMPTY,
  BROTLI_STATE_METABLOCK_HEADER_NIBBLES,
  BROTLI_STATE_METABLOCK_HEADER_SIZE,
  BROTLI_STATE_METABLOCK_HEADER_UNCOMPRESSED,
  BROTLI_STATE_METABLOCK_HEADER_RESERVED,
  BROTLI_STATE_METABLOCK_HEADER_BYTES,
  BROTLI_STATE_METABLOCK_HEADER_METADATA
} BrotliRunningMetablockHeaderState;

typedef enum {
  BROTLI_STATE_UNCOMPRESSED_NONE,
  BROTLI_STATE_UNCOMPRESSED_WRITE
} BrotliRunningUncompressedState;

typedef enum {
  BROTLI_STATE_TREE_GROUP_NONE,
  BROTLI_STATE_TREE_GROUP_LOOP
} BrotliRunningTreeGroupState;

typedef enum {
  BROTLI_STATE_CONTEXT_MAP_NONE,
  BROTLI_STATE_CONTEXT_MAP_READ_PREFIX,
  BROTLI_STATE_CONTEXT_MAP_HUFFMAN,
  BROTLI_STATE_CONTEXT_MAP_DECODE,
  BROTLI_STATE_CONTEXT_MAP_TRANSFORM
} BrotliRunningContextMapState;

typedef enum {
  BROTLI_STATE_HUFFMAN_NONE,
  BROTLI_STATE_HUFFMAN_SIMPLE_SIZE,
  BROTLI_STATE_HUFFMAN_SIMPLE_READ,
  BROTLI_STATE_HUFFMAN_SIMPLE_BUILD,
  BROTLI_STATE_HUFFMAN_COMPLEX,
  BROTLI_STATE_HUFFMAN_LENGTH_SYMBOLS
} BrotliRunningHuffmanState;

typedef enum {
  BROTLI_STATE_DECODE_UINT8_NONE,
  BROTLI_STATE_DECODE_UINT8_SHORT,
  BROTLI_STATE_DECODE_UINT8_LONG
} BrotliRunningDecodeUint8State;

typedef enum {
  BROTLI_STATE_READ_BLOCK_LENGTH_NONE,
  BROTLI_STATE_READ_BLOCK_LENGTH_SUFFIX
} BrotliRunningReadBlockLengthState;

typedef struct BrotliDecoderCompoundDictionary {
  uint8_t num_chunks;
  uint8_t block_bits;
  uint16_t br_index;
  uint32_t total_size;
  uint32_t br_offset;
  uint32_t br_length;
  uint32_t br_copied;
  const uint8_t* chunks[16];
  uint32_t chunk_offsets[16];
  uint8_t block_map[256];
} BrotliDecoderCompoundDictionary;

typedef struct BrotliMetablockHeaderArena {
  BrotliRunningTreeGroupState substate_tree_group;
  BrotliRunningContextMapState substate_context_map;
  BrotliRunningHuffmanState substate_huffman;

  brotli_reg_t sub_loop_counter;

  brotli_reg_t repeat_code_len;
  brotli_reg_t prev_code_len;

  brotli_reg_t symbol;
  brotli_reg_t repeat;
  brotli_reg_t space;

  HuffmanCode table[32];

  uint16_t* symbol_lists;

  uint16_t symbols_lists_array[BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                               BROTLI_NUM_COMMAND_SYMBOLS];

  int next_symbol[32];
  uint8_t code_length_code_lengths[BROTLI_CODE_LENGTH_CODES];

  uint16_t code_length_histo[16];

  int htree_index;
  HuffmanCode* next;

  brotli_reg_t context_index;
  brotli_reg_t max_run_length_prefix;
  brotli_reg_t code;
  HuffmanCode context_map_table[BROTLI_HUFFMAN_MAX_SIZE_272];
} BrotliMetablockHeaderArena;

typedef struct BrotliMetablockBodyArena {
  uint8_t dist_extra_bits[544];
  brotli_reg_t dist_offset[544];
} BrotliMetablockBodyArena;

struct BrotliDecoderStateStruct {
  BrotliRunningState state;

  int loop_counter;

  BrotliBitReader br;

  brotli_alloc_func alloc_func;
  brotli_free_func free_func;
  void* memory_manager_opaque;

  union {
    uint64_t u64;
    uint8_t u8[8];
  } buffer;
  brotli_reg_t buffer_length;

  int pos;
  int max_backward_distance;
  int max_distance;
  int ringbuffer_size;
  int ringbuffer_mask;
  int dist_rb_idx;
  int dist_rb[4];
  int error_code;
  int meta_block_remaining_len;

  uint8_t* ringbuffer;
  uint8_t* ringbuffer_end;
  HuffmanCode* htree_command;
  const uint8_t* context_lookup;
  uint8_t* context_map_slice;
  uint8_t* dist_context_map_slice;

  HuffmanTreeGroup literal_hgroup;
  HuffmanTreeGroup insert_copy_hgroup;
  HuffmanTreeGroup distance_hgroup;
  HuffmanCode* block_type_trees;
  HuffmanCode* block_len_trees;

  int trivial_literal_context;

  int distance_context;
  brotli_reg_t block_length[3];
  brotli_reg_t block_length_index;
  brotli_reg_t num_block_types[3];
  brotli_reg_t block_type_rb[6];
  brotli_reg_t distance_postfix_bits;
  brotli_reg_t num_direct_distance_codes;
  brotli_reg_t num_dist_htrees;
  uint8_t* dist_context_map;
  HuffmanCode* literal_htree;

  size_t rb_roundtrips;
  size_t partial_pos_out;

  brotli_reg_t mtf_upper_bound;
  uint32_t mtf[64 + 1];

  int copy_length;
  int distance_code;

  uint8_t dist_htree_index;

  brotli_decoder_metadata_start_func metadata_start_func;
  brotli_decoder_metadata_chunk_func metadata_chunk_func;
  void* metadata_callback_opaque;

  uint64_t used_input;

  BrotliRunningMetablockHeaderState substate_metablock_header;
  BrotliRunningUncompressedState substate_uncompressed;
  BrotliRunningDecodeUint8State substate_decode_uint8;
  BrotliRunningReadBlockLengthState substate_read_block_length;

  int new_ringbuffer_size;

  unsigned int is_last_metablock : 1;
  unsigned int is_uncompressed : 1;
  unsigned int is_metadata : 1;
  unsigned int should_wrap_ringbuffer : 1;
  unsigned int canny_ringbuffer_allocation : 1;
  unsigned int large_window : 1;
  unsigned int window_bits : 6;
  unsigned int size_nibbles : 8;

  brotli_reg_t num_literal_htrees;
  uint8_t* context_map;
  uint8_t* context_modes;

  BrotliSharedDictionary* dictionary;
  BrotliDecoderCompoundDictionary* compound_dictionary;

  uint32_t trivial_literal_contexts[8];

  union {
    BrotliMetablockHeaderArena header;
    BrotliMetablockBodyArena body;
  } arena;
};

typedef struct BrotliDecoderStateStruct BrotliDecoderStateInternal;
#define BrotliDecoderState BrotliDecoderStateInternal

BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderStateInit(BrotliDecoderState* s,
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);
BROTLI_INTERNAL void BrotliDecoderStateCleanup(BrotliDecoderState* s);
BROTLI_INTERNAL void BrotliDecoderStateMetablockBegin(BrotliDecoderState* s);
BROTLI_INTERNAL void BrotliDecoderStateCleanupAfterMetablock(
    BrotliDecoderState* s);
BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderHuffmanTreeGroupInit(
    BrotliDecoderState* s, HuffmanTreeGroup* group,
    brotli_reg_t alphabet_size_max, brotli_reg_t alphabet_size_limit,
    brotli_reg_t ntrees);

#define BROTLI_DECODER_ALLOC(S, L) S->alloc_func(S->memory_manager_opaque, L)

#define BROTLI_DECODER_FREE(S, X) {          \
  S->free_func(S->memory_manager_opaque, X); \
  X = NULL;                                  \
}

#define BROTLI_BLOCK_SIZE_CAP (1U << 24)

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef THIRD_PARTY_BROTLI_DEC_STATIC_INIT_H_
#define THIRD_PARTY_BROTLI_DEC_STATIC_INIT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_LAZY)
BROTLI_INTERNAL void BrotliDecoderLazyStaticInitInner(void);
BROTLI_INTERNAL void BrotliDecoderLazyStaticInit(void);
#endif

BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderEnsureStaticInit(void);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_BACKWARD_REFERENCES_HQ_H_
#define BROTLI_ENC_BACKWARD_REFERENCES_HQ_H_

#ifndef BROTLI_ENC_COMMAND_H_
#define BROTLI_ENC_COMMAND_H_

#ifndef BROTLI_ENC_FAST_LOG_H_
#define BROTLI_ENC_FAST_LOG_H_

#include <math.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static BROTLI_INLINE uint32_t Log2FloorNonZero(size_t n) {
#if defined(BROTLI_BSR32)
  return BROTLI_BSR32((uint32_t)n);
#else
  uint32_t result = 0;
  while (n >>= 1) result++;
  return result;
#endif
}

#define BROTLI_LOG2_TABLE_SIZE 256

BROTLI_INTERNAL extern const BROTLI_MODEL("small")
double kBrotliLog2Table[BROTLI_LOG2_TABLE_SIZE];

#if !defined(BROTLI_HAVE_LOG2)
#if ((defined(_MSC_VER) && _MSC_VER <= 1700) || \
     (defined(__ANDROID_API__) && __ANDROID_API__ < 18))
#define BROTLI_HAVE_LOG2 0
#else
#define BROTLI_HAVE_LOG2 1
#endif
#endif

#define LOG_2_INV 1.4426950408889634

static BROTLI_INLINE double FastLog2(size_t v) {
  if (v < BROTLI_LOG2_TABLE_SIZE) {
    return kBrotliLog2Table[v];
  }
#if !(BROTLI_HAVE_LOG2)
  return log((double)v) * LOG_2_INV;
#else
  return log2((double)v);
#endif
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_PARAMS_H_
#define BROTLI_ENC_PARAMS_H_

#ifndef BROTLI_ENC_ENCODER_DICT_H_
#define BROTLI_ENC_ENCODER_DICT_H_

#ifndef BROTLI_ENC_PREPARED_DICTIONARY_H_
#define BROTLI_ENC_PREPARED_DICTIONARY_H_

#ifndef BROTLI_ENC_MEMORY_H_
#define BROTLI_ENC_MEMORY_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if !defined(BROTLI_ENCODER_CLEANUP_ON_OOM) && \
    !defined(BROTLI_ENCODER_EXIT_ON_OOM)
#define BROTLI_ENCODER_EXIT_ON_OOM
#endif

#if !defined(BROTLI_ENCODER_EXIT_ON_OOM)
#if defined(BROTLI_EXPERIMENTAL)
#define BROTLI_ENCODER_MEMORY_MANAGER_SLOTS (48*1024)
#else
#define BROTLI_ENCODER_MEMORY_MANAGER_SLOTS 256
#endif
#else
#define BROTLI_ENCODER_MEMORY_MANAGER_SLOTS 0
#endif

typedef struct MemoryManager {
  brotli_alloc_func alloc_func;
  brotli_free_func free_func;
  void* opaque;
#if !defined(BROTLI_ENCODER_EXIT_ON_OOM)
  BROTLI_BOOL is_oom;
  size_t perm_allocated;
  size_t new_allocated;
  size_t new_freed;
  void* pointers[BROTLI_ENCODER_MEMORY_MANAGER_SLOTS];
#endif
} MemoryManager;

BROTLI_INTERNAL void BrotliInitMemoryManager(
    MemoryManager* m, brotli_alloc_func alloc_func, brotli_free_func free_func,
    void* opaque);

BROTLI_INTERNAL void* BrotliAllocate(MemoryManager* m, size_t n);
#define BROTLI_ALLOC(M, T, N)                               \
  ((N) > 0 ? ((T*)BrotliAllocate((M), (N) * sizeof(T))) : NULL)

BROTLI_INTERNAL void BrotliFree(MemoryManager* m, void* p);
#define BROTLI_FREE(M, P) { \
  BrotliFree((M), (P));     \
  P = NULL;                 \
}

#if defined(BROTLI_ENCODER_EXIT_ON_OOM)
#define BROTLI_IS_OOM(M) (!!0)
#else
#define BROTLI_IS_OOM(M) (!!(M)->is_oom)
#endif

#if defined(__clang_analyzer__) && !defined(BROTLI_ENCODER_EXIT_ON_OOM)
#define BROTLI_IS_NULL(A) ((A) == nullptr)
#else
#define BROTLI_IS_NULL(A) (!!0)
#endif

BROTLI_INTERNAL void BrotliWipeOutMemoryManager(MemoryManager* m);

#define BROTLI_ENSURE_CAPACITY(M, T, A, C, R) {                    \
  if (C < (R)) {                                                   \
    size_t _new_size = (C == 0) ? (R) : C;                         \
    T* new_array;                                                  \
    while (_new_size < (R)) _new_size *= 2;                        \
    new_array = BROTLI_ALLOC((M), T, _new_size);                   \
    if (!BROTLI_IS_OOM(M) && !BROTLI_IS_NULL(new_array) && C != 0) \
      memcpy(new_array, A, C * sizeof(T));                         \
    BROTLI_FREE((M), A);                                           \
    A = new_array;                                                 \
    C = _new_size;                                                 \
  }                                                                \
}

#define BROTLI_ENSURE_CAPACITY_APPEND(M, T, A, C, S, V) { \
  (S)++;                                                  \
  BROTLI_ENSURE_CAPACITY(M, T, A, C, S);                  \
  A[(S) - 1] = (V);                                       \
}

BROTLI_INTERNAL void* BrotliBootstrapAlloc(size_t size,
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);
BROTLI_INTERNAL void BrotliBootstrapFree(void* address, MemoryManager* m);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

static const uint32_t kPreparedDictionaryMagic = 0xDEBCEDE0;

static const uint32_t kSharedDictionaryMagic = 0xDEBCEDE1;

static const uint32_t kManagedDictionaryMagic = 0xDEBCEDE2;

static const uint32_t kLeanPreparedDictionaryMagic = 0xDEBCEDE3;

static const uint64_t kPreparedDictionaryHashMul64Long =
    BROTLI_MAKE_UINT64_T(0x1FE35A7Bu, 0xD3579BD3u);

typedef struct PreparedDictionary {
  uint32_t magic;
  uint32_t num_items;
  uint32_t source_size;
  uint32_t hash_bits;
  uint32_t bucket_bits;
  uint32_t slot_bits;

} PreparedDictionary;

BROTLI_INTERNAL PreparedDictionary* CreatePreparedDictionary(MemoryManager* m,
    const uint8_t* source, size_t source_size);

BROTLI_INTERNAL void DestroyPreparedDictionary(MemoryManager* m,
    PreparedDictionary* dictionary);

typedef struct PreparedDictionaryView {
  const uint32_t* slot_offsets;
  const uint16_t* heads;
  const uint32_t* items;
  const uint8_t* source;
  uint32_t source_size;
  uint32_t hash_shift;
  uint32_t slot_mask;
  uint64_t hash_mask;
} PreparedDictionaryView;

typedef struct CompoundDictionary {

  size_t num_chunks;
  size_t total_size;

  const PreparedDictionary* chunks[SHARED_BROTLI_MAX_COMPOUND_DICTS + 1];

  PreparedDictionaryView chunk_views[SHARED_BROTLI_MAX_COMPOUND_DICTS + 1];
  const uint8_t* chunk_source[SHARED_BROTLI_MAX_COMPOUND_DICTS + 1];
  size_t chunk_offsets[SHARED_BROTLI_MAX_COMPOUND_DICTS + 1];

  size_t num_prepared_instances_;

  PreparedDictionary* prepared_instances_[SHARED_BROTLI_MAX_COMPOUND_DICTS + 1];
} CompoundDictionary;

BROTLI_INTERNAL BROTLI_BOOL AttachPreparedDictionary(
    CompoundDictionary* compound, const PreparedDictionary* dictionary);

#endif

#ifndef BROTLI_ENC_STATIC_DICT_LUT_H_
#define BROTLI_ENC_STATIC_DICT_LUT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct DictWord {

  uint8_t len;
  uint8_t transform;
  uint16_t idx;
} DictWord;

#define BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS 32768
#define BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS 31705

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
BROTLI_INTERNAL BROTLI_BOOL BrotliEncoderInitStaticDictionaryLut(
    const BrotliDictionary* dictionary, uint16_t* buckets, DictWord* words);
BROTLI_INTERNAL extern BROTLI_MODEL("small") uint16_t
    kStaticDictionaryBuckets[BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS];
BROTLI_INTERNAL extern BROTLI_MODEL("small") DictWord
    kStaticDictionaryWords[BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS];
#else
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint16_t
    kStaticDictionaryBuckets[BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS];
BROTLI_INTERNAL extern const BROTLI_MODEL("small") DictWord
    kStaticDictionaryWords[BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS];
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct BrotliTrieNode {
  uint8_t single;
  uint8_t c;
  uint8_t len_;
  uint32_t idx_;
  uint32_t sub;
} BrotliTrieNode;

typedef struct BrotliTrie {
  BrotliTrieNode* pool;
  size_t pool_capacity;
  size_t pool_size;
  BrotliTrieNode root;
} BrotliTrie;

#if defined(BROTLI_EXPERIMENTAL)
BROTLI_INTERNAL const BrotliTrieNode* BrotliTrieSub(const BrotliTrie* trie,
    const BrotliTrieNode* node, uint8_t c);
#endif

typedef struct BrotliEncoderDictionary {
  const BrotliDictionary* words;
  uint32_t num_transforms;

  uint32_t cutoffTransformsCount;
  uint64_t cutoffTransforms;

  const uint16_t* hash_table_words;
  const uint8_t* hash_table_lengths;

  const uint16_t* buckets;
  const DictWord* dict_words;

  BrotliTrie trie;
  BROTLI_BOOL has_words_heavy;

  const struct ContextualEncoderDictionary* parent;

  uint16_t* hash_table_data_words_;
  uint8_t* hash_table_data_lengths_;
  size_t buckets_alloc_size_;
  uint16_t* buckets_data_;
  size_t dict_words_alloc_size_;
  DictWord* dict_words_data_;
  BrotliDictionary* words_instance_;
} BrotliEncoderDictionary;

typedef struct ContextualEncoderDictionary {
  BROTLI_BOOL context_based;
  uint8_t num_dictionaries;
  uint8_t context_map[SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS];
  const BrotliEncoderDictionary* dict[SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS];

  size_t num_instances_;
  BrotliEncoderDictionary instance_;
  BrotliEncoderDictionary* instances_;
} ContextualEncoderDictionary;

typedef struct SharedEncoderDictionary {

  uint32_t magic;

  CompoundDictionary compound;

  ContextualEncoderDictionary contextual;

  int max_quality;
} SharedEncoderDictionary;

typedef struct ManagedDictionary {
  uint32_t magic;
  MemoryManager memory_manager_;
  uint32_t* dictionary;
} ManagedDictionary;

BROTLI_INTERNAL void BrotliInitSharedEncoderDictionary(
    SharedEncoderDictionary* dict);

#if defined(BROTLI_EXPERIMENTAL)

BROTLI_INTERNAL BROTLI_BOOL BrotliInitCustomSharedEncoderDictionary(
    MemoryManager* m, const uint8_t* encoded_dict, size_t size,
    int quality, SharedEncoderDictionary* dict);
#endif

BROTLI_INTERNAL void BrotliCleanupSharedEncoderDictionary(
    MemoryManager* m, SharedEncoderDictionary* dict);

BROTLI_INTERNAL ManagedDictionary* BrotliCreateManagedDictionary(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

BROTLI_INTERNAL void BrotliDestroyManagedDictionary(
    ManagedDictionary* dictionary);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

typedef struct BrotliHasherParams {
  int type;
  int bucket_bits;
  int block_bits;
  int num_last_distances_to_check;
} BrotliHasherParams;

typedef struct BrotliDistanceParams {
  uint32_t distance_postfix_bits;
  uint32_t num_direct_distance_codes;
  uint32_t alphabet_size_max;
  uint32_t alphabet_size_limit;
  size_t max_distance;
} BrotliDistanceParams;

typedef struct BrotliEncoderParams {
  BrotliEncoderMode mode;
  int quality;
  int lgwin;
  int lgblock;
  size_t stream_offset;
  size_t size_hint;
  BROTLI_BOOL disable_literal_context_modeling;
  BROTLI_BOOL large_window;
  BrotliHasherParams hasher;
  BrotliDistanceParams dist;

  SharedEncoderDictionary dictionary;
  int base64_mode;
  size_t max_base64_regions;
  size_t min_base64_region_len;
  BrotliEncoderSimdHasher simd_hasher;
  BROTLI_BOOL hasher_opt;
} BrotliEncoderParams;

#endif

#ifndef BROTLI_ENC_PREFIX_H_
#define BROTLI_ENC_PREFIX_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static BROTLI_INLINE void PrefixEncodeCopyDistance(size_t distance_code,
                                                   size_t num_direct_codes,
                                                   size_t postfix_bits,
                                                   uint16_t* code,
                                                   uint32_t* extra_bits) {
  if (distance_code < BROTLI_NUM_DISTANCE_SHORT_CODES + num_direct_codes) {
    *code = (uint16_t)distance_code;
    *extra_bits = 0;
    return;
  } else {
    size_t dist = ((size_t)1 << (postfix_bits + 2u)) +
        (distance_code - BROTLI_NUM_DISTANCE_SHORT_CODES - num_direct_codes);
    size_t bucket = Log2FloorNonZero(dist) - 1;
    size_t postfix_mask = (1u << postfix_bits) - 1;
    size_t postfix = dist & postfix_mask;
    size_t prefix = (dist >> bucket) & 1;
    size_t offset = (2 + prefix) << bucket;
    size_t nbits = bucket - postfix_bits;
    *code = (uint16_t)((nbits << 10) |
        (BROTLI_NUM_DISTANCE_SHORT_CODES + num_direct_codes +
         ((2 * (nbits - 1) + prefix) << postfix_bits) + postfix));
    *extra_bits = (uint32_t)((dist - offset) >> postfix_bits);
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL extern const BROTLI_MODEL("small")
uint32_t kBrotliInsBase[BROTLI_NUM_INS_COPY_CODES];
BROTLI_INTERNAL extern const BROTLI_MODEL("small")
uint32_t kBrotliInsExtra[BROTLI_NUM_INS_COPY_CODES];
BROTLI_INTERNAL extern const BROTLI_MODEL("small")
uint32_t kBrotliCopyBase[BROTLI_NUM_INS_COPY_CODES];
BROTLI_INTERNAL extern const BROTLI_MODEL("small")
uint32_t kBrotliCopyExtra[BROTLI_NUM_INS_COPY_CODES];

static BROTLI_INLINE uint16_t GetInsertLengthCode(size_t insertlen) {
  if (insertlen < 6) {
    return (uint16_t)insertlen;
  } else if (insertlen < 130) {
    uint32_t nbits = Log2FloorNonZero(insertlen - 2) - 1u;
    return (uint16_t)((nbits << 1) + ((insertlen - 2) >> nbits) + 2);
  } else if (insertlen < 2114) {
    return (uint16_t)(Log2FloorNonZero(insertlen - 66) + 10);
  } else if (insertlen < 6210) {
    return 21u;
  } else if (insertlen < 22594) {
    return 22u;
  } else {
    return 23u;
  }
}

static BROTLI_INLINE uint16_t GetCopyLengthCode(size_t copylen) {
  if (copylen < 10) {
    return (uint16_t)(copylen - 2);
  } else if (copylen < 134) {
    uint32_t nbits = Log2FloorNonZero(copylen - 6) - 1u;
    return (uint16_t)((nbits << 1) + ((copylen - 6) >> nbits) + 4);
  } else if (copylen < 2118) {
    return (uint16_t)(Log2FloorNonZero(copylen - 70) + 12);
  } else {
    return 23u;
  }
}

static BROTLI_INLINE uint16_t CombineLengthCodes(
    uint16_t inscode, uint16_t copycode, BROTLI_BOOL use_last_distance) {
  uint16_t bits64 =
      (uint16_t)((copycode & 0x7u) | ((inscode & 0x7u) << 3u));
  if (use_last_distance && inscode < 8u && copycode < 16u) {
    return (copycode < 8u) ? bits64 : (bits64 | 64u);
  } else {

    uint32_t offset = 2u * ((copycode >> 3u) + 3u * (inscode >> 3u));

    offset = (offset << 5u) + 0x40u + ((0x520D40u >> offset) & 0xC0u);
    return (uint16_t)(offset | bits64);
  }
}

static BROTLI_INLINE void GetLengthCode(size_t insertlen, size_t copylen,
                                        BROTLI_BOOL use_last_distance,
                                        uint16_t* code) {
  uint16_t inscode = GetInsertLengthCode(insertlen);
  uint16_t copycode = GetCopyLengthCode(copylen);
  *code = CombineLengthCodes(inscode, copycode, use_last_distance);
}

static BROTLI_INLINE uint32_t GetInsertBase(uint16_t inscode) {
  return kBrotliInsBase[inscode];
}

static BROTLI_INLINE uint32_t GetInsertExtra(uint16_t inscode) {
  return kBrotliInsExtra[inscode];
}

static BROTLI_INLINE uint32_t GetCopyBase(uint16_t copycode) {
  return kBrotliCopyBase[copycode];
}

static BROTLI_INLINE uint32_t GetCopyExtra(uint16_t copycode) {
  return kBrotliCopyExtra[copycode];
}

typedef struct Command {
  uint32_t insert_len_;

  uint32_t copy_len_;

  uint32_t dist_extra_;
  uint16_t cmd_prefix_;

  uint16_t dist_prefix_;
} Command;

static BROTLI_INLINE void InitCommand(Command* self,
    const BrotliDistanceParams* dist, size_t insertlen,
    size_t copylen, int copylen_code_delta, size_t distance_code) {

  uint32_t delta = (uint8_t)((int8_t)copylen_code_delta);
  self->insert_len_ = (uint32_t)insertlen;
  self->copy_len_ = (uint32_t)(copylen | (delta << 25));

  PrefixEncodeCopyDistance(
      distance_code, dist->num_direct_distance_codes,
      dist->distance_postfix_bits, &self->dist_prefix_, &self->dist_extra_);
  GetLengthCode(
      insertlen, (size_t)((int)copylen + copylen_code_delta),
      TO_BROTLI_BOOL((self->dist_prefix_ & 0x3FF) == 0), &self->cmd_prefix_);
}

static BROTLI_INLINE void InitInsertCommand(Command* self, size_t insertlen) {
  self->insert_len_ = (uint32_t)insertlen;
  self->copy_len_ = 4 << 25;
  self->dist_extra_ = 0;
  self->dist_prefix_ = BROTLI_NUM_DISTANCE_SHORT_CODES;
  GetLengthCode(insertlen, 4, BROTLI_FALSE, &self->cmd_prefix_);
}

static BROTLI_INLINE uint32_t CommandRestoreDistanceCode(
    const Command* self, const BrotliDistanceParams* dist) {
  if ((self->dist_prefix_ & 0x3FFu) <
      BROTLI_NUM_DISTANCE_SHORT_CODES + dist->num_direct_distance_codes) {
    return self->dist_prefix_ & 0x3FFu;
  } else {
    uint32_t dcode = self->dist_prefix_ & 0x3FFu;
    uint32_t nbits = self->dist_prefix_ >> 10;
    uint32_t extra = self->dist_extra_;
    uint32_t postfix_mask = (1U << dist->distance_postfix_bits) - 1U;
    uint32_t hcode = (dcode - dist->num_direct_distance_codes -
        BROTLI_NUM_DISTANCE_SHORT_CODES) >>
        dist->distance_postfix_bits;
    uint32_t lcode = (dcode - dist->num_direct_distance_codes -
        BROTLI_NUM_DISTANCE_SHORT_CODES) & postfix_mask;
    uint32_t offset = ((2U + (hcode & 1U)) << nbits) - 4U;
    return ((offset + extra) << dist->distance_postfix_bits) + lcode +
        dist->num_direct_distance_codes + BROTLI_NUM_DISTANCE_SHORT_CODES;
  }
}

static BROTLI_INLINE uint32_t CommandDistanceContext(const Command* self) {
  uint32_t r = self->cmd_prefix_ >> 6;
  uint32_t c = self->cmd_prefix_ & 7;
  if ((r == 0 || r == 2 || r == 4 || r == 7) && (c <= 2)) {
    return c;
  }
  return 3;
}

static BROTLI_INLINE uint32_t CommandCopyLen(const Command* self) {
  return self->copy_len_ & 0x1FFFFFF;
}

static BROTLI_INLINE uint32_t CommandCopyLenCode(const Command* self) {
  uint32_t modifier = self->copy_len_ >> 25;
  int32_t delta = (int8_t)((uint8_t)(modifier | ((modifier & 0x40) << 1)));
  return (uint32_t)((int32_t)(self->copy_len_ & 0x1FFFFFF) + delta);
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_HASH_H_
#define BROTLI_ENC_HASH_H_

#ifndef BROTLI_ENC_FIND_MATCH_LENGTH_H_
#define BROTLI_ENC_FIND_MATCH_LENGTH_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if defined(BROTLI_TZCNT64) && BROTLI_64_BITS && BROTLI_LITTLE_ENDIAN
static BROTLI_INLINE size_t FindMatchLengthWithLimit(const uint8_t* s1,
                                                     const uint8_t* s2,
                                                     size_t limit) {
  const uint8_t *s1_orig = s1;
  for (; limit >= 8; limit -= 8) {
    uint64_t x = BROTLI_UNALIGNED_LOAD64LE(s2) ^
                 BROTLI_UNALIGNED_LOAD64LE(s1);
    s2 += 8;
    if (x != 0) {
      size_t matching_bits = (size_t)BROTLI_TZCNT64(x);
      return (size_t)(s1 - s1_orig) + (matching_bits >> 3);
    }
    s1 += 8;
  }
  while (limit && *s1 == *s2) {
    limit--;
    ++s2;
    ++s1;
  }
  return (size_t)(s1 - s1_orig);
}
#else
static BROTLI_INLINE size_t FindMatchLengthWithLimit(const uint8_t* s1,
                                                     const uint8_t* s2,
                                                     size_t limit) {
  size_t matched = 0;
  const uint8_t* s2_limit = s2 + limit;
  const uint8_t* s2_ptr = s2;

  while (s2_ptr <= s2_limit - 4 &&
         BrotliUnalignedRead32(s2_ptr) ==
         BrotliUnalignedRead32(s1 + matched)) {
    s2_ptr += 4;
    matched += 4;
  }
  while ((s2_ptr < s2_limit) && (s1[matched] == *s2_ptr)) {
    ++s2_ptr;
    ++matched;
  }
  return matched;
}
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef THIRD_PARTY_BROTLI_ENC_HASH_BASE_H_
#define THIRD_PARTY_BROTLI_ENC_HASH_BASE_H_

static const uint32_t kHashMul32 = 0x1E35A7BD;
static const uint64_t kHashMul64 =
    BROTLI_MAKE_UINT64_T(0x1FE35A7Bu, 0xD3579BD3u);

static BROTLI_INLINE uint32_t Hash14(const uint8_t* data) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return h >> (32 - 14);
}

static BROTLI_INLINE uint32_t Hash15(const uint8_t* data) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return h >> (32 - 15);
}

#endif

#ifndef THIRD_PARTY_BROTLI_ENC_MATCHING_TAG_MASK_H_
#define THIRD_PARTY_BROTLI_ENC_MATCHING_TAG_MASK_H_

#if defined(__SSE2__) || defined(_M_AMD64) || \
    (defined(_M_IX86) && defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
  #if !defined(_MSC_VER) || (_MSC_VER >= 1800)

    #define SUPPORTS_SSE_2
  #endif
#endif

#if defined(SUPPORTS_SSE_2)
#include <immintrin.h>
#endif

static BROTLI_INLINE uint64_t GetMatchingTagMask(
    size_t chunk_count, const uint8_t tag,
    const uint8_t* BROTLI_RESTRICT tag_bucket, const size_t head) {
  uint64_t matches = 0;
#if defined(SUPPORTS_SSE_2)
  const __m128i comparison_mask = _mm_set1_epi8((char)tag);
  size_t i;
  for (i = 0; i < chunk_count && i < 4; i++) {
    const __m128i chunk =
        _mm_loadu_si128((const __m128i*)(const void*)(tag_bucket + 16 * i));
    const __m128i equal_mask = _mm_cmpeq_epi8(chunk, comparison_mask);
    matches |= (uint64_t)_mm_movemask_epi8(equal_mask) << 16 * i;
  }
#else
  const int chunk_size = sizeof(size_t);
  const size_t shift_amount = ((chunk_size * 8) - chunk_size);
  const size_t xFF = ~((size_t)0);
  const size_t x01 = xFF / 0xFF;
  const size_t x80 = x01 << 7;
  const size_t splat_char = tag * x01;
  int i = ((int)chunk_count * 16) - chunk_size;
  BROTLI_DCHECK((sizeof(size_t) == 4) || (sizeof(size_t) == 8));
  {
#if BROTLI_LITTLE_ENDIAN
  const size_t extractMagic = (xFF / 0x7F) >> chunk_size;
  do {
      size_t chunk = BrotliUnalignedReadSizeT(&tag_bucket[i]);
      chunk ^= splat_char;
      chunk = (((chunk | x80) - x01) | chunk) & x80;
      matches <<= chunk_size;
      matches |= (chunk * extractMagic) >> shift_amount;
      i -= chunk_size;
  } while (i >= 0);
#else
  const size_t msb = xFF ^ (xFF >> 1);
  const size_t extractMagic = (msb / 0x1FF) | msb;
  do {
      size_t chunk = BrotliUnalignedReadSizeT(&tag_bucket[i]);
      chunk ^= splat_char;
      chunk = (((chunk | x80) - x01) | chunk) & x80;
      matches <<= chunk_size;
      matches |= ((chunk >> 7) * extractMagic) >> shift_amount;
      i -= chunk_size;
  } while (i >= 0);
#endif
  }
  matches = ~matches;
#endif
  if (chunk_count == 1) return BrotliRotateRight16((uint16_t)matches, head);
  if (chunk_count == 2) return BrotliRotateRight32((uint32_t)matches, head);
  return BrotliRotateRight64(matches, head);
}

#undef SUPPORTS_SSE_2

#endif

#ifndef BROTLI_ENC_QUALITY_H_
#define BROTLI_ENC_QUALITY_H_

#define FAST_ONE_PASS_COMPRESSION_QUALITY 0
#define FAST_TWO_PASS_COMPRESSION_QUALITY 1
#define ZOPFLIFICATION_QUALITY 10
#define HQ_ZOPFLIFICATION_QUALITY 11

#define MAX_QUALITY_FOR_STATIC_ENTROPY_CODES 2
#define MIN_QUALITY_FOR_BLOCK_SPLIT 4
#define MIN_QUALITY_FOR_NONZERO_DISTANCE_PARAMS 4
#define MIN_QUALITY_FOR_OPTIMIZE_HISTOGRAMS 4
#define MIN_QUALITY_FOR_EXTENSIVE_REFERENCE_SEARCH 5
#define MIN_QUALITY_FOR_CONTEXT_MODELING 5
#define MIN_QUALITY_FOR_HQ_CONTEXT_MODELING 7
#define MIN_QUALITY_FOR_HQ_BLOCK_SPLITTING 10

#define MAX_NUM_DELAYED_SYMBOLS 0x2FFF

static BROTLI_INLINE size_t MaxHashTableSize(int quality) {
  return quality == FAST_ONE_PASS_COMPRESSION_QUALITY ? 1 << 15 : 1 << 17;
}

#define MAX_ZOPFLI_LEN_QUALITY_10 150
#define MAX_ZOPFLI_LEN_QUALITY_11 325

#define BROTLI_LONG_COPY_QUICK_STEP 16384

static BROTLI_INLINE size_t MaxZopfliLen(const BrotliEncoderParams* params) {
  return params->quality <= 10 ?
      MAX_ZOPFLI_LEN_QUALITY_10 :
      MAX_ZOPFLI_LEN_QUALITY_11;
}

static BROTLI_INLINE size_t MaxZopfliCandidates(
  const BrotliEncoderParams* params) {
  return params->quality <= 10 ? 1 : 5;
}

static BROTLI_INLINE void SanitizeParams(BrotliEncoderParams* params) {
  params->quality = BROTLI_MIN(int, BROTLI_MAX_QUALITY,
      BROTLI_MAX(int, BROTLI_MIN_QUALITY, params->quality));
  if (params->quality <= MAX_QUALITY_FOR_STATIC_ENTROPY_CODES) {
    params->large_window = BROTLI_FALSE;
  }
  if (params->lgwin < BROTLI_MIN_WINDOW_BITS) {
    params->lgwin = BROTLI_MIN_WINDOW_BITS;
  } else {
    int max_lgwin = params->large_window ? BROTLI_LARGE_MAX_WINDOW_BITS :
                                           BROTLI_MAX_WINDOW_BITS;
    if (params->lgwin > max_lgwin) params->lgwin = max_lgwin;
  }
}

static BROTLI_INLINE int ComputeLgBlock(const BrotliEncoderParams* params) {
  int lgblock = params->lgblock;
  if (params->quality == FAST_ONE_PASS_COMPRESSION_QUALITY ||
      params->quality == FAST_TWO_PASS_COMPRESSION_QUALITY) {
    lgblock = params->lgwin;
  } else if (params->quality < MIN_QUALITY_FOR_BLOCK_SPLIT) {
    lgblock = 14;
  } else if (lgblock == 0) {
    lgblock = 16;
    if (params->quality >= 9 && params->lgwin > lgblock) {
      lgblock = BROTLI_MIN(int, 18, params->lgwin);
    }
  } else {
    lgblock = BROTLI_MIN(int, BROTLI_MAX_INPUT_BLOCK_BITS,
        BROTLI_MAX(int, BROTLI_MIN_INPUT_BLOCK_BITS, lgblock));
  }
  return lgblock;
}

static BROTLI_INLINE int ComputeRbBits(const BrotliEncoderParams* params) {
  return 1 + BROTLI_MAX(int, params->lgwin, params->lgblock);
}

static BROTLI_INLINE size_t MaxMetablockSize(
    const BrotliEncoderParams* params) {
  int bits =
      BROTLI_MIN(int, ComputeRbBits(params), BROTLI_MAX_INPUT_BLOCK_BITS);
  return (size_t)1 << bits;
}

static BROTLI_INLINE size_t LiteralSpreeLengthForSparseSearch(
    const BrotliEncoderParams* params) {
  return params->quality < 9 ? 64 : 512;
}

#if defined(BROTLI_MAX_SIMD_QUALITY)
static BROTLI_INLINE BROTLI_BOOL
ShouldUseSimdHasher(const BrotliEncoderParams* params) {
  int max_quality;
  if (params->simd_hasher == BROTLI_SIMD_HASHER_DISABLE) {
    return BROTLI_FALSE;
  }
  max_quality = BROTLI_MAX_RECOMMENDED_SIMD_QUALITY;

  if (params->simd_hasher == BROTLI_SIMD_HASHER_ENABLE) {
    max_quality = BROTLI_MAX_SIMD_QUALITY;
  }
  return TO_BROTLI_BOOL(params->quality <= max_quality);
}
#endif

static BROTLI_INLINE void ChooseHasher(const BrotliEncoderParams* params,
                                       BrotliHasherParams* hparams) {
  if (params->quality > 9) {
    hparams->type = 10;
  } else if (params->quality == 4 && params->size_hint >= (1 << 20)) {
    hparams->type = 54;
  } else if (params->quality < 5) {
    hparams->type = params->quality;
  } else if (params->lgwin <= 16) {
    hparams->type = params->quality < 7 ? 40 : params->quality < 9 ? 41 : 42;
  } else if (params->size_hint >= (1 << 20) && params->lgwin >= 19) {
#if defined(BROTLI_MAX_SIMD_QUALITY)
    hparams->type = ShouldUseSimdHasher(params) ? 68 : 6;
#else
    hparams->type = 6;
#endif
    hparams->block_bits = params->quality - 1;
    hparams->bucket_bits = 15;
    hparams->num_last_distances_to_check =
        params->quality < 7 ? 4 : params->quality < 9 ? 10 : 16;
  } else {

#if defined(BROTLI_MAX_SIMD_QUALITY)
    hparams->type =
        ShouldUseSimdHasher(params) ? (params->hasher_opt ? 59 : 58) : 5;
#else
    hparams->type = 5;
#endif
    hparams->block_bits = params->quality - 1;
    hparams->bucket_bits = params->quality < 7 ? 14 : 15;
    hparams->num_last_distances_to_check =
        params->quality < 7 ? 4 : params->quality < 9 ? 10 : 16;
  }

  if (params->lgwin > 24) {

    if (hparams->type == 3) {
      hparams->type = 35;
    }
    if (hparams->type == 54) {
      hparams->type = 55;
    }
    if (hparams->type == 6 || hparams->type == 68) {
      hparams->type = 65;
    }
  }
}

#endif

#ifndef BROTLI_ENC_STATIC_DICT_H_
#define BROTLI_ENC_STATIC_DICT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_MAX_STATIC_DICTIONARY_MATCH_LEN 37
static const uint32_t kInvalidMatch = 0xFFFFFFF;

BROTLI_INTERNAL BROTLI_BOOL BrotliFindAllStaticDictionaryMatches(
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* data, size_t min_length, size_t max_length,
    uint32_t* matches);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct {
  size_t start_literal_pos;
  size_t length;
} Base64Region;

typedef struct {

  void* extra[4];

  BROTLI_BOOL is_setup_;

  size_t dict_num_lookups;
  size_t dict_num_matches;

  BrotliHasherParams params;

  BROTLI_BOOL is_prepared_;

  Base64Region* base64_regions;
  size_t num_base64_regions;
} HasherCommon;

#define score_t size_t

static const uint32_t kCutoffTransformsCount = 10;

static const uint64_t kCutoffTransforms =
    BROTLI_MAKE_UINT64_T(0x071B520A, 0xDA2D3200);

typedef struct HasherSearchResult {
  size_t len;
  size_t distance;
  score_t score;
  int len_code_delta;
} HasherSearchResult;

static BROTLI_INLINE void PrepareDistanceCache(
    int* BROTLI_RESTRICT distance_cache, const int num_distances) {
  if (num_distances > 4) {
    int last_distance = distance_cache[0];
    distance_cache[4] = last_distance - 1;
    distance_cache[5] = last_distance + 1;
    distance_cache[6] = last_distance - 2;
    distance_cache[7] = last_distance + 2;
    distance_cache[8] = last_distance - 3;
    distance_cache[9] = last_distance + 3;
    if (num_distances > 10) {
      int next_last_distance = distance_cache[1];
      distance_cache[10] = next_last_distance - 1;
      distance_cache[11] = next_last_distance + 1;
      distance_cache[12] = next_last_distance - 2;
      distance_cache[13] = next_last_distance + 2;
      distance_cache[14] = next_last_distance - 3;
      distance_cache[15] = next_last_distance + 3;
    }
  }
}

#define BROTLI_LITERAL_BYTE_SCORE 135
#define BROTLI_DISTANCE_BIT_PENALTY 30

#define BROTLI_SCORE_BASE (BROTLI_DISTANCE_BIT_PENALTY * 8 * sizeof(size_t))

static BROTLI_INLINE score_t BackwardReferenceScore(
    size_t copy_length, size_t backward_reference_offset) {
  return BROTLI_SCORE_BASE + BROTLI_LITERAL_BYTE_SCORE * (score_t)copy_length -
      BROTLI_DISTANCE_BIT_PENALTY * Log2FloorNonZero(backward_reference_offset);
}

static BROTLI_INLINE size_t MinimumBetterLength(score_t score) {
  return (score - (BROTLI_SCORE_BASE + 15)) / BROTLI_LITERAL_BYTE_SCORE;
}

static BROTLI_INLINE score_t BackwardReferenceScoreUsingLastDistance(
    size_t copy_length) {
  return BROTLI_LITERAL_BYTE_SCORE * (score_t)copy_length +
      BROTLI_SCORE_BASE + 15;
}

static BROTLI_INLINE score_t BackwardReferencePenaltyUsingLastDistance(
    size_t distance_short_code) {
  return (score_t)39 + ((0x1CA10 >> (distance_short_code & 0xE)) & 0xE);
}

static BROTLI_INLINE BROTLI_BOOL TestStaticDictionaryItem(
    const BrotliEncoderDictionary* dictionary, size_t len, size_t word_idx,
    const uint8_t* data, size_t max_length, size_t max_backward,
    size_t max_distance, HasherSearchResult* out) {
  size_t offset;
  size_t matchlen;
  size_t backward;
  score_t score;
  offset = dictionary->words->offsets_by_length[len] + len * word_idx;
  if (len > max_length) {
    return BROTLI_FALSE;
  }

  matchlen =
      FindMatchLengthWithLimit(data, &dictionary->words->data[offset], len);
  if (matchlen + dictionary->cutoffTransformsCount <= len || matchlen == 0) {
    return BROTLI_FALSE;
  }
  {
    size_t cut = len - matchlen;
    size_t transform_id = (cut << 2) +
        (size_t)((dictionary->cutoffTransforms >> (cut * 6)) & 0x3F);
    backward = max_backward + 1 + word_idx +
        (transform_id << dictionary->words->size_bits_by_length[len]);
  }
  if (backward > max_distance) {
    return BROTLI_FALSE;
  }
  score = BackwardReferenceScore(matchlen, backward);
  if (score < out->score) {
    return BROTLI_FALSE;
  }
  out->len = matchlen;
  out->len_code_delta = (int)len - (int)matchlen;
  out->distance = backward;
  out->score = score;
  return BROTLI_TRUE;
}

static BROTLI_INLINE void SearchInStaticDictionary(
    const BrotliEncoderDictionary* dictionary,
    HasherCommon* common, const uint8_t* data, size_t max_length,
    size_t max_backward, size_t max_distance,
    HasherSearchResult* out, BROTLI_BOOL shallow) {
  size_t key;
  size_t i;
  if (common->dict_num_matches < (common->dict_num_lookups >> 7)) {
    return;
  }
  key = Hash14(data) << 1;
  for (i = 0; i < (shallow ? 1u : 2u); ++i, ++key) {
    common->dict_num_lookups++;
    if (dictionary->hash_table_lengths[key] != 0) {
      BROTLI_BOOL item_matches = TestStaticDictionaryItem(
          dictionary, dictionary->hash_table_lengths[key],
          dictionary->hash_table_words[key], data,
          max_length, max_backward, max_distance, out);
      if (item_matches) {
        common->dict_num_matches++;
      }
    }
  }
}

typedef struct BackwardMatch {
  uint32_t distance;
  uint32_t length_and_code;
} BackwardMatch;

static BROTLI_INLINE void InitBackwardMatch(BackwardMatch* self,
    size_t dist, size_t len) {
  self->distance = (uint32_t)dist;
  self->length_and_code = (uint32_t)(len << 5);
}

static BROTLI_INLINE void InitDictionaryBackwardMatch(BackwardMatch* self,
    size_t dist, size_t len, size_t len_code) {
  self->distance = (uint32_t)dist;
  self->length_and_code =
      (uint32_t)((len << 5) | (len == len_code ? 0 : len_code));
}

static BROTLI_INLINE size_t BackwardMatchLength(const BackwardMatch* self) {
  return self->length_and_code >> 5;
}

static BROTLI_INLINE size_t BackwardMatchLengthCode(const BackwardMatch* self) {
  size_t code = self->length_and_code & 31;
  return code ? code : BackwardMatchLength(self);
}

#define EXPAND_CAT(a, b) CAT(a, b)
#define CAT(a, b) a ## b
#define FN(X) EXPAND_CAT(X, HASHER())

#define HASHER() H10
#define BUCKET_BITS 17
#define MAX_TREE_SEARCH_DEPTH 64
#define MAX_TREE_COMP_LENGTH 128

#define HashToBinaryTree HASHER()

#define BUCKET_SIZE (1 << BUCKET_BITS)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) {
  return MAX_TREE_COMP_LENGTH;
}

static uint32_t FN(HashBytes)(const uint8_t* BROTLI_RESTRICT data) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return h >> (32 - BUCKET_BITS);
}

typedef struct HashToBinaryTree {

  size_t window_mask_;

  uint32_t* buckets_;

  uint32_t invalid_pos_;

  uint32_t* forest_;
} HashToBinaryTree;

static void FN(Initialize)(
    HasherCommon* common, HashToBinaryTree* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->buckets_ = (uint32_t*)common->extra[0];
  self->forest_ = (uint32_t*)common->extra[1];

  self->window_mask_ = (1u << params->lgwin) - 1u;
  self->invalid_pos_ = (uint32_t)(0 - self->window_mask_);
}

static void FN(Prepare)
    (HashToBinaryTree* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t invalid_pos = self->invalid_pos_;
  uint32_t i;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  BROTLI_UNUSED(data);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  for (i = 0; i < BUCKET_SIZE; i++) {
    buckets[i] = invalid_pos;
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t num_nodes = (size_t)1 << params->lgwin;
  if (one_shot && input_size < num_nodes) {
    num_nodes = input_size;
  }
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE;
  alloc_size[1] = 2 * sizeof(uint32_t) * num_nodes;
}

static BROTLI_INLINE size_t FN(LeftChildIndex)(
    HashToBinaryTree* BROTLI_RESTRICT self,
    const size_t pos) {
  return 2 * (pos & self->window_mask_);
}

static BROTLI_INLINE size_t FN(RightChildIndex)(
    HashToBinaryTree* BROTLI_RESTRICT self,
    const size_t pos) {
  return 2 * (pos & self->window_mask_) + 1;
}

static BROTLI_INLINE BackwardMatch* FN(StoreAndFindMatches)(
    HashToBinaryTree* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t cur_ix, const size_t ring_buffer_mask, const size_t max_length,
    const size_t max_backward, size_t* const BROTLI_RESTRICT best_len,
    BackwardMatch* BROTLI_RESTRICT matches) {
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  const size_t max_comp_len =
      BROTLI_MIN(size_t, max_length, MAX_TREE_COMP_LENGTH);
  const BROTLI_BOOL should_reroot_tree =
      TO_BROTLI_BOOL(max_length >= MAX_TREE_COMP_LENGTH);
  const uint32_t key = FN(HashBytes)(&data[cur_ix_masked]);
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  uint32_t* BROTLI_RESTRICT forest = self->forest_;
  size_t prev_ix = buckets[key];

  size_t node_left = FN(LeftChildIndex)(self, cur_ix);

  size_t node_right = FN(RightChildIndex)(self, cur_ix);

  size_t best_len_left = 0;

  size_t best_len_right = 0;
  size_t depth_remaining;
  if (should_reroot_tree) {
    buckets[key] = (uint32_t)cur_ix;
  }
  for (depth_remaining = MAX_TREE_SEARCH_DEPTH; ; --depth_remaining) {
    const size_t backward = cur_ix - prev_ix;
    const size_t prev_ix_masked = prev_ix & ring_buffer_mask;
    if (backward == 0 || backward > max_backward || depth_remaining == 0) {
      if (should_reroot_tree) {
        forest[node_left] = self->invalid_pos_;
        forest[node_right] = self->invalid_pos_;
      }
      break;
    }
    {
      const size_t cur_len = BROTLI_MIN(size_t, best_len_left, best_len_right);
      size_t len;
      BROTLI_DCHECK(cur_len <= MAX_TREE_COMP_LENGTH);
      len = cur_len +
          FindMatchLengthWithLimit(&data[cur_ix_masked + cur_len],
                                   &data[prev_ix_masked + cur_len],
                                   max_length - cur_len);
      BROTLI_DCHECK(
          0 == memcmp(&data[cur_ix_masked], &data[prev_ix_masked], len));
      if (matches && len > *best_len) {
        *best_len = len;
        InitBackwardMatch(matches++, backward, len);
      }
      if (len >= max_comp_len) {
        if (should_reroot_tree) {
          forest[node_left] = forest[FN(LeftChildIndex)(self, prev_ix)];
          forest[node_right] = forest[FN(RightChildIndex)(self, prev_ix)];
        }
        break;
      }
      if (data[cur_ix_masked + len] > data[prev_ix_masked + len]) {
        best_len_left = len;
        if (should_reroot_tree) {
          forest[node_left] = (uint32_t)prev_ix;
        }
        node_left = FN(RightChildIndex)(self, prev_ix);
        prev_ix = forest[node_left];
      } else {
        best_len_right = len;
        if (should_reroot_tree) {
          forest[node_right] = (uint32_t)prev_ix;
        }
        node_right = FN(LeftChildIndex)(self, prev_ix);
        prev_ix = forest[node_right];
      }
    }
  }
  return matches;
}

static BROTLI_INLINE size_t FN(FindAllMatches)(
    HashToBinaryTree* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const BrotliEncoderParams* params,
    BackwardMatch* matches) {
  BackwardMatch* const orig_matches = matches;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  size_t best_len = 1;
  const size_t short_match_max_backward =
      params->quality != HQ_ZOPFLIFICATION_QUALITY ? 16 : 64;
  size_t stop = cur_ix - short_match_max_backward;
  uint32_t dict_matches[BROTLI_MAX_STATIC_DICTIONARY_MATCH_LEN + 1];
  size_t i;
  if (cur_ix < short_match_max_backward) { stop = 0; }
  for (i = cur_ix - 1; i > stop && best_len <= 2; --i) {
    size_t prev_ix = i;
    const size_t backward = cur_ix - prev_ix;
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      break;
    }
    prev_ix &= ring_buffer_mask;
    if (data[cur_ix_masked] != data[prev_ix] ||
        data[cur_ix_masked + 1] != data[prev_ix + 1]) {
      continue;
    }
    {
      const size_t len =
          FindMatchLengthWithLimit(&data[prev_ix], &data[cur_ix_masked],
                                   max_length);
      if (len > best_len) {
        best_len = len;
        InitBackwardMatch(matches++, backward, len);
      }
    }
  }
  if (best_len < max_length) {
    matches = FN(StoreAndFindMatches)(self, data, cur_ix,
        ring_buffer_mask, max_length, max_backward, &best_len, matches);
  }
  for (i = 0; i <= BROTLI_MAX_STATIC_DICTIONARY_MATCH_LEN; ++i) {
    dict_matches[i] = kInvalidMatch;
  }
  {
    size_t minlen = BROTLI_MAX(size_t, 4, best_len + 1);
    if (BrotliFindAllStaticDictionaryMatches(dictionary,
        &data[cur_ix_masked], minlen, max_length, &dict_matches[0])) {
      size_t maxlen = BROTLI_MIN(
          size_t, BROTLI_MAX_STATIC_DICTIONARY_MATCH_LEN, max_length);
      size_t l;
      for (l = minlen; l <= maxlen; ++l) {
        uint32_t dict_id = dict_matches[l];
        if (dict_id < kInvalidMatch) {
          size_t distance = dictionary_distance + (dict_id >> 5) + 1;
          if (distance <= params->dist.max_distance) {
            InitDictionaryBackwardMatch(matches++, distance, l, dict_id & 31);
          }
        }
      }
    }
  }
  return (size_t)(matches - orig_matches);
}

static BROTLI_INLINE void FN(Store)(HashToBinaryTree* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix) {

  const size_t max_backward = self->window_mask_ - BROTLI_WINDOW_GAP + 1;
  FN(StoreAndFindMatches)(self, data, ix, mask, MAX_TREE_COMP_LENGTH,
      max_backward, NULL, NULL);
}

static BROTLI_INLINE void FN(StoreRange)(HashToBinaryTree* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i = ix_start;
  size_t j = ix_start;
  if (ix_start + 63 <= ix_end) {
    i = ix_end - 63;
  }
  if (ix_start + 512 <= i) {
    for (; j < i; j += 8) {
      FN(Store)(self, data, mask, j);
    }
  }
  for (; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashToBinaryTree* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 &&
      position >= MAX_TREE_COMP_LENGTH) {

    const size_t i_start = position - MAX_TREE_COMP_LENGTH + 1;
    const size_t i_end = BROTLI_MIN(size_t, position, i_start + num_bytes);
    size_t i;
    for (i = i_start; i < i_end; ++i) {

      const size_t max_backward =
          self->window_mask_ - BROTLI_MAX(size_t,
                                          BROTLI_WINDOW_GAP - 1,
                                          position - i);

      FN(StoreAndFindMatches)(self, ringbuffer, i, ringbuffer_mask,
          MAX_TREE_COMP_LENGTH, max_backward, NULL, NULL);
    }
  }
}

#undef BUCKET_SIZE

#undef HashToBinaryTree

#undef MAX_TREE_SEARCH_DEPTH
#undef MAX_TREE_COMP_LENGTH
#undef BUCKET_BITS
#undef HASHER

#define MAX_NUM_MATCHES_H10 128

#define HASHER() H2
#define BUCKET_BITS 16
#define BUCKET_SWEEP_BITS 0
#define HASH_LEN 5
#define USE_DICTIONARY 1

#define HashLongestMatchQuickly HASHER()

#define BUCKET_SIZE (1 << BUCKET_BITS)
#define BUCKET_MASK (BUCKET_SIZE - 1)
#define BUCKET_SWEEP (1 << BUCKET_SWEEP_BITS)
#define BUCKET_SWEEP_MASK ((BUCKET_SWEEP - 1) << 3)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

static uint32_t FN(HashBytes)(const uint8_t* data) {
  const uint64_t h = ((BROTLI_UNALIGNED_LOAD64LE(data) << (64 - 8 * HASH_LEN)) *
                      kHashMul64);

  return (uint32_t)(h >> (64 - BUCKET_BITS));
}

typedef struct HashLongestMatchQuickly {

  HasherCommon* common;

  uint32_t* buckets_;
} HashLongestMatchQuickly;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;

  BROTLI_UNUSED(params);
  self->buckets_ = (uint32_t*)common->extra[0];
}

static void FN(Prepare)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;

  size_t partial_prepare_threshold = BUCKET_SIZE >> 5;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i]);
      if (BUCKET_SWEEP == 1) {
        buckets[key] = 0;
      } else {
        uint32_t j;
        for (j = 0; j < BUCKET_SWEEP; ++j) {
          buckets[(key + (j << 3)) & BUCKET_MASK] = 0;
        }
      }
    }
  } else {

    memset(buckets, 0, sizeof(uint32_t) * BUCKET_SIZE);
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  const uint32_t key = FN(HashBytes)(&data[ix & mask]);
  if (BUCKET_SWEEP == 1) {
    self->buckets_[key] = (uint32_t)ix;
  } else {

    const uint32_t off = ix & BUCKET_SWEEP_MASK;
    self->buckets_[(key + off) & BUCKET_MASK] = (uint32_t)ix;
  }
}

static BROTLI_INLINE void FN(StoreRange)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position,
    const uint8_t* ringbuffer, size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t best_len_in = out->len;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  int compare_char = data[cur_ix_masked + best_len_in];
  size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  size_t key_out;
  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = best_len_in;
  size_t cached_backward = (size_t)distance_cache[0];
  size_t prev_ix = cur_ix - cached_backward;

  out->len_code_delta = 0;
  if (prev_ix < cur_ix && cached_backward <= max_backward) {
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char == data[prev_ix + best_len]) {
      const size_t len = FindMatchLengthWithLimit(
          &data[prev_ix], &data[cur_ix_masked], max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          out->len = len;
          out->distance = cached_backward;
          out->score = score;
          if (BUCKET_SWEEP == 1) {
            buckets[key] = (uint32_t)cur_ix;
            return;
          } else {
            best_len = len;
            best_score = score;
            compare_char = data[cur_ix_masked + len];
          }
        }
      }
    }
  }
  if (BUCKET_SWEEP == 1) {
    size_t backward;
    size_t len;

    prev_ix = buckets[key];
    buckets[key] = (uint32_t)cur_ix;
    backward = cur_ix - prev_ix;
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char != data[prev_ix + best_len_in]) {
      return;
    }
    if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
      return;
    }
    len = FindMatchLengthWithLimit(&data[prev_ix],
                                   &data[cur_ix_masked],
                                   max_length);
    if (len >= 4) {
      const score_t score = BackwardReferenceScore(len, backward);
      if (best_score < score) {
        out->len = len;
        out->distance = backward;
        out->score = score;
        return;
      }
    }
  } else {
    size_t keys[BUCKET_SWEEP];
    size_t i;
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      keys[i] = (key + (i << 3)) & BUCKET_MASK;
    }
    key_out = keys[(cur_ix & BUCKET_SWEEP_MASK) >> 3];
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      size_t len;
      size_t backward;
      prev_ix = buckets[keys[i]];
      backward = cur_ix - prev_ix;
      prev_ix &= (uint32_t)ring_buffer_mask;
      if (compare_char != data[prev_ix + best_len]) {
        continue;
      }
      if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
        continue;
      }
      len = FindMatchLengthWithLimit(&data[prev_ix],
                                     &data[cur_ix_masked],
                                     max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_len = len;
          out->len = len;
          compare_char = data[cur_ix_masked + len];
          best_score = score;
          out->score = score;
          out->distance = backward;
        }
      }
    }
  }
  if (USE_DICTIONARY && min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_TRUE);
  }
  if (BUCKET_SWEEP != 1) {
    buckets[key_out] = (uint32_t)cur_ix;
  }
}

#undef BUCKET_SWEEP_MASK
#undef BUCKET_SWEEP
#undef BUCKET_MASK
#undef BUCKET_SIZE

#undef HashLongestMatchQuickly

#undef BUCKET_SWEEP_BITS
#undef USE_DICTIONARY
#undef HASHER

#define HASHER() H3
#define BUCKET_SWEEP_BITS 1
#define USE_DICTIONARY 0

#define HashLongestMatchQuickly HASHER()

#define BUCKET_SIZE (1 << BUCKET_BITS)
#define BUCKET_MASK (BUCKET_SIZE - 1)
#define BUCKET_SWEEP (1 << BUCKET_SWEEP_BITS)
#define BUCKET_SWEEP_MASK ((BUCKET_SWEEP - 1) << 3)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

static uint32_t FN(HashBytes)(const uint8_t* data) {
  const uint64_t h = ((BROTLI_UNALIGNED_LOAD64LE(data) << (64 - 8 * HASH_LEN)) *
                      kHashMul64);

  return (uint32_t)(h >> (64 - BUCKET_BITS));
}

typedef struct HashLongestMatchQuickly {

  HasherCommon* common;

  uint32_t* buckets_;
} HashLongestMatchQuickly;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;

  BROTLI_UNUSED(params);
  self->buckets_ = (uint32_t*)common->extra[0];
}

static void FN(Prepare)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;

  size_t partial_prepare_threshold = BUCKET_SIZE >> 5;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i]);
      if (BUCKET_SWEEP == 1) {
        buckets[key] = 0;
      } else {
        uint32_t j;
        for (j = 0; j < BUCKET_SWEEP; ++j) {
          buckets[(key + (j << 3)) & BUCKET_MASK] = 0;
        }
      }
    }
  } else {

    memset(buckets, 0, sizeof(uint32_t) * BUCKET_SIZE);
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  const uint32_t key = FN(HashBytes)(&data[ix & mask]);
  if (BUCKET_SWEEP == 1) {
    self->buckets_[key] = (uint32_t)ix;
  } else {

    const uint32_t off = ix & BUCKET_SWEEP_MASK;
    self->buckets_[(key + off) & BUCKET_MASK] = (uint32_t)ix;
  }
}

static BROTLI_INLINE void FN(StoreRange)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position,
    const uint8_t* ringbuffer, size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t best_len_in = out->len;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  int compare_char = data[cur_ix_masked + best_len_in];
  size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  size_t key_out;
  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = best_len_in;
  size_t cached_backward = (size_t)distance_cache[0];
  size_t prev_ix = cur_ix - cached_backward;

  out->len_code_delta = 0;
  if (prev_ix < cur_ix && cached_backward <= max_backward) {
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char == data[prev_ix + best_len]) {
      const size_t len = FindMatchLengthWithLimit(
          &data[prev_ix], &data[cur_ix_masked], max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          out->len = len;
          out->distance = cached_backward;
          out->score = score;
          if (BUCKET_SWEEP == 1) {
            buckets[key] = (uint32_t)cur_ix;
            return;
          } else {
            best_len = len;
            best_score = score;
            compare_char = data[cur_ix_masked + len];
          }
        }
      }
    }
  }
  if (BUCKET_SWEEP == 1) {
    size_t backward;
    size_t len;

    prev_ix = buckets[key];
    buckets[key] = (uint32_t)cur_ix;
    backward = cur_ix - prev_ix;
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char != data[prev_ix + best_len_in]) {
      return;
    }
    if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
      return;
    }
    len = FindMatchLengthWithLimit(&data[prev_ix],
                                   &data[cur_ix_masked],
                                   max_length);
    if (len >= 4) {
      const score_t score = BackwardReferenceScore(len, backward);
      if (best_score < score) {
        out->len = len;
        out->distance = backward;
        out->score = score;
        return;
      }
    }
  } else {
    size_t keys[BUCKET_SWEEP];
    size_t i;
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      keys[i] = (key + (i << 3)) & BUCKET_MASK;
    }
    key_out = keys[(cur_ix & BUCKET_SWEEP_MASK) >> 3];
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      size_t len;
      size_t backward;
      prev_ix = buckets[keys[i]];
      backward = cur_ix - prev_ix;
      prev_ix &= (uint32_t)ring_buffer_mask;
      if (compare_char != data[prev_ix + best_len]) {
        continue;
      }
      if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
        continue;
      }
      len = FindMatchLengthWithLimit(&data[prev_ix],
                                     &data[cur_ix_masked],
                                     max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_len = len;
          out->len = len;
          compare_char = data[cur_ix_masked + len];
          best_score = score;
          out->score = score;
          out->distance = backward;
        }
      }
    }
  }
  if (USE_DICTIONARY && min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_TRUE);
  }
  if (BUCKET_SWEEP != 1) {
    buckets[key_out] = (uint32_t)cur_ix;
  }
}

#undef BUCKET_SWEEP_MASK
#undef BUCKET_SWEEP
#undef BUCKET_MASK
#undef BUCKET_SIZE

#undef HashLongestMatchQuickly

#undef USE_DICTIONARY
#undef BUCKET_SWEEP_BITS
#undef BUCKET_BITS
#undef HASHER

#define HASHER() H4
#define BUCKET_BITS 17
#define BUCKET_SWEEP_BITS 2
#define USE_DICTIONARY 1

#define HashLongestMatchQuickly HASHER()

#define BUCKET_SIZE (1 << BUCKET_BITS)
#define BUCKET_MASK (BUCKET_SIZE - 1)
#define BUCKET_SWEEP (1 << BUCKET_SWEEP_BITS)
#define BUCKET_SWEEP_MASK ((BUCKET_SWEEP - 1) << 3)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

static uint32_t FN(HashBytes)(const uint8_t* data) {
  const uint64_t h = ((BROTLI_UNALIGNED_LOAD64LE(data) << (64 - 8 * HASH_LEN)) *
                      kHashMul64);

  return (uint32_t)(h >> (64 - BUCKET_BITS));
}

typedef struct HashLongestMatchQuickly {

  HasherCommon* common;

  uint32_t* buckets_;
} HashLongestMatchQuickly;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;

  BROTLI_UNUSED(params);
  self->buckets_ = (uint32_t*)common->extra[0];
}

static void FN(Prepare)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;

  size_t partial_prepare_threshold = BUCKET_SIZE >> 5;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i]);
      if (BUCKET_SWEEP == 1) {
        buckets[key] = 0;
      } else {
        uint32_t j;
        for (j = 0; j < BUCKET_SWEEP; ++j) {
          buckets[(key + (j << 3)) & BUCKET_MASK] = 0;
        }
      }
    }
  } else {

    memset(buckets, 0, sizeof(uint32_t) * BUCKET_SIZE);
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  const uint32_t key = FN(HashBytes)(&data[ix & mask]);
  if (BUCKET_SWEEP == 1) {
    self->buckets_[key] = (uint32_t)ix;
  } else {

    const uint32_t off = ix & BUCKET_SWEEP_MASK;
    self->buckets_[(key + off) & BUCKET_MASK] = (uint32_t)ix;
  }
}

static BROTLI_INLINE void FN(StoreRange)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position,
    const uint8_t* ringbuffer, size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t best_len_in = out->len;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  int compare_char = data[cur_ix_masked + best_len_in];
  size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  size_t key_out;
  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = best_len_in;
  size_t cached_backward = (size_t)distance_cache[0];
  size_t prev_ix = cur_ix - cached_backward;

  out->len_code_delta = 0;
  if (prev_ix < cur_ix && cached_backward <= max_backward) {
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char == data[prev_ix + best_len]) {
      const size_t len = FindMatchLengthWithLimit(
          &data[prev_ix], &data[cur_ix_masked], max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          out->len = len;
          out->distance = cached_backward;
          out->score = score;
          if (BUCKET_SWEEP == 1) {
            buckets[key] = (uint32_t)cur_ix;
            return;
          } else {
            best_len = len;
            best_score = score;
            compare_char = data[cur_ix_masked + len];
          }
        }
      }
    }
  }
  if (BUCKET_SWEEP == 1) {
    size_t backward;
    size_t len;

    prev_ix = buckets[key];
    buckets[key] = (uint32_t)cur_ix;
    backward = cur_ix - prev_ix;
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char != data[prev_ix + best_len_in]) {
      return;
    }
    if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
      return;
    }
    len = FindMatchLengthWithLimit(&data[prev_ix],
                                   &data[cur_ix_masked],
                                   max_length);
    if (len >= 4) {
      const score_t score = BackwardReferenceScore(len, backward);
      if (best_score < score) {
        out->len = len;
        out->distance = backward;
        out->score = score;
        return;
      }
    }
  } else {
    size_t keys[BUCKET_SWEEP];
    size_t i;
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      keys[i] = (key + (i << 3)) & BUCKET_MASK;
    }
    key_out = keys[(cur_ix & BUCKET_SWEEP_MASK) >> 3];
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      size_t len;
      size_t backward;
      prev_ix = buckets[keys[i]];
      backward = cur_ix - prev_ix;
      prev_ix &= (uint32_t)ring_buffer_mask;
      if (compare_char != data[prev_ix + best_len]) {
        continue;
      }
      if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
        continue;
      }
      len = FindMatchLengthWithLimit(&data[prev_ix],
                                     &data[cur_ix_masked],
                                     max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_len = len;
          out->len = len;
          compare_char = data[cur_ix_masked + len];
          best_score = score;
          out->score = score;
          out->distance = backward;
        }
      }
    }
  }
  if (USE_DICTIONARY && min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_TRUE);
  }
  if (BUCKET_SWEEP != 1) {
    buckets[key_out] = (uint32_t)cur_ix;
  }
}

#undef BUCKET_SWEEP_MASK
#undef BUCKET_SWEEP
#undef BUCKET_MASK
#undef BUCKET_SIZE

#undef HashLongestMatchQuickly

#undef USE_DICTIONARY
#undef HASH_LEN
#undef BUCKET_SWEEP_BITS
#undef BUCKET_BITS
#undef HASHER

#define HASHER() H5

#define HashLongestMatch HASHER()

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static uint32_t FN(HashBytes)(
    const uint8_t* BROTLI_RESTRICT data, const int shift) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return (uint32_t)(h >> shift);
}

typedef struct HashLongestMatch {

  size_t bucket_size_;

  size_t block_size_;

  int hash_shift_;

  uint32_t block_mask_;

  int block_bits_;
  int num_last_distances_to_check_;

  HasherCommon* common_;

  uint16_t* num_;

  uint32_t* buckets_;
} HashLongestMatch;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common_ = common;

  BROTLI_UNUSED(params);
  self->hash_shift_ = 32 - common->params.bucket_bits;
  self->bucket_size_ = (size_t)1 << common->params.bucket_bits;
  self->block_size_ = (size_t)1 << common->params.block_bits;
  self->block_mask_ = (uint32_t)(self->block_size_ - 1);
  self->num_ = (uint16_t*)common->extra[0];
  self->buckets_ = (uint32_t*)common->extra[1];
  self->block_bits_ = common->params.block_bits;
  self->num_last_distances_to_check_ =
      common->params.num_last_distances_to_check;
}

static void FN(Prepare)(
    HashLongestMatch* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint16_t* BROTLI_RESTRICT num = self->num_;

  size_t partial_prepare_threshold = self->bucket_size_ >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i], self->hash_shift_);
      num[key] = 0;
    }
  } else {
    memset(num, 0, self->bucket_size_ * sizeof(num[0]));
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t bucket_size = (size_t)1 << params->hasher.bucket_bits;
  size_t block_size = (size_t)1 << params->hasher.block_bits;
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint16_t) * bucket_size;
  alloc_size[1] = sizeof(uint32_t) * bucket_size * block_size;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatch* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const uint32_t key = FN(HashBytes)(&data[ix & mask], self->hash_shift_);
  const size_t minor_ix = num[key] & self->block_mask_;
  const size_t offset = minor_ix + (key << self->block_bits_);
  ++num[key];
  buckets[offset] = (uint32_t)ix;
}

static BROTLI_INLINE void FN(StoreRange)(HashLongestMatch* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatch* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatch* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  PrepareDistanceCache(distance_cache, self->num_last_distances_to_check_);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;

  const uint32_t key =
      FN(HashBytes)(&data[cur_ix_masked], self->hash_shift_);
  uint32_t* BROTLI_RESTRICT bucket = &buckets[key << self->block_bits_];
  PREFETCH_L1(bucket);
  if (self->block_bits_ > 4) PREFETCH_L1(bucket + 16);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < (size_t)self->num_last_distances_to_check_; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (size_t)(cur_ix - backward);
    if (prev_ix >= cur_ix) {
      continue;
    }
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      continue;
    }
    prev_ix &= ring_buffer_mask;

    if (cur_ix_masked + best_len > ring_buffer_mask) {
      break;
    }
    if (prev_ix + best_len > ring_buffer_mask ||
        data[cur_ix_masked + best_len] != data[prev_ix + best_len]) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 3 || (len == 2 && i < 2)) {

        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const size_t down =
        (num[key] > self->block_size_) ? (num[key] - self->block_size_) : 0u;
    for (i = num[key]; i > down;) {
      size_t prev_ix = bucket[--i & self->block_mask_];
      const size_t backward = cur_ix - prev_ix;
      if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
        break;
      }
      prev_ix &= ring_buffer_mask;
      if (cur_ix_masked + best_len > ring_buffer_mask) {
        break;
      }
      if (prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {

          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    bucket[num[key] & self->block_mask_] = (uint32_t)cur_ix;
    ++num[key];
  }
  if (min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common_, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef HashLongestMatch

#undef HASHER

#define HASHER() H6

#define HashLongestMatch HASHER()

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

static BROTLI_INLINE size_t FN(HashBytes)(const uint8_t* BROTLI_RESTRICT data,
                                          uint64_t hash_mul) {
  const uint64_t h = BROTLI_UNALIGNED_LOAD64LE(data) * hash_mul;

  return (size_t)(h >> (64 - 15));
}

typedef struct HashLongestMatch {

  size_t bucket_size_;

  size_t block_size_;

  uint64_t hash_mul_;

  uint32_t block_mask_;

  int block_bits_;
  int num_last_distances_to_check_;

  HasherCommon* common_;

  uint16_t* num_;

  uint32_t* buckets_;
} HashLongestMatch;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common_ = common;

  BROTLI_UNUSED(params);
  self->hash_mul_ = kHashMul64 << (64 - 5 * 8);
  BROTLI_DCHECK(common->params.bucket_bits == 15);
  self->bucket_size_ = (size_t)1 << common->params.bucket_bits;
  self->block_bits_ = common->params.block_bits;
  self->block_size_ = (size_t)1 << common->params.block_bits;
  self->block_mask_ = (uint32_t)(self->block_size_ - 1);
  self->num_last_distances_to_check_ =
      common->params.num_last_distances_to_check;
  self->num_ = (uint16_t*)common->extra[0];
  self->buckets_ = (uint32_t*)common->extra[1];
}

static void FN(Prepare)(
    HashLongestMatch* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint16_t* BROTLI_RESTRICT num = self->num_;

  size_t partial_prepare_threshold = self->bucket_size_ >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const size_t key = FN(HashBytes)(&data[i], self->hash_mul_);
      num[key] = 0;
    }
  } else {
    memset(num, 0, self->bucket_size_ * sizeof(num[0]));
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t bucket_size = (size_t)1 << params->hasher.bucket_bits;
  size_t block_size = (size_t)1 << params->hasher.block_bits;
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint16_t) * bucket_size;
  alloc_size[1] = sizeof(uint32_t) * bucket_size * block_size;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatch* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t key = FN(HashBytes)(&data[ix & mask], self->hash_mul_);
  const size_t minor_ix = num[key] & self->block_mask_;
  const size_t offset = minor_ix + (key << self->block_bits_);
  ++num[key];
  buckets[offset] = (uint32_t)ix;
}

static BROTLI_INLINE void FN(StoreRange)(HashLongestMatch* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatch* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatch* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  PrepareDistanceCache(distance_cache, self->num_last_distances_to_check_);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;

  const size_t key = FN(HashBytes)(&data[cur_ix_masked], self->hash_mul_);
  uint32_t* BROTLI_RESTRICT bucket = &buckets[key << self->block_bits_];
  PREFETCH_L1(bucket);
  if (self->block_bits_ > 4) PREFETCH_L1(bucket + 16);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < (size_t)self->num_last_distances_to_check_; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (size_t)(cur_ix - backward);
    if (prev_ix >= cur_ix) {
      continue;
    }
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      continue;
    }
    prev_ix &= ring_buffer_mask;

    if (cur_ix_masked + best_len > ring_buffer_mask) {
      break;
    }
    if (prev_ix + best_len > ring_buffer_mask ||
        data[cur_ix_masked + best_len] != data[prev_ix + best_len]) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 3 || (len == 2 && i < 2)) {

        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const size_t down =
        (num[key] > self->block_size_) ?
        (num[key] - self->block_size_) : 0u;
    const uint32_t first4 = BrotliUnalignedRead32(data + cur_ix_masked);
    const size_t max_length_m4 = max_length - 4;
    i = num[key];
    for (; i > down;) {
      size_t prev_ix = bucket[--i & self->block_mask_];
      uint32_t current4;
      const size_t backward = cur_ix - prev_ix;
      if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
        break;
      }
      prev_ix &= ring_buffer_mask;
      if (cur_ix_masked + best_len > ring_buffer_mask) {
        break;
      }
      if (prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      current4 = BrotliUnalignedRead32(data + prev_ix);
      if (first4 != current4) continue;
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix + 4],
                                                    &data[cur_ix_masked + 4],
                                                    max_length_m4) + 4;
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_score = score;
          best_len = len;
          out->len = best_len;
          out->distance = backward;
          out->score = best_score;
        }
      }
    }
    bucket[num[key] & self->block_mask_] = (uint32_t)cur_ix;
    ++num[key];
  }
  if (min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common_, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef HashLongestMatch

#undef HASHER

#if defined(BROTLI_MAX_SIMD_QUALITY)
#define HASHER() H58

#define HashLongestMatch HASHER()
#define TAG_HASH_BITS 8
#define TAG_HASH_MASK ((1 << TAG_HASH_BITS) - 1)
static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static uint32_t FN(HashBytes)(
    const uint8_t* BROTLI_RESTRICT data, const int shift) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return (uint32_t)(h >> shift);
}
typedef struct HashLongestMatch {

  size_t bucket_size_;

  size_t block_size_;

  int hash_shift_;

  uint32_t block_mask_;
  int block_bits_;
  int num_last_distances_to_check_;

  HasherCommon* common_;

  uint16_t* num_;
  uint8_t* tags_;

  uint32_t* buckets_;
} HashLongestMatch;
static void FN(Initialize)(
    HasherCommon* common, HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common_ = common;
  BROTLI_UNUSED(params);
  self->hash_shift_ = 32 - common->params.bucket_bits - TAG_HASH_BITS;
  self->bucket_size_ = (size_t)1 << common->params.bucket_bits;
  self->block_size_ = (size_t)1 << common->params.block_bits;
  self->block_mask_ = (uint32_t)(self->block_size_ - 1);
  self->num_ = (uint16_t*)common->extra[0];
  self->tags_ = (uint8_t*)common->extra[1];
  self->buckets_ = (uint32_t*)common->extra[2];
  self->block_bits_ = common->params.block_bits;
  self->num_last_distances_to_check_ =
      common->params.num_last_distances_to_check;
}
static void FN(Prepare)(
    HashLongestMatch* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint16_t* BROTLI_RESTRICT num = self->num_;

  size_t partial_prepare_threshold = self->bucket_size_ >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t hash = FN(HashBytes)(&data[i], self->hash_shift_);
      const uint32_t key = hash >> TAG_HASH_BITS;
      num[key] = 65535;
    }
  } else {

    memset(num, 255, self->bucket_size_ * sizeof(num[0]));
  }
}
static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t bucket_size = (size_t)1 << params->hasher.bucket_bits;
  size_t block_size = (size_t)1 << params->hasher.block_bits;
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint16_t) * bucket_size;
  alloc_size[1] = sizeof(uint8_t) * bucket_size * block_size;
  alloc_size[2] = sizeof(uint32_t) * bucket_size * block_size;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatch* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint8_t* BROTLI_RESTRICT tags = self->tags_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t hash = FN(HashBytes)(&data[ix & mask], self->hash_shift_);
  const size_t key = hash >> TAG_HASH_BITS;
  const uint8_t tag = hash & TAG_HASH_MASK;
  const size_t minor_ix = num[key] & self->block_mask_;
  const size_t offset = minor_ix + (key << self->block_bits_);
  --num[key];
  buckets[offset] = (uint32_t)ix;
  tags[offset] = tag;
}
static BROTLI_INLINE void FN(StoreRange)(HashLongestMatch* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}
static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatch* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}
static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatch* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  PrepareDistanceCache(distance_cache, self->num_last_distances_to_check_);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  uint8_t* BROTLI_RESTRICT tags = self->tags_;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;

  const uint32_t hash =
      FN(HashBytes)(&data[cur_ix_masked], self->hash_shift_);
  const uint32_t key = hash >> TAG_HASH_BITS;
  uint32_t* BROTLI_RESTRICT bucket = &buckets[key << self->block_bits_];
  uint8_t* BROTLI_RESTRICT tag_bucket = &tags[key << self->block_bits_];
  PREFETCH_L1(bucket);
  PREFETCH_L1(tag_bucket);
  if (self->block_bits_ > 4) PREFETCH_L1(bucket + 16);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < (size_t)self->num_last_distances_to_check_; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (size_t)(cur_ix - backward);
    if (prev_ix >= cur_ix) {
      continue;
    }
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      continue;
    }
    prev_ix &= ring_buffer_mask;

    if (cur_ix_masked + best_len > ring_buffer_mask) {
      break;
    }
    if (prev_ix + best_len > ring_buffer_mask ||
        data[cur_ix_masked + best_len] != data[prev_ix + best_len]) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 3 || (len == 2 && i < 2)) {

        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const uint8_t tag = hash & TAG_HASH_MASK;
    const size_t head = (num[key] + 1) & self->block_mask_;
    uint64_t matches =
        GetMatchingTagMask(self->block_size_ / 16, tag, tag_bucket, head);

    uint16_t n = 65535 - num[key];
    uint64_t block_has_unused_slots = self->block_size_ > n;
    uint64_t mask = (block_has_unused_slots << (n & (64 - 1))) - 1;
    matches &= mask;
    for (; matches > 0; matches &= (matches - 1)) {
      const size_t rb_index =
          (head + (size_t)BROTLI_TZCNT64(matches)) & self->block_mask_;
      size_t prev_ix = bucket[rb_index];
      const size_t backward = cur_ix - prev_ix;
      if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
        break;
      }
      prev_ix &= ring_buffer_mask;
      if (cur_ix_masked + best_len > ring_buffer_mask) {
        break;
      }
      if (prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {

          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    bucket[num[key] & self->block_mask_] = (uint32_t)cur_ix;
    tag_bucket[num[key] & self->block_mask_] = tag;
    --num[key];
  }
  if (min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common_, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}
#undef HashLongestMatch

#undef HASHER

#define HASHER() H59

#define HashLongestMatch HASHER()
#define TAG_HASH_BITS 8
#define TAG_HASH_MASK ((1 << TAG_HASH_BITS) - 1)
static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static uint32_t FN(HashBytes)(
    const uint8_t* BROTLI_RESTRICT data, const int shift) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return (uint32_t)(h >> shift);
}
typedef struct HashLongestMatch {

  size_t bucket_size_;

  size_t block_size_;

  int hash_shift_;

  uint32_t block_mask_;
  int block_bits_;
  int num_last_distances_to_check_;

  HasherCommon* common_;

  uint16_t* num_;
  uint8_t* tags_;

  uint32_t* buckets_;
} HashLongestMatch;
static void FN(Initialize)(
    HasherCommon* common, HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common_ = common;
  BROTLI_UNUSED(params);
  self->hash_shift_ = 32 - common->params.bucket_bits - TAG_HASH_BITS;
  self->bucket_size_ = (size_t)1 << common->params.bucket_bits;
  self->block_size_ = (size_t)1 << common->params.block_bits;
  self->block_mask_ = (uint32_t)(self->block_size_ - 1);
  self->num_ = (uint16_t*)common->extra[0];
  self->tags_ = (uint8_t*)common->extra[1];
  self->buckets_ = (uint32_t*)common->extra[2];
  self->block_bits_ = common->params.block_bits;
  self->num_last_distances_to_check_ =
      common->params.num_last_distances_to_check;
}
static void FN(Prepare)(
    HashLongestMatch* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint16_t* BROTLI_RESTRICT num = self->num_;

  size_t partial_prepare_threshold = self->bucket_size_ >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t hash = FN(HashBytes)(&data[i], self->hash_shift_);
      const uint32_t key = hash >> TAG_HASH_BITS;
      num[key] = 65535;
    }
  } else {

    memset(num, 255, self->bucket_size_ * sizeof(num[0]));
  }
}
static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t bucket_size = (size_t)1 << params->hasher.bucket_bits;
  size_t block_size = (size_t)1 << params->hasher.block_bits;
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint16_t) * bucket_size;
  alloc_size[1] = sizeof(uint8_t) * bucket_size * block_size;
  alloc_size[2] = sizeof(uint32_t) * bucket_size * block_size;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatch* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint8_t* BROTLI_RESTRICT tags = self->tags_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t hash = FN(HashBytes)(&data[ix & mask], self->hash_shift_);
  const size_t key = hash >> TAG_HASH_BITS;
  const uint8_t tag = hash & TAG_HASH_MASK;
  const size_t minor_ix = num[key] & self->block_mask_;
  const size_t offset = minor_ix + (key << self->block_bits_);
  --num[key];
  buckets[offset] = (uint32_t)ix;
  tags[offset] = tag;
}
static BROTLI_NOINLINE void FN(StoreRange)(
    HashLongestMatch* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}
static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatch* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}
static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatch* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  PrepareDistanceCache(distance_cache, self->num_last_distances_to_check_);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  uint8_t* BROTLI_RESTRICT tags = self->tags_;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;

  size_t best_len =
      self->common_->dict_num_matches < (self->common_->dict_num_lookups >> 7)
          ? out->len
          : 0;
  size_t i;

  const uint32_t hash =
      FN(HashBytes)(&data[cur_ix_masked], self->hash_shift_);
  const uint32_t key = hash >> TAG_HASH_BITS;
  uint32_t* BROTLI_RESTRICT bucket = &buckets[key << self->block_bits_];
  uint8_t* BROTLI_RESTRICT tag_bucket = &tags[key << self->block_bits_];
  PREFETCH_L1(bucket);
  PREFETCH_L1(tag_bucket);
  if (self->block_bits_ > 4) PREFETCH_L1(bucket + 16);
  out->len = 0;
  out->len_code_delta = 0;

  if (best_len < 1) {
    best_len = 1;
  }

  for (i = 0; i < (size_t)self->num_last_distances_to_check_; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (size_t)(cur_ix - backward);
    if (prev_ix >= cur_ix) {
      continue;
    }
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      continue;
    }
    prev_ix &= ring_buffer_mask;

    if (BrotliUnalignedRead16(&data[cur_ix_masked + best_len - 1]) !=
            BrotliUnalignedRead16(&data[prev_ix + best_len - 1])) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 3 || (len == 2 && i < 2)) {

        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const uint8_t tag = hash & TAG_HASH_MASK;
    const size_t head = (num[key] + 1) & self->block_mask_;
    uint64_t matches =
        GetMatchingTagMask(self->block_size_ / 16, tag, tag_bucket, head);

    uint16_t n = 65535 - num[key];
    uint64_t block_has_unused_slots = self->block_size_ > n;
    uint64_t mask = (block_has_unused_slots << (n & (64 - 1))) - 1;
    matches &= mask;
    for (; matches > 0; matches &= (matches - 1)) {
      const size_t rb_index =
          (head + (size_t)BROTLI_TZCNT64(matches)) & self->block_mask_;
      size_t prev_ix = bucket[rb_index];
      const size_t backward = cur_ix - prev_ix;
      if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
        break;
      }
      prev_ix &= ring_buffer_mask;
      if (
          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {

          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    bucket[num[key] & self->block_mask_] = (uint32_t)cur_ix;
    tag_bucket[num[key] & self->block_mask_] = tag;
    --num[key];
  }
  if (min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common_, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

BROTLI_UNUSED_FUNCTION void FN(SuppressUnusedFunctions)(void) {
  BROTLI_UNUSED(&FN(SuppressUnusedFunctions));
  BROTLI_UNUSED(&FN(StoreRange));
}
#undef HashLongestMatch

#undef HASHER

#define HASHER() H68

#define HashLongestMatch HASHER()

#define TAG_HASH_BITS 8
#define TAG_HASH_MASK ((1 << TAG_HASH_BITS) - 1)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

static BROTLI_INLINE size_t FN(HashBytes)(const uint8_t* BROTLI_RESTRICT data,
                                          uint64_t hash_mul) {
  const uint64_t h = BROTLI_UNALIGNED_LOAD64LE(data) * hash_mul;

  return (size_t)(h >> (64 - 15 - TAG_HASH_BITS));
}

typedef struct HashLongestMatch {

  size_t bucket_size_;

  size_t block_size_;

  uint64_t hash_mul_;

  uint32_t block_mask_;

  int block_bits_;
  int num_last_distances_to_check_;

  HasherCommon* common_;

  uint16_t* num_;

  uint8_t* tags_;

  uint32_t* buckets_;
} HashLongestMatch;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common_ = common;

  BROTLI_UNUSED(params);
  self->hash_mul_ = kHashMul64 << (64 - 5 * 8);
  BROTLI_DCHECK(common->params.bucket_bits == 15);
  self->bucket_size_ = (size_t)1 << common->params.bucket_bits;
  self->block_bits_ = common->params.block_bits;
  self->block_size_ = (size_t)1 << common->params.block_bits;
  self->block_mask_ = (uint32_t)(self->block_size_ - 1);
  self->num_last_distances_to_check_ =
      common->params.num_last_distances_to_check;
  self->num_ = (uint16_t*)common->extra[0];
  self->tags_ = (uint8_t*)common->extra[1];
  self->buckets_ = (uint32_t*)common->extra[2];
}

static void FN(Prepare)(
    HashLongestMatch* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint16_t* BROTLI_RESTRICT num = self->num_;

  size_t partial_prepare_threshold = self->bucket_size_ >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const size_t hash = FN(HashBytes)(&data[i], self->hash_mul_);
      const size_t key = hash >> TAG_HASH_BITS;
      num[key] = 65535;
    }
  } else {

    memset(num, 255, self->bucket_size_ * sizeof(num[0]));
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t bucket_size = (size_t)1 << params->hasher.bucket_bits;
  size_t block_size = (size_t)1 << params->hasher.block_bits;
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint16_t) * bucket_size;
  alloc_size[1] = sizeof(uint8_t) * bucket_size * block_size;
  alloc_size[2] = sizeof(uint32_t) * bucket_size * block_size;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatch* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint8_t* BROTLI_RESTRICT tags = self->tags_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t hash = FN(HashBytes)(&data[ix & mask], self->hash_mul_);
  const size_t key = hash >> TAG_HASH_BITS;
  const uint8_t tag = hash & TAG_HASH_MASK;
  const size_t minor_ix = num[key] & self->block_mask_;
  const size_t offset = minor_ix + (key << self->block_bits_);
  --num[key];
  buckets[offset] = (uint32_t)ix;
  tags[offset] = tag;
}

static BROTLI_INLINE void FN(StoreRange)(HashLongestMatch* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatch* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatch* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  PrepareDistanceCache(distance_cache, self->num_last_distances_to_check_);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatch* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint16_t* BROTLI_RESTRICT num = self->num_;
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  uint8_t* BROTLI_RESTRICT tags = self->tags_;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;

  const size_t hash = FN(HashBytes)(&data[cur_ix_masked], self->hash_mul_);
  const size_t key = hash >> TAG_HASH_BITS;
  uint32_t* BROTLI_RESTRICT bucket = &buckets[key << self->block_bits_];
  uint8_t* BROTLI_RESTRICT tag_bucket = &tags[key << self->block_bits_];
  PREFETCH_L1(bucket);
  PREFETCH_L1(tag_bucket);
  if (self->block_bits_ > 4) PREFETCH_L1(bucket + 16);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < (size_t)self->num_last_distances_to_check_; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (size_t)(cur_ix - backward);
    if (prev_ix >= cur_ix) {
      continue;
    }
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      continue;
    }
    prev_ix &= ring_buffer_mask;

    if (cur_ix_masked + best_len > ring_buffer_mask) {
      break;
    }
    if (prev_ix + best_len > ring_buffer_mask ||
        data[cur_ix_masked + best_len] != data[prev_ix + best_len]) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 3 || (len == 2 && i < 2)) {

        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const uint8_t tag = hash & TAG_HASH_MASK;
    const uint32_t first4 = BrotliUnalignedRead32(data + cur_ix_masked);
    const size_t max_length_m4 = max_length - 4;
    const size_t head = (num[key] + 1) & self->block_mask_;
    uint64_t matches =
        GetMatchingTagMask(self->block_size_ / 16, tag, tag_bucket, head);

    uint16_t n = 65535 - num[key];
    uint64_t block_has_unused_slots = self->block_size_ > n;
    uint64_t mask = (block_has_unused_slots << (n & (64 - 1))) - 1;
    matches &= mask;
    for (; matches > 0; matches &= (matches - 1)) {
      const size_t rb_index =
          (head + (size_t)BROTLI_TZCNT64(matches)) & self->block_mask_;
      size_t prev_ix = bucket[rb_index];
      uint32_t current4;
      const size_t backward = cur_ix - prev_ix;
      if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
        break;
      }
      prev_ix &= ring_buffer_mask;
      if (cur_ix_masked + best_len > ring_buffer_mask) {
        break;
      }
      if (prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      current4 = BrotliUnalignedRead32(data + prev_ix);
      if (first4 != current4) continue;
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix + 4],
                                                    &data[cur_ix_masked + 4],
                                                    max_length_m4) + 4;
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_score = score;
          best_len = len;
          out->len = best_len;
          out->distance = backward;
          out->score = best_score;
        }
      }
    }
    bucket[num[key] & self->block_mask_] = (uint32_t)cur_ix;
    tag_bucket[num[key] & self->block_mask_] = tag;
    --num[key];
  }
  if (min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common_, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef HashLongestMatch

#undef HASHER
#endif

#define BUCKET_BITS 15

#define NUM_LAST_DISTANCES_TO_CHECK 4
#define NUM_BANKS 1
#define BANK_BITS 16
#define HASHER() H40

#define HashForgetfulChain HASHER()

#define BANK_SIZE (1 << BANK_BITS)

#define BUCKET_SIZE (1 << BUCKET_BITS)

#define CAPPED_CHAINS 0

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static BROTLI_INLINE size_t FN(HashBytes)(const uint8_t* BROTLI_RESTRICT data) {
  const uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return h >> (32 - BUCKET_BITS);
}

typedef struct FN(Slot) {
  uint16_t delta;
  uint16_t next;
} FN(Slot);

typedef struct FN(Bank) {
  FN(Slot) slots[BANK_SIZE];
} FN(Bank);

typedef struct HashForgetfulChain {
  uint16_t free_slot_idx[NUM_BANKS];
  size_t max_hops;

  void* extra[2];
  HasherCommon* common;

} HashForgetfulChain;

static uint32_t* FN(Addr)(void* extra) {
  return (uint32_t*)extra;
}

static uint16_t* FN(Head)(void* extra) {
  return (uint16_t*)(&FN(Addr)(extra)[BUCKET_SIZE]);
}

static uint8_t* FN(TinyHash)(void* extra) {
  return (uint8_t*)(&FN(Head)(extra)[BUCKET_SIZE]);
}

static FN(Bank)* FN(Banks)(void* extra) {
  return (FN(Bank)*)(extra);
}

static void FN(Initialize)(
    HasherCommon* common, HashForgetfulChain* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;
  self->extra[0] = common->extra[0];
  self->extra[1] = common->extra[1];

  self->max_hops = (params->quality > 6 ? 7u : 8u) << (params->quality - 4);
}

static void FN(Prepare)(
    HashForgetfulChain* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hash = FN(TinyHash)(self->extra[0]);

  size_t partial_prepare_threshold = BUCKET_SIZE >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      size_t bucket = FN(HashBytes)(&data[i]);

      addr[bucket] = 0xCCCCCCCC;
      head[bucket] = 0xCCCC;
    }
  } else {

    memset(addr, 0xCC, sizeof(uint32_t) * BUCKET_SIZE);
    memset(head, 0, sizeof(uint16_t) * BUCKET_SIZE);
  }
  memset(tiny_hash, 0, sizeof(uint8_t) * 65536);
  memset(self->free_slot_idx, 0, sizeof(self->free_slot_idx));
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE +
                  sizeof(uint16_t) * BUCKET_SIZE + sizeof(uint8_t) * 65536;
  alloc_size[1] = sizeof(FN(Bank)) * NUM_BANKS;
}

static BROTLI_INLINE void FN(Store)(HashForgetfulChain* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hash = FN(TinyHash)(self->extra[0]);
  FN(Bank)* BROTLI_RESTRICT banks = FN(Banks)(self->extra[1]);
  const size_t key = FN(HashBytes)(&data[ix & mask]);
  const size_t bank = key & (NUM_BANKS - 1);
  const size_t idx = self->free_slot_idx[bank]++ & (BANK_SIZE - 1);
  size_t delta = ix - addr[key];
  tiny_hash[(uint16_t)ix] = (uint8_t)key;
  if (delta > 0xFFFF) delta = CAPPED_CHAINS ? 0 : 0xFFFF;
  banks[bank].slots[idx].delta = (uint16_t)delta;
  banks[bank].slots[idx].next = head[key];
  addr[key] = (uint32_t)ix;
  head[key] = (uint16_t)idx;
}

static BROTLI_INLINE void FN(StoreRange)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  PrepareDistanceCache(distance_cache, NUM_LAST_DISTANCES_TO_CHECK);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hashes = FN(TinyHash)(self->extra[0]);
  FN(Bank)* BROTLI_RESTRICT banks = FN(Banks)(self->extra[1]);
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;
  const size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  const uint8_t tiny_hash = (uint8_t)(key);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < NUM_LAST_DISTANCES_TO_CHECK; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (cur_ix - backward);

    if (i > 0 && tiny_hashes[(uint16_t)prev_ix] != tiny_hash) continue;
    if (prev_ix >= cur_ix || backward > max_backward) {
      continue;
    }
    prev_ix &= ring_buffer_mask;
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 2) {
        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const size_t bank = key & (NUM_BANKS - 1);
    size_t backward = 0;
    size_t hops = self->max_hops;
    size_t delta = cur_ix - addr[key];
    size_t slot = head[key];
    while (hops--) {
      size_t prev_ix;
      size_t last = slot;
      backward += delta;
      if (backward > max_backward || (CAPPED_CHAINS && !delta)) break;
      prev_ix = (cur_ix - backward) & ring_buffer_mask;
      slot = banks[bank].slots[last].next;
      delta = banks[bank].slots[last].delta;
      if (cur_ix_masked + best_len > ring_buffer_mask ||
          prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {

          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    FN(Store)(self, data, ring_buffer_mask, cur_ix);
  }
  if (out->score == min_score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef BANK_SIZE
#undef BUCKET_SIZE
#undef CAPPED_CHAINS

#undef HashForgetfulChain

#undef HASHER
#undef NUM_LAST_DISTANCES_TO_CHECK

#define NUM_LAST_DISTANCES_TO_CHECK 10
#define HASHER() H41

#define HashForgetfulChain HASHER()

#define BANK_SIZE (1 << BANK_BITS)

#define BUCKET_SIZE (1 << BUCKET_BITS)

#define CAPPED_CHAINS 0

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static BROTLI_INLINE size_t FN(HashBytes)(const uint8_t* BROTLI_RESTRICT data) {
  const uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return h >> (32 - BUCKET_BITS);
}

typedef struct FN(Slot) {
  uint16_t delta;
  uint16_t next;
} FN(Slot);

typedef struct FN(Bank) {
  FN(Slot) slots[BANK_SIZE];
} FN(Bank);

typedef struct HashForgetfulChain {
  uint16_t free_slot_idx[NUM_BANKS];
  size_t max_hops;

  void* extra[2];
  HasherCommon* common;

} HashForgetfulChain;

static uint32_t* FN(Addr)(void* extra) {
  return (uint32_t*)extra;
}

static uint16_t* FN(Head)(void* extra) {
  return (uint16_t*)(&FN(Addr)(extra)[BUCKET_SIZE]);
}

static uint8_t* FN(TinyHash)(void* extra) {
  return (uint8_t*)(&FN(Head)(extra)[BUCKET_SIZE]);
}

static FN(Bank)* FN(Banks)(void* extra) {
  return (FN(Bank)*)(extra);
}

static void FN(Initialize)(
    HasherCommon* common, HashForgetfulChain* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;
  self->extra[0] = common->extra[0];
  self->extra[1] = common->extra[1];

  self->max_hops = (params->quality > 6 ? 7u : 8u) << (params->quality - 4);
}

static void FN(Prepare)(
    HashForgetfulChain* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hash = FN(TinyHash)(self->extra[0]);

  size_t partial_prepare_threshold = BUCKET_SIZE >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      size_t bucket = FN(HashBytes)(&data[i]);

      addr[bucket] = 0xCCCCCCCC;
      head[bucket] = 0xCCCC;
    }
  } else {

    memset(addr, 0xCC, sizeof(uint32_t) * BUCKET_SIZE);
    memset(head, 0, sizeof(uint16_t) * BUCKET_SIZE);
  }
  memset(tiny_hash, 0, sizeof(uint8_t) * 65536);
  memset(self->free_slot_idx, 0, sizeof(self->free_slot_idx));
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE +
                  sizeof(uint16_t) * BUCKET_SIZE + sizeof(uint8_t) * 65536;
  alloc_size[1] = sizeof(FN(Bank)) * NUM_BANKS;
}

static BROTLI_INLINE void FN(Store)(HashForgetfulChain* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hash = FN(TinyHash)(self->extra[0]);
  FN(Bank)* BROTLI_RESTRICT banks = FN(Banks)(self->extra[1]);
  const size_t key = FN(HashBytes)(&data[ix & mask]);
  const size_t bank = key & (NUM_BANKS - 1);
  const size_t idx = self->free_slot_idx[bank]++ & (BANK_SIZE - 1);
  size_t delta = ix - addr[key];
  tiny_hash[(uint16_t)ix] = (uint8_t)key;
  if (delta > 0xFFFF) delta = CAPPED_CHAINS ? 0 : 0xFFFF;
  banks[bank].slots[idx].delta = (uint16_t)delta;
  banks[bank].slots[idx].next = head[key];
  addr[key] = (uint32_t)ix;
  head[key] = (uint16_t)idx;
}

static BROTLI_INLINE void FN(StoreRange)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  PrepareDistanceCache(distance_cache, NUM_LAST_DISTANCES_TO_CHECK);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hashes = FN(TinyHash)(self->extra[0]);
  FN(Bank)* BROTLI_RESTRICT banks = FN(Banks)(self->extra[1]);
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;
  const size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  const uint8_t tiny_hash = (uint8_t)(key);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < NUM_LAST_DISTANCES_TO_CHECK; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (cur_ix - backward);

    if (i > 0 && tiny_hashes[(uint16_t)prev_ix] != tiny_hash) continue;
    if (prev_ix >= cur_ix || backward > max_backward) {
      continue;
    }
    prev_ix &= ring_buffer_mask;
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 2) {
        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const size_t bank = key & (NUM_BANKS - 1);
    size_t backward = 0;
    size_t hops = self->max_hops;
    size_t delta = cur_ix - addr[key];
    size_t slot = head[key];
    while (hops--) {
      size_t prev_ix;
      size_t last = slot;
      backward += delta;
      if (backward > max_backward || (CAPPED_CHAINS && !delta)) break;
      prev_ix = (cur_ix - backward) & ring_buffer_mask;
      slot = banks[bank].slots[last].next;
      delta = banks[bank].slots[last].delta;
      if (cur_ix_masked + best_len > ring_buffer_mask ||
          prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {

          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    FN(Store)(self, data, ring_buffer_mask, cur_ix);
  }
  if (out->score == min_score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef BANK_SIZE
#undef BUCKET_SIZE
#undef CAPPED_CHAINS

#undef HashForgetfulChain

#undef HASHER
#undef NUM_LAST_DISTANCES_TO_CHECK
#undef NUM_BANKS
#undef BANK_BITS

#define NUM_LAST_DISTANCES_TO_CHECK 16
#define NUM_BANKS 512
#define BANK_BITS 9
#define HASHER() H42

#define HashForgetfulChain HASHER()

#define BANK_SIZE (1 << BANK_BITS)

#define BUCKET_SIZE (1 << BUCKET_BITS)

#define CAPPED_CHAINS 0

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static BROTLI_INLINE size_t FN(HashBytes)(const uint8_t* BROTLI_RESTRICT data) {
  const uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;

  return h >> (32 - BUCKET_BITS);
}

typedef struct FN(Slot) {
  uint16_t delta;
  uint16_t next;
} FN(Slot);

typedef struct FN(Bank) {
  FN(Slot) slots[BANK_SIZE];
} FN(Bank);

typedef struct HashForgetfulChain {
  uint16_t free_slot_idx[NUM_BANKS];
  size_t max_hops;

  void* extra[2];
  HasherCommon* common;

} HashForgetfulChain;

static uint32_t* FN(Addr)(void* extra) {
  return (uint32_t*)extra;
}

static uint16_t* FN(Head)(void* extra) {
  return (uint16_t*)(&FN(Addr)(extra)[BUCKET_SIZE]);
}

static uint8_t* FN(TinyHash)(void* extra) {
  return (uint8_t*)(&FN(Head)(extra)[BUCKET_SIZE]);
}

static FN(Bank)* FN(Banks)(void* extra) {
  return (FN(Bank)*)(extra);
}

static void FN(Initialize)(
    HasherCommon* common, HashForgetfulChain* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;
  self->extra[0] = common->extra[0];
  self->extra[1] = common->extra[1];

  self->max_hops = (params->quality > 6 ? 7u : 8u) << (params->quality - 4);
}

static void FN(Prepare)(
    HashForgetfulChain* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hash = FN(TinyHash)(self->extra[0]);

  size_t partial_prepare_threshold = BUCKET_SIZE >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      size_t bucket = FN(HashBytes)(&data[i]);

      addr[bucket] = 0xCCCCCCCC;
      head[bucket] = 0xCCCC;
    }
  } else {

    memset(addr, 0xCC, sizeof(uint32_t) * BUCKET_SIZE);
    memset(head, 0, sizeof(uint16_t) * BUCKET_SIZE);
  }
  memset(tiny_hash, 0, sizeof(uint8_t) * 65536);
  memset(self->free_slot_idx, 0, sizeof(self->free_slot_idx));
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE +
                  sizeof(uint16_t) * BUCKET_SIZE + sizeof(uint8_t) * 65536;
  alloc_size[1] = sizeof(FN(Bank)) * NUM_BANKS;
}

static BROTLI_INLINE void FN(Store)(HashForgetfulChain* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hash = FN(TinyHash)(self->extra[0]);
  FN(Bank)* BROTLI_RESTRICT banks = FN(Banks)(self->extra[1]);
  const size_t key = FN(HashBytes)(&data[ix & mask]);
  const size_t bank = key & (NUM_BANKS - 1);
  const size_t idx = self->free_slot_idx[bank]++ & (BANK_SIZE - 1);
  size_t delta = ix - addr[key];
  tiny_hash[(uint16_t)ix] = (uint8_t)key;
  if (delta > 0xFFFF) delta = CAPPED_CHAINS ? 0 : 0xFFFF;
  banks[bank].slots[idx].delta = (uint16_t)delta;
  banks[bank].slots[idx].next = head[key];
  addr[key] = (uint32_t)ix;
  head[key] = (uint16_t)idx;
}

static BROTLI_INLINE void FN(StoreRange)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ring_buffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  PrepareDistanceCache(distance_cache, NUM_LAST_DISTANCES_TO_CHECK);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashForgetfulChain* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT addr = FN(Addr)(self->extra[0]);
  uint16_t* BROTLI_RESTRICT head = FN(Head)(self->extra[0]);
  uint8_t* BROTLI_RESTRICT tiny_hashes = FN(TinyHash)(self->extra[0]);
  FN(Bank)* BROTLI_RESTRICT banks = FN(Banks)(self->extra[1]);
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;
  const size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  const uint8_t tiny_hash = (uint8_t)(key);
  out->len = 0;
  out->len_code_delta = 0;

  for (i = 0; i < NUM_LAST_DISTANCES_TO_CHECK; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (cur_ix - backward);

    if (i > 0 && tiny_hashes[(uint16_t)prev_ix] != tiny_hash) continue;
    if (prev_ix >= cur_ix || backward > max_backward) {
      continue;
    }
    prev_ix &= ring_buffer_mask;
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 2) {
        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  {
    const size_t bank = key & (NUM_BANKS - 1);
    size_t backward = 0;
    size_t hops = self->max_hops;
    size_t delta = cur_ix - addr[key];
    size_t slot = head[key];
    while (hops--) {
      size_t prev_ix;
      size_t last = slot;
      backward += delta;
      if (backward > max_backward || (CAPPED_CHAINS && !delta)) break;
      prev_ix = (cur_ix - backward) & ring_buffer_mask;
      slot = banks[bank].slots[last].next;
      delta = banks[bank].slots[last].delta;
      if (cur_ix_masked + best_len > ring_buffer_mask ||
          prev_ix + best_len > ring_buffer_mask ||

          BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
              BrotliUnalignedRead32(&data[prev_ix + best_len - 3])) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {

          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    FN(Store)(self, data, ring_buffer_mask, cur_ix);
  }
  if (out->score == min_score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef BANK_SIZE
#undef BUCKET_SIZE
#undef CAPPED_CHAINS

#undef HashForgetfulChain

#undef HASHER
#undef NUM_LAST_DISTANCES_TO_CHECK
#undef NUM_BANKS
#undef BANK_BITS

#undef BUCKET_BITS

#define HASHER() H54
#define BUCKET_BITS 20
#define BUCKET_SWEEP_BITS 2
#define HASH_LEN 7
#define USE_DICTIONARY 0

#define HashLongestMatchQuickly HASHER()

#define BUCKET_SIZE (1 << BUCKET_BITS)
#define BUCKET_MASK (BUCKET_SIZE - 1)
#define BUCKET_SWEEP (1 << BUCKET_SWEEP_BITS)
#define BUCKET_SWEEP_MASK ((BUCKET_SWEEP - 1) << 3)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

static uint32_t FN(HashBytes)(const uint8_t* data) {
  const uint64_t h = ((BROTLI_UNALIGNED_LOAD64LE(data) << (64 - 8 * HASH_LEN)) *
                      kHashMul64);

  return (uint32_t)(h >> (64 - BUCKET_BITS));
}

typedef struct HashLongestMatchQuickly {

  HasherCommon* common;

  uint32_t* buckets_;
} HashLongestMatchQuickly;

static void FN(Initialize)(
    HasherCommon* common, HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  self->common = common;

  BROTLI_UNUSED(params);
  self->buckets_ = (uint32_t*)common->extra[0];
}

static void FN(Prepare)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;

  size_t partial_prepare_threshold = BUCKET_SIZE >> 5;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i]);
      if (BUCKET_SWEEP == 1) {
        buckets[key] = 0;
      } else {
        uint32_t j;
        for (j = 0; j < BUCKET_SWEEP; ++j) {
          buckets[(key + (j << 3)) & BUCKET_MASK] = 0;
        }
      }
    }
  } else {

    memset(buckets, 0, sizeof(uint32_t) * BUCKET_SIZE);
  }
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = sizeof(uint32_t) * BUCKET_SIZE;
}

static BROTLI_INLINE void FN(Store)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  const uint32_t key = FN(HashBytes)(&data[ix & mask]);
  if (BUCKET_SWEEP == 1) {
    self->buckets_[key] = (uint32_t)ix;
  } else {

    const uint32_t off = ix & BUCKET_SWEEP_MASK;
    self->buckets_[(key + off) & BUCKET_MASK] = (uint32_t)ix;
  }
}

static BROTLI_INLINE void FN(StoreRange)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(self, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position,
    const uint8_t* ringbuffer, size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {

    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(self, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashLongestMatchQuickly* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  uint32_t* BROTLI_RESTRICT buckets = self->buckets_;
  const size_t best_len_in = out->len;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;

  int compare_char = data[cur_ix_masked + best_len_in];
  size_t key = FN(HashBytes)(&data[cur_ix_masked]);
  size_t key_out;
  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = best_len_in;
  size_t cached_backward = (size_t)distance_cache[0];
  size_t prev_ix = cur_ix - cached_backward;

  out->len_code_delta = 0;
  if (prev_ix < cur_ix && cached_backward <= max_backward) {
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char == data[prev_ix + best_len]) {
      const size_t len = FindMatchLengthWithLimit(
          &data[prev_ix], &data[cur_ix_masked], max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          out->len = len;
          out->distance = cached_backward;
          out->score = score;
          if (BUCKET_SWEEP == 1) {
            buckets[key] = (uint32_t)cur_ix;
            return;
          } else {
            best_len = len;
            best_score = score;
            compare_char = data[cur_ix_masked + len];
          }
        }
      }
    }
  }
  if (BUCKET_SWEEP == 1) {
    size_t backward;
    size_t len;

    prev_ix = buckets[key];
    buckets[key] = (uint32_t)cur_ix;
    backward = cur_ix - prev_ix;
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char != data[prev_ix + best_len_in]) {
      return;
    }
    if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
      return;
    }
    len = FindMatchLengthWithLimit(&data[prev_ix],
                                   &data[cur_ix_masked],
                                   max_length);
    if (len >= 4) {
      const score_t score = BackwardReferenceScore(len, backward);
      if (best_score < score) {
        out->len = len;
        out->distance = backward;
        out->score = score;
        return;
      }
    }
  } else {
    size_t keys[BUCKET_SWEEP];
    size_t i;
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      keys[i] = (key + (i << 3)) & BUCKET_MASK;
    }
    key_out = keys[(cur_ix & BUCKET_SWEEP_MASK) >> 3];
    for (i = 0; i < BUCKET_SWEEP; ++i) {
      size_t len;
      size_t backward;
      prev_ix = buckets[keys[i]];
      backward = cur_ix - prev_ix;
      prev_ix &= (uint32_t)ring_buffer_mask;
      if (compare_char != data[prev_ix + best_len]) {
        continue;
      }
      if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
        continue;
      }
      len = FindMatchLengthWithLimit(&data[prev_ix],
                                     &data[cur_ix_masked],
                                     max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_len = len;
          out->len = len;
          compare_char = data[cur_ix_masked + len];
          best_score = score;
          out->score = score;
          out->distance = backward;
        }
      }
    }
  }
  if (USE_DICTIONARY && min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        self->common, &data[cur_ix_masked], max_length, dictionary_distance,
        max_distance, out, BROTLI_TRUE);
  }
  if (BUCKET_SWEEP != 1) {
    buckets[key_out] = (uint32_t)cur_ix;
  }
}

#undef BUCKET_SWEEP_MASK
#undef BUCKET_SWEEP
#undef BUCKET_MASK
#undef BUCKET_SIZE

#undef HashLongestMatchQuickly

#undef USE_DICTIONARY
#undef HASH_LEN
#undef BUCKET_SWEEP_BITS
#undef BUCKET_BITS
#undef HASHER

#define HASHER() HROLLING_FAST
#define CHUNKLEN 32
#define JUMP 4
#define NUMBUCKETS 16777216
#define MASK ((NUMBUCKETS * 64) - 1)

#define HashRolling HASHER()

static const uint32_t FN(kRollingHashMul32) = 69069;
static const uint32_t FN(kInvalidPos) = 0xffffffff;

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static uint32_t FN(HashByte)(uint8_t byte) {
  return (uint32_t)byte + 1u;
}

static uint32_t FN(HashRollingFunctionInitial)(uint32_t state, uint8_t add,
                                               uint32_t factor) {
  return (uint32_t)(factor * state + FN(HashByte)(add));
}

static uint32_t FN(HashRollingFunction)(uint32_t state, uint8_t add,
                                        uint8_t rem, uint32_t factor,
                                        uint32_t factor_remove) {
  return (uint32_t)(factor * state +
      FN(HashByte)(add) - factor_remove * FN(HashByte)(rem));
}

typedef struct HashRolling {
  uint32_t state;
  uint32_t* table;
  size_t next_ix;

  uint32_t chunk_len;
  uint32_t factor;
  uint32_t factor_remove;
} HashRolling;

static void FN(Initialize)(
    HasherCommon* common, HashRolling* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  size_t i;
  self->state = 0;
  self->next_ix = 0;

  self->factor = FN(kRollingHashMul32);

  self->factor_remove = 1;
  for (i = 0; i < CHUNKLEN; i += JUMP) {
    self->factor_remove *= self->factor;
  }

  self->table = (uint32_t*)common->extra[0];
  for (i = 0; i < NUMBUCKETS; i++) {
    self->table[i] = FN(kInvalidPos);
  }

  BROTLI_UNUSED(params);
}

static void FN(Prepare)(HashRolling* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  size_t i;

  if (input_size < CHUNKLEN) return;
  self->state = 0;
  for (i = 0; i < CHUNKLEN; i += JUMP) {
    self->state = FN(HashRollingFunctionInitial)(
        self->state, data[i], self->factor);
  }
  BROTLI_UNUSED(one_shot);
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = NUMBUCKETS * sizeof(uint32_t);
}

static BROTLI_INLINE void FN(Store)(HashRolling* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(data);
  BROTLI_UNUSED(mask);
  BROTLI_UNUSED(ix);
}

static BROTLI_INLINE void FN(StoreRange)(HashRolling* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(data);
  BROTLI_UNUSED(mask);
  BROTLI_UNUSED(ix_start);
  BROTLI_UNUSED(ix_end);
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashRolling* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {

  size_t position_masked;
  size_t available = num_bytes;
  if ((position & (JUMP - 1)) != 0) {
    size_t diff = JUMP - (position & (JUMP - 1));
    available = (diff > available) ? 0 : (available - diff);
    position += diff;
  }
  position_masked = position & ring_buffer_mask;

  if (available > ring_buffer_mask - position_masked) {
    available = ring_buffer_mask - position_masked;
  }

  FN(Prepare)(self, BROTLI_FALSE, available,
      ringbuffer + (position & ring_buffer_mask));
  self->next_ix = position;
  BROTLI_UNUSED(num_bytes);
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashRolling* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashRolling* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  size_t pos;

  if ((cur_ix & (JUMP - 1)) != 0) return;

  if (max_length < CHUNKLEN) return;

  for (pos = self->next_ix; pos <= cur_ix; pos += JUMP) {
    uint32_t code = self->state & MASK;

    uint8_t rem = data[pos & ring_buffer_mask];
    uint8_t add = data[(pos + CHUNKLEN) & ring_buffer_mask];
    size_t found_ix = FN(kInvalidPos);

    self->state = FN(HashRollingFunction)(
        self->state, add, rem, self->factor, self->factor_remove);

    if (code < NUMBUCKETS) {
      found_ix = self->table[code];
      self->table[code] = (uint32_t)pos;
      if (pos == cur_ix && found_ix != FN(kInvalidPos)) {

        size_t backward = (uint32_t)(cur_ix - found_ix);
        if (backward <= max_backward) {
          const size_t found_ix_masked = found_ix & ring_buffer_mask;
          const size_t len = FindMatchLengthWithLimit(&data[found_ix_masked],
                                                      &data[cur_ix_masked],
                                                      max_length);
          if (len >= 4 && len > out->len) {
            score_t score = BackwardReferenceScore(len, backward);
            if (score > out->score) {
              out->len = len;
              out->distance = backward;
              out->score = score;
              out->len_code_delta = 0;
            }
          }
        }
      }
    }
  }

  self->next_ix = cur_ix + JUMP;

  BROTLI_UNUSED(dictionary);
  BROTLI_UNUSED(distance_cache);
  BROTLI_UNUSED(dictionary_distance);
  BROTLI_UNUSED(max_distance);
}

#undef HashRolling

#undef JUMP
#undef HASHER

#define HASHER() HROLLING
#define JUMP 1

#define HashRolling HASHER()

static const uint32_t FN(kRollingHashMul32) = 69069;
static const uint32_t FN(kInvalidPos) = 0xffffffff;

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 4; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 4; }

static uint32_t FN(HashByte)(uint8_t byte) {
  return (uint32_t)byte + 1u;
}

static uint32_t FN(HashRollingFunctionInitial)(uint32_t state, uint8_t add,
                                               uint32_t factor) {
  return (uint32_t)(factor * state + FN(HashByte)(add));
}

static uint32_t FN(HashRollingFunction)(uint32_t state, uint8_t add,
                                        uint8_t rem, uint32_t factor,
                                        uint32_t factor_remove) {
  return (uint32_t)(factor * state +
      FN(HashByte)(add) - factor_remove * FN(HashByte)(rem));
}

typedef struct HashRolling {
  uint32_t state;
  uint32_t* table;
  size_t next_ix;

  uint32_t chunk_len;
  uint32_t factor;
  uint32_t factor_remove;
} HashRolling;

static void FN(Initialize)(
    HasherCommon* common, HashRolling* BROTLI_RESTRICT self,
    const BrotliEncoderParams* params) {
  size_t i;
  self->state = 0;
  self->next_ix = 0;

  self->factor = FN(kRollingHashMul32);

  self->factor_remove = 1;
  for (i = 0; i < CHUNKLEN; i += JUMP) {
    self->factor_remove *= self->factor;
  }

  self->table = (uint32_t*)common->extra[0];
  for (i = 0; i < NUMBUCKETS; i++) {
    self->table[i] = FN(kInvalidPos);
  }

  BROTLI_UNUSED(params);
}

static void FN(Prepare)(HashRolling* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  size_t i;

  if (input_size < CHUNKLEN) return;
  self->state = 0;
  for (i = 0; i < CHUNKLEN; i += JUMP) {
    self->state = FN(HashRollingFunctionInitial)(
        self->state, data[i], self->factor);
  }
  BROTLI_UNUSED(one_shot);
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  alloc_size[0] = NUMBUCKETS * sizeof(uint32_t);
}

static BROTLI_INLINE void FN(Store)(HashRolling* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(data);
  BROTLI_UNUSED(mask);
  BROTLI_UNUSED(ix);
}

static BROTLI_INLINE void FN(StoreRange)(HashRolling* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask,
    const size_t ix_start, const size_t ix_end) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(data);
  BROTLI_UNUSED(mask);
  BROTLI_UNUSED(ix_start);
  BROTLI_UNUSED(ix_end);
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashRolling* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {

  size_t position_masked;
  size_t available = num_bytes;
  if ((position & (JUMP - 1)) != 0) {
    size_t diff = JUMP - (position & (JUMP - 1));
    available = (diff > available) ? 0 : (available - diff);
    position += diff;
  }
  position_masked = position & ring_buffer_mask;

  if (available > ring_buffer_mask - position_masked) {
    available = ring_buffer_mask - position_masked;
  }

  FN(Prepare)(self, BROTLI_FALSE, available,
      ringbuffer + (position & ring_buffer_mask));
  self->next_ix = position;
  BROTLI_UNUSED(num_bytes);
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashRolling* BROTLI_RESTRICT self,
    int* BROTLI_RESTRICT distance_cache) {
  BROTLI_UNUSED(self);
  BROTLI_UNUSED(distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashRolling* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  size_t pos;

  if ((cur_ix & (JUMP - 1)) != 0) return;

  if (max_length < CHUNKLEN) return;

  for (pos = self->next_ix; pos <= cur_ix; pos += JUMP) {
    uint32_t code = self->state & MASK;

    uint8_t rem = data[pos & ring_buffer_mask];
    uint8_t add = data[(pos + CHUNKLEN) & ring_buffer_mask];
    size_t found_ix = FN(kInvalidPos);

    self->state = FN(HashRollingFunction)(
        self->state, add, rem, self->factor, self->factor_remove);

    if (code < NUMBUCKETS) {
      found_ix = self->table[code];
      self->table[code] = (uint32_t)pos;
      if (pos == cur_ix && found_ix != FN(kInvalidPos)) {

        size_t backward = (uint32_t)(cur_ix - found_ix);
        if (backward <= max_backward) {
          const size_t found_ix_masked = found_ix & ring_buffer_mask;
          const size_t len = FindMatchLengthWithLimit(&data[found_ix_masked],
                                                      &data[cur_ix_masked],
                                                      max_length);
          if (len >= 4 && len > out->len) {
            score_t score = BackwardReferenceScore(len, backward);
            if (score > out->score) {
              out->len = len;
              out->distance = backward;
              out->score = score;
              out->len_code_delta = 0;
            }
          }
        }
      }
    }
  }

  self->next_ix = cur_ix + JUMP;

  BROTLI_UNUSED(dictionary);
  BROTLI_UNUSED(distance_cache);
  BROTLI_UNUSED(dictionary_distance);
  BROTLI_UNUSED(max_distance);
}

#undef HashRolling

#undef MASK
#undef NUMBUCKETS
#undef JUMP
#undef CHUNKLEN
#undef HASHER

#define HASHER() H35
#define HASHER_A H3
#define HASHER_B HROLLING_FAST

#define HashComposite HASHER()

#define FN_A(X) EXPAND_CAT(X, HASHER_A)
#define FN_B(X) EXPAND_CAT(X, HASHER_B)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) {
  size_t a =  FN_A(HashTypeLength)();
  size_t b =  FN_B(HashTypeLength)();
  return a > b ? a : b;
}

static BROTLI_INLINE size_t FN(StoreLookahead)(void) {
  size_t a =  FN_A(StoreLookahead)();
  size_t b =  FN_B(StoreLookahead)();
  return a > b ? a : b;
}

typedef struct HashComposite {
  HASHER_A ha;
  HASHER_B hb;
  HasherCommon ha_common;
  HasherCommon hb_common;

  HasherCommon* common;

  BROTLI_BOOL fresh;
  const BrotliEncoderParams* params;
} HashComposite;

static void FN(Initialize)(HasherCommon* common,
    HashComposite* BROTLI_RESTRICT self, const BrotliEncoderParams* params) {
  self->common = common;

  self->ha_common = *self->common;
  self->hb_common = *self->common;
  self->fresh = BROTLI_TRUE;
  self->params = params;

}

static void FN(Prepare)(
    HashComposite* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  if (self->fresh) {
    self->fresh = BROTLI_FALSE;
    self->ha_common.extra[0] = self->common->extra[0];
    self->ha_common.extra[1] = self->common->extra[1];
    self->ha_common.extra[2] = NULL;
    self->ha_common.extra[3] = NULL;
    self->hb_common.extra[0] = self->common->extra[2];
    self->hb_common.extra[1] = self->common->extra[3];
    self->hb_common.extra[2] = NULL;
    self->hb_common.extra[3] = NULL;

    FN_A(Initialize)(&self->ha_common, &self->ha, self->params);
    FN_B(Initialize)(&self->hb_common, &self->hb, self->params);
  }
  FN_A(Prepare)(&self->ha, one_shot, input_size, data);
  FN_B(Prepare)(&self->hb, one_shot, input_size, data);
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t alloc_size_a[4] = {0};
  size_t alloc_size_b[4] = {0};
  FN_A(HashMemAllocInBytes)(params, one_shot, input_size, alloc_size_a);
  FN_B(HashMemAllocInBytes)(params, one_shot, input_size, alloc_size_b);

  if (alloc_size_a[2] != 0 || alloc_size_a[3] != 0) exit(EXIT_FAILURE);
  if (alloc_size_b[2] != 0 || alloc_size_b[3] != 0) exit(EXIT_FAILURE);
  alloc_size[0] = alloc_size_a[0];
  alloc_size[1] = alloc_size_a[1];
  alloc_size[2] = alloc_size_b[0];
  alloc_size[3] = alloc_size_b[1];
}

static BROTLI_INLINE void FN(Store)(HashComposite* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  FN_A(Store)(&self->ha, data, mask, ix);
  FN_B(Store)(&self->hb, data, mask, ix);
}

static BROTLI_INLINE void FN(StoreRange)(
    HashComposite* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix_start,
    const size_t ix_end) {
  FN_A(StoreRange)(&self->ha, data, mask, ix_start, ix_end);
  FN_B(StoreRange)(&self->hb, data, mask, ix_start, ix_end);
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashComposite* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  FN_A(StitchToPreviousBlock)(&self->ha, num_bytes, position,
      ringbuffer, ring_buffer_mask);
  FN_B(StitchToPreviousBlock)(&self->hb, num_bytes, position,
      ringbuffer, ring_buffer_mask);
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashComposite* BROTLI_RESTRICT self, int* BROTLI_RESTRICT distance_cache) {
  FN_A(PrepareDistanceCache)(&self->ha, distance_cache);
  FN_B(PrepareDistanceCache)(&self->hb, distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashComposite* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  FN_A(FindLongestMatch)(&self->ha, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, dictionary_distance,
      max_distance, out);
  FN_B(FindLongestMatch)(&self->hb, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, dictionary_distance,
      max_distance, out);
}

#undef HashComposite

#undef HASHER_A
#undef HASHER_B
#undef HASHER

#define HASHER() H55
#define HASHER_A H54
#define HASHER_B HROLLING_FAST

#define HashComposite HASHER()

#define FN_A(X) EXPAND_CAT(X, HASHER_A)
#define FN_B(X) EXPAND_CAT(X, HASHER_B)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) {
  size_t a =  FN_A(HashTypeLength)();
  size_t b =  FN_B(HashTypeLength)();
  return a > b ? a : b;
}

static BROTLI_INLINE size_t FN(StoreLookahead)(void) {
  size_t a =  FN_A(StoreLookahead)();
  size_t b =  FN_B(StoreLookahead)();
  return a > b ? a : b;
}

typedef struct HashComposite {
  HASHER_A ha;
  HASHER_B hb;
  HasherCommon ha_common;
  HasherCommon hb_common;

  HasherCommon* common;

  BROTLI_BOOL fresh;
  const BrotliEncoderParams* params;
} HashComposite;

static void FN(Initialize)(HasherCommon* common,
    HashComposite* BROTLI_RESTRICT self, const BrotliEncoderParams* params) {
  self->common = common;

  self->ha_common = *self->common;
  self->hb_common = *self->common;
  self->fresh = BROTLI_TRUE;
  self->params = params;

}

static void FN(Prepare)(
    HashComposite* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  if (self->fresh) {
    self->fresh = BROTLI_FALSE;
    self->ha_common.extra[0] = self->common->extra[0];
    self->ha_common.extra[1] = self->common->extra[1];
    self->ha_common.extra[2] = NULL;
    self->ha_common.extra[3] = NULL;
    self->hb_common.extra[0] = self->common->extra[2];
    self->hb_common.extra[1] = self->common->extra[3];
    self->hb_common.extra[2] = NULL;
    self->hb_common.extra[3] = NULL;

    FN_A(Initialize)(&self->ha_common, &self->ha, self->params);
    FN_B(Initialize)(&self->hb_common, &self->hb, self->params);
  }
  FN_A(Prepare)(&self->ha, one_shot, input_size, data);
  FN_B(Prepare)(&self->hb, one_shot, input_size, data);
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t alloc_size_a[4] = {0};
  size_t alloc_size_b[4] = {0};
  FN_A(HashMemAllocInBytes)(params, one_shot, input_size, alloc_size_a);
  FN_B(HashMemAllocInBytes)(params, one_shot, input_size, alloc_size_b);

  if (alloc_size_a[2] != 0 || alloc_size_a[3] != 0) exit(EXIT_FAILURE);
  if (alloc_size_b[2] != 0 || alloc_size_b[3] != 0) exit(EXIT_FAILURE);
  alloc_size[0] = alloc_size_a[0];
  alloc_size[1] = alloc_size_a[1];
  alloc_size[2] = alloc_size_b[0];
  alloc_size[3] = alloc_size_b[1];
}

static BROTLI_INLINE void FN(Store)(HashComposite* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  FN_A(Store)(&self->ha, data, mask, ix);
  FN_B(Store)(&self->hb, data, mask, ix);
}

static BROTLI_INLINE void FN(StoreRange)(
    HashComposite* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix_start,
    const size_t ix_end) {
  FN_A(StoreRange)(&self->ha, data, mask, ix_start, ix_end);
  FN_B(StoreRange)(&self->hb, data, mask, ix_start, ix_end);
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashComposite* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  FN_A(StitchToPreviousBlock)(&self->ha, num_bytes, position,
      ringbuffer, ring_buffer_mask);
  FN_B(StitchToPreviousBlock)(&self->hb, num_bytes, position,
      ringbuffer, ring_buffer_mask);
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashComposite* BROTLI_RESTRICT self, int* BROTLI_RESTRICT distance_cache) {
  FN_A(PrepareDistanceCache)(&self->ha, distance_cache);
  FN_B(PrepareDistanceCache)(&self->hb, distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashComposite* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  FN_A(FindLongestMatch)(&self->ha, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, dictionary_distance,
      max_distance, out);
  FN_B(FindLongestMatch)(&self->hb, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, dictionary_distance,
      max_distance, out);
}

#undef HashComposite

#undef HASHER_A
#undef HASHER_B
#undef HASHER

#define HASHER() H65
#define HASHER_A H6
#define HASHER_B HROLLING

#define HashComposite HASHER()

#define FN_A(X) EXPAND_CAT(X, HASHER_A)
#define FN_B(X) EXPAND_CAT(X, HASHER_B)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) {
  size_t a =  FN_A(HashTypeLength)();
  size_t b =  FN_B(HashTypeLength)();
  return a > b ? a : b;
}

static BROTLI_INLINE size_t FN(StoreLookahead)(void) {
  size_t a =  FN_A(StoreLookahead)();
  size_t b =  FN_B(StoreLookahead)();
  return a > b ? a : b;
}

typedef struct HashComposite {
  HASHER_A ha;
  HASHER_B hb;
  HasherCommon ha_common;
  HasherCommon hb_common;

  HasherCommon* common;

  BROTLI_BOOL fresh;
  const BrotliEncoderParams* params;
} HashComposite;

static void FN(Initialize)(HasherCommon* common,
    HashComposite* BROTLI_RESTRICT self, const BrotliEncoderParams* params) {
  self->common = common;

  self->ha_common = *self->common;
  self->hb_common = *self->common;
  self->fresh = BROTLI_TRUE;
  self->params = params;

}

static void FN(Prepare)(
    HashComposite* BROTLI_RESTRICT self, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* BROTLI_RESTRICT data) {
  if (self->fresh) {
    self->fresh = BROTLI_FALSE;
    self->ha_common.extra[0] = self->common->extra[0];
    self->ha_common.extra[1] = self->common->extra[1];
    self->ha_common.extra[2] = NULL;
    self->ha_common.extra[3] = NULL;
    self->hb_common.extra[0] = self->common->extra[2];
    self->hb_common.extra[1] = self->common->extra[3];
    self->hb_common.extra[2] = NULL;
    self->hb_common.extra[3] = NULL;

    FN_A(Initialize)(&self->ha_common, &self->ha, self->params);
    FN_B(Initialize)(&self->hb_common, &self->hb, self->params);
  }
  FN_A(Prepare)(&self->ha, one_shot, input_size, data);
  FN_B(Prepare)(&self->hb, one_shot, input_size, data);
}

static BROTLI_INLINE void FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size, size_t* alloc_size) {
  size_t alloc_size_a[4] = {0};
  size_t alloc_size_b[4] = {0};
  FN_A(HashMemAllocInBytes)(params, one_shot, input_size, alloc_size_a);
  FN_B(HashMemAllocInBytes)(params, one_shot, input_size, alloc_size_b);

  if (alloc_size_a[2] != 0 || alloc_size_a[3] != 0) exit(EXIT_FAILURE);
  if (alloc_size_b[2] != 0 || alloc_size_b[3] != 0) exit(EXIT_FAILURE);
  alloc_size[0] = alloc_size_a[0];
  alloc_size[1] = alloc_size_a[1];
  alloc_size[2] = alloc_size_b[0];
  alloc_size[3] = alloc_size_b[1];
}

static BROTLI_INLINE void FN(Store)(HashComposite* BROTLI_RESTRICT self,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  FN_A(Store)(&self->ha, data, mask, ix);
  FN_B(Store)(&self->hb, data, mask, ix);
}

static BROTLI_INLINE void FN(StoreRange)(
    HashComposite* BROTLI_RESTRICT self, const uint8_t* BROTLI_RESTRICT data,
    const size_t mask, const size_t ix_start,
    const size_t ix_end) {
  FN_A(StoreRange)(&self->ha, data, mask, ix_start, ix_end);
  FN_B(StoreRange)(&self->hb, data, mask, ix_start, ix_end);
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HashComposite* BROTLI_RESTRICT self,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  FN_A(StitchToPreviousBlock)(&self->ha, num_bytes, position,
      ringbuffer, ring_buffer_mask);
  FN_B(StitchToPreviousBlock)(&self->hb, num_bytes, position,
      ringbuffer, ring_buffer_mask);
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HashComposite* BROTLI_RESTRICT self, int* BROTLI_RESTRICT distance_cache) {
  FN_A(PrepareDistanceCache)(&self->ha, distance_cache);
  FN_B(PrepareDistanceCache)(&self->hb, distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(
    HashComposite* BROTLI_RESTRICT self,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t dictionary_distance, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  FN_A(FindLongestMatch)(&self->ha, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, dictionary_distance,
      max_distance, out);
  FN_B(FindLongestMatch)(&self->hb, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, dictionary_distance,
      max_distance, out);
}

#undef HashComposite

#undef HASHER_A
#undef HASHER_B
#undef HASHER

#undef FN
#undef CAT
#undef EXPAND_CAT

#if defined(BROTLI_MAX_SIMD_QUALITY)
#define FOR_SIMPLE_HASHERS(H) \
  H(2) H(3) H(4) H(5) H(6) H(40) H(41) H(42) H(54) H(58) H(59) H(68)
#else
#define FOR_SIMPLE_HASHERS(H) \
  H(2) H(3) H(4) H(5) H(6) H(40) H(41) H(42) H(54)
#endif
#define FOR_COMPOSITE_HASHERS(H) H(35) H(55) H(65)
#define FOR_GENERIC_HASHERS(H) FOR_SIMPLE_HASHERS(H) FOR_COMPOSITE_HASHERS(H)
#define FOR_ALL_HASHERS(H) FOR_GENERIC_HASHERS(H) H(10)

typedef struct {
  HasherCommon common;

  union {
#define MEMBER_(N) \
    H ## N _H ## N;
    FOR_ALL_HASHERS(MEMBER_)
#undef MEMBER_
  } privat;
} Hasher;

static BROTLI_INLINE void HasherInit(Hasher* hasher) {
  hasher->common.is_setup_ = BROTLI_FALSE;
  hasher->common.extra[0] = NULL;
  hasher->common.extra[1] = NULL;
  hasher->common.extra[2] = NULL;
  hasher->common.extra[3] = NULL;
  hasher->common.base64_regions = NULL;
}

static BROTLI_INLINE void DestroyHasher(MemoryManager* m, Hasher* hasher) {
  if (hasher->common.extra[0] != NULL) BROTLI_FREE(m, hasher->common.extra[0]);
  if (hasher->common.extra[1] != NULL) BROTLI_FREE(m, hasher->common.extra[1]);
  if (hasher->common.extra[2] != NULL) BROTLI_FREE(m, hasher->common.extra[2]);
  if (hasher->common.extra[3] != NULL) BROTLI_FREE(m, hasher->common.extra[3]);
  if (hasher->common.base64_regions != NULL) {
    BROTLI_FREE(m, hasher->common.base64_regions);
  }
}

static BROTLI_INLINE void HasherReset(Hasher* hasher) {
  hasher->common.is_prepared_ = BROTLI_FALSE;
}

static BROTLI_INLINE void HasherSize(const BrotliEncoderParams* params,
    BROTLI_BOOL one_shot, const size_t input_size, size_t* alloc_size) {
  switch (params->hasher.type) {
#define SIZE_(N)                                                           \
    case N:                                                                \
      HashMemAllocInBytesH ## N(params, one_shot, input_size, alloc_size); \
      break;
    FOR_ALL_HASHERS(SIZE_)
#undef SIZE_
    default:
      break;
  }
}

static BROTLI_INLINE void HasherSetup(MemoryManager* m, Hasher* hasher,
    BrotliEncoderParams* params, const uint8_t* data, size_t position,
    size_t input_size, BROTLI_BOOL is_last) {
  BROTLI_BOOL one_shot = (position == 0 && is_last);
  if (!hasher->common.is_setup_) {
    size_t alloc_size[4] = {0};
    size_t i;
    ChooseHasher(params, &params->hasher);
    hasher->common.params = params->hasher;
    hasher->common.dict_num_lookups = 0;
    hasher->common.dict_num_matches = 0;
    HasherSize(params, one_shot, input_size, alloc_size);
    for (i = 0; i < 4; ++i) {
      if (alloc_size[i] == 0) continue;
      hasher->common.extra[i] = BROTLI_ALLOC(m, uint8_t, alloc_size[i]);
      if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(hasher->common.extra[i])) return;
    }
    if (params->base64_mode && params->max_base64_regions > 0) {
      hasher->common.base64_regions = BROTLI_ALLOC(
          m, Base64Region, params->max_base64_regions);
      if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(hasher->common.base64_regions)) {
        return;
      }
    }
    switch (hasher->common.params.type) {
#define INITIALIZE_(N)                        \
      case N:                                 \
        InitializeH ## N(&hasher->common,     \
            &hasher->privat._H ## N, params); \
        break;
      FOR_ALL_HASHERS(INITIALIZE_);
#undef INITIALIZE_
      default:
        break;
    }
    HasherReset(hasher);
    hasher->common.is_setup_ = BROTLI_TRUE;
  }

  if (!hasher->common.is_prepared_) {
    switch (hasher->common.params.type) {
#define PREPARE_(N)                      \
      case N:                            \
        PrepareH ## N(                   \
            &hasher->privat._H ## N,     \
            one_shot, input_size, data); \
        break;
      FOR_ALL_HASHERS(PREPARE_)
#undef PREPARE_
      default: break;
    }
    hasher->common.is_prepared_ = BROTLI_TRUE;
  }
}

static BROTLI_INLINE void InitOrStitchToPreviousBlock(
    MemoryManager* m, Hasher* hasher, const uint8_t* data, size_t mask,
    BrotliEncoderParams* params, size_t position, size_t input_size,
    BROTLI_BOOL is_last) {
  HasherSetup(m, hasher, params, data, position, input_size, is_last);
  if (BROTLI_IS_OOM(m)) return;
  switch (hasher->common.params.type) {
#define INIT_(N)                             \
    case N:                                  \
      StitchToPreviousBlockH ## N(           \
          &hasher->privat._H ## N,           \
          input_size, position, data, mask); \
    break;
    FOR_ALL_HASHERS(INIT_)
#undef INIT_
    default: break;
  }
}

static BROTLI_INLINE void FindCompoundDictionaryMatch(
    const PreparedDictionary* self, const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t distance_offset,
    const size_t max_distance, HasherSearchResult* BROTLI_RESTRICT out) {
  const uint32_t source_size = self->source_size;
  const size_t boundary = distance_offset - source_size;
  const uint32_t hash_bits = self->hash_bits;
  const uint32_t bucket_bits = self->bucket_bits;
  const uint32_t slot_bits = self->slot_bits;

  const uint32_t hash_shift = 64u - bucket_bits;
  const uint32_t slot_mask = (~((uint32_t)0U)) >> (32 - slot_bits);
  const uint64_t hash_mask = (~((uint64_t)0U)) >> (64 - hash_bits);

  const uint32_t* slot_offsets = (uint32_t*)(&self[1]);
  const uint16_t* heads = (uint16_t*)(&slot_offsets[(size_t)1u << slot_bits]);
  const uint32_t* items = (uint32_t*)(&heads[(size_t)1u << bucket_bits]);
  const uint8_t* source = NULL;

  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;
  const uint64_t h =
      (BROTLI_UNALIGNED_LOAD64LE(&data[cur_ix_masked]) & hash_mask) *
      kPreparedDictionaryHashMul64Long;
  const uint32_t key = (uint32_t)(h >> hash_shift);
  const uint32_t slot = key & slot_mask;
  const uint32_t head = heads[key];
  const uint32_t* BROTLI_RESTRICT chain = &items[slot_offsets[slot] + head];
  uint32_t item = (head == 0xFFFF) ? 1 : 0;

  const void* tail = (void*)&items[self->num_items];
  if (self->magic == kPreparedDictionaryMagic) {
    source = (const uint8_t*)tail;
  } else {

    source = (const uint8_t*)BROTLI_UNALIGNED_LOAD_PTR((const uint8_t**)tail);
  }

  for (i = 0; i < 4; ++i) {
    const size_t distance = (size_t)distance_cache[i];
    size_t offset;
    size_t limit;
    size_t len;
    if (distance <= boundary || distance > distance_offset) continue;
    offset = distance_offset - distance;
    limit = source_size - offset;
    limit = limit > max_length ? max_length : limit;
    len = FindMatchLengthWithLimit(&source[offset], &data[cur_ix_masked],
                                   limit);
    if (len >= 2) {
      score_t score = BackwardReferenceScoreUsingLastDistance(len);
      if (best_score < score) {
        if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
        if (best_score < score) {
          best_score = score;
          if (len > best_len) best_len = len;
          out->len = len;
          out->len_code_delta = 0;
          out->distance = distance;
          out->score = best_score;
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  while (item == 0) {
    size_t offset;
    size_t distance;
    size_t limit;
    item = *chain;
    chain++;
    offset = item & 0x7FFFFFFF;
    item &= 0x80000000;
    distance = distance_offset - offset;
    limit = source_size - offset;
    limit = (limit > max_length) ? max_length : limit;
    if (distance > max_distance) continue;
    if (cur_ix_masked + best_len > ring_buffer_mask || best_len >= limit ||

        BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
            BrotliUnalignedRead32(&source[offset + best_len - 3])) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&source[offset],
                                                  &data[cur_ix_masked],
                                                  limit);
      if (len >= 4) {
        score_t score = BackwardReferenceScore(len, distance);
        if (best_score < score) {
          best_score = score;
          best_len = len;
          out->len = best_len;
          out->len_code_delta = 0;
          out->distance = distance;
          out->score = best_score;
        }
      }
    }
  }
}

static BROTLI_INLINE size_t FindAllCompoundDictionaryMatches(
    const PreparedDictionary* self, const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const size_t cur_ix, const size_t min_length,
    const size_t max_length, const size_t distance_offset,
    const size_t max_distance, BackwardMatch* matches, size_t match_limit) {
  const uint32_t source_size = self->source_size;
  const uint32_t hash_bits = self->hash_bits;
  const uint32_t bucket_bits = self->bucket_bits;
  const uint32_t slot_bits = self->slot_bits;

  const uint32_t hash_shift = 64u - bucket_bits;
  const uint32_t slot_mask = (~((uint32_t)0U)) >> (32 - slot_bits);
  const uint64_t hash_mask = (~((uint64_t)0U)) >> (64 - hash_bits);

  const uint32_t* slot_offsets = (uint32_t*)(&self[1]);
  const uint16_t* heads = (uint16_t*)(&slot_offsets[(size_t)1u << slot_bits]);
  const uint32_t* items = (uint32_t*)(&heads[(size_t)1u << bucket_bits]);
  const uint8_t* source = NULL;

  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  size_t best_len = min_length;
  const uint64_t h =
      (BROTLI_UNALIGNED_LOAD64LE(&data[cur_ix_masked]) & hash_mask) *
      kPreparedDictionaryHashMul64Long;
  const uint32_t key = (uint32_t)(h >> hash_shift);
  const uint32_t slot = key & slot_mask;
  const uint32_t head = heads[key];
  const uint32_t* BROTLI_RESTRICT chain = &items[slot_offsets[slot] + head];
  uint32_t item = (head == 0xFFFF) ? 1 : 0;
  size_t found = 0;

  const void* tail = (void*)&items[self->num_items];
  if (self->magic == kPreparedDictionaryMagic) {
    source = (const uint8_t*)tail;
  } else {

    source = (const uint8_t*)BROTLI_UNALIGNED_LOAD_PTR((const uint8_t**)tail);
  }

  while (item == 0) {
    size_t offset;
    size_t distance;
    size_t limit;
    size_t len;
    item = *chain;
    chain++;
    offset = item & 0x7FFFFFFF;
    item &= 0x80000000;
    distance = distance_offset - offset;
    limit = source_size - offset;
    limit = (limit > max_length) ? max_length : limit;
    if (distance > max_distance) continue;
    if (cur_ix_masked + best_len > ring_buffer_mask ||
        best_len >= limit ||
        data[cur_ix_masked + best_len] != source[offset + best_len]) {
      continue;
    }
    len = FindMatchLengthWithLimit(
        &source[offset], &data[cur_ix_masked], limit);
    if (len > best_len) {
      best_len = len;
      InitBackwardMatch(matches++, distance, len);
      found++;
      if (found == match_limit) break;
    }
  }
  return found;
}

static BROTLI_INLINE void LookupCompoundDictionaryMatch(
    const CompoundDictionary* addon, const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length,
    const size_t max_ring_buffer_distance, const size_t max_distance,
    HasherSearchResult* sr) {
  size_t base_offset = max_ring_buffer_distance + 1 + addon->total_size - 1;
  size_t d;
  for (d = 0; d < addon->num_chunks; ++d) {

    FindCompoundDictionaryMatch(
        (const PreparedDictionary*)addon->chunks[d], data, ring_buffer_mask,
        distance_cache, cur_ix, max_length,
        base_offset - addon->chunk_offsets[d], max_distance, sr);
  }
}

static BROTLI_INLINE size_t LookupAllCompoundDictionaryMatches(
    const CompoundDictionary* addon, const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const size_t cur_ix, size_t min_length,
    const size_t max_length, const size_t max_ring_buffer_distance,
    const size_t max_distance, BackwardMatch* matches,
    size_t match_limit) {
  size_t base_offset = max_ring_buffer_distance + 1 + addon->total_size - 1;
  size_t d;
  size_t total_found = 0;
  for (d = 0; d < addon->num_chunks; ++d) {

    total_found += FindAllCompoundDictionaryMatches(
        (const PreparedDictionary*)addon->chunks[d], data, ring_buffer_mask,
        cur_ix, min_length, max_length, base_offset - addon->chunk_offsets[d],
        max_distance, matches + total_found, match_limit - total_found);
    if (total_found == match_limit) break;
    if (total_found > 0) {
      min_length = BackwardMatchLength(&matches[total_found - 1]);
    }
  }
  return total_found;
}

typedef struct PreparedDictionaryProbe {
  const uint32_t* chain;
  uint32_t item;
} PreparedDictionaryProbe;

static BROTLI_INLINE void PrefetchCompoundDictionaryMatchOpt(
    const CompoundDictionary* addon, const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const size_t cur_ix,
    PreparedDictionaryProbe* BROTLI_RESTRICT probes) {
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  const uint64_t bytes = BROTLI_UNALIGNED_LOAD64LE(&data[cur_ix_masked]);
  size_t d;
  for (d = 0; d < addon->num_chunks; ++d) {
    const PreparedDictionaryView* view = &addon->chunk_views[d];
    const uint64_t h =
        (bytes & view->hash_mask) * kPreparedDictionaryHashMul64Long;
    const uint32_t key = (uint32_t)(h >> view->hash_shift);
    const uint32_t slot = key & view->slot_mask;
    const uint32_t head = view->heads[key];

    const uint32_t* chain = &view->items[view->slot_offsets[slot] + head];
    PREFETCH_L1(chain);
    probes[d].chain = chain;
    probes[d].item = (head == 0xFFFF) ? 1 : 0;

    {
      const uint64_t h1 =
          ((bytes >> 8) & view->hash_mask) * kPreparedDictionaryHashMul64Long;
      PREFETCH_L1(&view->heads[(uint32_t)(h1 >> view->hash_shift)]);
    }
  }
}

static BROTLI_INLINE void PrefetchCompoundDictionaryHeadsOpt(
    const CompoundDictionary* addon, const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const size_t cur_ix) {
  const uint64_t bytes =
      BROTLI_UNALIGNED_LOAD64LE(&data[cur_ix & ring_buffer_mask]);
  size_t d;
  for (d = 0; d < addon->num_chunks; ++d) {
    const PreparedDictionaryView* view = &addon->chunk_views[d];
    const uint64_t h =
        (bytes & view->hash_mask) * kPreparedDictionaryHashMul64Long;
    PREFETCH_L1(&view->heads[(uint32_t)(h >> view->hash_shift)]);
  }
}

static BROTLI_INLINE void FindCompoundDictionaryMatchOpt(
    const PreparedDictionaryView* self,
    const PreparedDictionaryProbe* BROTLI_RESTRICT probe,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t distance_offset,
    const size_t max_distance, HasherSearchResult* BROTLI_RESTRICT out) {
  const uint32_t source_size = self->source_size;
  const size_t boundary = distance_offset - source_size;

  const uint8_t* source = self->source;

  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;

  const uint32_t* BROTLI_RESTRICT chain = probe->chain;
  uint32_t item = probe->item;
#if defined(BROTLI_DEBUG) || defined(BROTLI_ENABLE_LOG)
  {
    const uint64_t bytes = BROTLI_UNALIGNED_LOAD64LE(&data[cur_ix_masked]);
    const uint64_t h =
        (bytes & self->hash_mask) * kPreparedDictionaryHashMul64Long;
    const uint32_t key = (uint32_t)(h >> self->hash_shift);
    const uint32_t slot = key & self->slot_mask;
    const uint32_t head = self->heads[key];
    const uint32_t* expected_chain =
        &self->items[self->slot_offsets[slot] + head];
    const uint32_t expected_item = (head == 0xFFFF) ? 1 : 0;
    BROTLI_DCHECK(probe->chain == expected_chain);
    BROTLI_DCHECK(probe->item == expected_item);
  }
#endif

  for (i = 0; i < 4; ++i) {
    const size_t distance = (size_t)distance_cache[i];
    size_t offset;
    size_t limit;
    size_t len;
    if (distance <= boundary || distance > distance_offset) continue;
    offset = distance_offset - distance;
    limit = source_size - offset;
    limit = limit > max_length ? max_length : limit;
    len = FindMatchLengthWithLimit(&source[offset], &data[cur_ix_masked],
                                   limit);
    if (len >= 2) {
      score_t score = BackwardReferenceScoreUsingLastDistance(len);
      if (best_score < score) {
        if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
        if (best_score < score) {
          best_score = score;
          if (len > best_len) best_len = len;
          out->len = len;
          out->len_code_delta = 0;
          out->distance = distance;
          out->score = best_score;
        }
      }
    }
  }

  if (best_len < 3) {
    best_len = 3;
  }
  while (item == 0) {
    size_t offset;
    size_t distance;
    size_t limit;
    item = *chain;
    chain++;
    offset = item & 0x7FFFFFFF;
    item &= 0x80000000;
    distance = distance_offset - offset;
    limit = source_size - offset;
    limit = (limit > max_length) ? max_length : limit;
    if (distance > max_distance) continue;
    if (best_len >= limit ||

        BrotliUnalignedRead32(&data[cur_ix_masked + best_len - 3]) !=
            BrotliUnalignedRead32(&source[offset + best_len - 3])) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&source[offset],
                                                  &data[cur_ix_masked],
                                                  limit);
      if (len >= 4) {
        score_t score = BackwardReferenceScore(len, distance);
        if (best_score < score) {
          best_score = score;
          best_len = len;
          out->len = best_len;
          out->len_code_delta = 0;
          out->distance = distance;
          out->score = best_score;
        }
      }
    }
  }
}

static BROTLI_INLINE void LookupCompoundDictionaryMatchOpt(
    const CompoundDictionary* addon,
    const PreparedDictionaryProbe* BROTLI_RESTRICT probes,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length,
    const size_t max_ring_buffer_distance, const size_t max_distance,
    HasherSearchResult* sr) {
  size_t base_offset = max_ring_buffer_distance + 1 + addon->total_size - 1;
  size_t d;
  for (d = 0; d < addon->num_chunks; ++d) {
    FindCompoundDictionaryMatchOpt(
        &addon->chunk_views[d], &probes[d], data, ring_buffer_mask,
        distance_cache, cur_ix, max_length,
        base_offset - addon->chunk_offsets[d], max_distance, sr);
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL void BrotliCreateZopfliBackwardReferences(MemoryManager* m,
    size_t num_bytes,
    size_t position, const uint8_t* ringbuffer, size_t ringbuffer_mask,
    ContextLut literal_context_lut, const BrotliEncoderParams* params,
    Hasher* hasher, int* dist_cache, size_t* last_insert_len,
    Command* commands, size_t* num_commands, size_t* num_literals);

BROTLI_INTERNAL void BrotliCreateHqZopfliBackwardReferences(MemoryManager* m,
    size_t num_bytes,
    size_t position, const uint8_t* ringbuffer, size_t ringbuffer_mask,
    ContextLut literal_context_lut, const BrotliEncoderParams* params,
    Hasher* hasher, int* dist_cache, size_t* last_insert_len,
    Command* commands, size_t* num_commands, size_t* num_literals);

typedef struct ZopfliNode {

  uint32_t length;

  uint32_t distance;

  uint32_t dcode_insert_length;

  union {

    float cost;

    uint32_t next;

    uint32_t shortcut;
  } u;
} ZopfliNode;

BROTLI_INTERNAL void BrotliInitZopfliNodes(ZopfliNode* array, size_t length);

BROTLI_INTERNAL size_t BrotliZopfliComputeShortestPath(
    MemoryManager* m, size_t num_bytes,
    size_t position, const uint8_t* ringbuffer, size_t ringbuffer_mask,
    ContextLut literal_context_lut, const BrotliEncoderParams* params,
    const int* dist_cache, Hasher* hasher, ZopfliNode* nodes);

BROTLI_INTERNAL void BrotliZopfliCreateCommands(
    const size_t num_bytes, const size_t block_start, const ZopfliNode* nodes,
    int* dist_cache, size_t* last_insert_len, const BrotliEncoderParams* params,
    Command* commands, size_t* num_literals);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_LITERAL_COST_H_
#define BROTLI_ENC_LITERAL_COST_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL void BrotliEstimateBitCostsForLiterals(
    size_t pos, size_t len, size_t mask, const uint8_t* data, size_t* histogram,
    float* cost);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_BACKWARD_REFERENCES_H_
#define BROTLI_ENC_BACKWARD_REFERENCES_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL void BrotliCreateBackwardReferences(size_t num_bytes,
    size_t position, const uint8_t* ringbuffer, size_t ringbuffer_mask,
    ContextLut literal_context_lut, const BrotliEncoderParams* params,
    Hasher* hasher, int* dist_cache, size_t* last_insert_len,
    Command* commands, size_t* num_commands, size_t* num_literals);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_BIT_COST_H_
#define BROTLI_ENC_BIT_COST_H_

#ifndef BROTLI_ENC_HISTOGRAM_H_
#define BROTLI_ENC_HISTOGRAM_H_

#ifndef BROTLI_ENC_BLOCK_SPLITTER_H_
#define BROTLI_ENC_BLOCK_SPLITTER_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct BlockSplit {
  size_t num_types;
  size_t num_blocks;
  uint8_t* types;
  uint32_t* lengths;

  size_t types_alloc_size;
  size_t lengths_alloc_size;
} BlockSplit;

BROTLI_INTERNAL void BrotliInitBlockSplit(BlockSplit* self);
BROTLI_INTERNAL void BrotliDestroyBlockSplit(MemoryManager* m,
                                             BlockSplit* self);

BROTLI_INTERNAL void BrotliSplitBlock(MemoryManager* m,
                                      const Command* cmds,
                                      const size_t num_commands,
                                      const uint8_t* data,
                                      const size_t offset,
                                      const size_t mask,
                                      const BrotliEncoderParams* params,
                                      BlockSplit* literal_split,
                                      BlockSplit* insert_and_copy_split,
                                      BlockSplit* dist_split);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_NUM_HISTOGRAM_DISTANCE_SYMBOLS 544

#define FN(X) X ## Literal
#define DATA_SIZE BROTLI_NUM_LITERAL_SYMBOLS
#define DataType uint8_t

typedef struct FN(Histogram) {
  uint32_t data_[DATA_SIZE];
  size_t total_count_;
  double bit_cost_;
} FN(Histogram);

static BROTLI_INLINE void FN(HistogramClear)(FN(Histogram)* self) {
  memset(self->data_, 0, sizeof(self->data_));
  self->total_count_ = 0;
  self->bit_cost_ = HUGE_VAL;
}

static BROTLI_INLINE void FN(ClearHistograms)(
    FN(Histogram)* array, size_t length) {
  size_t i;
  for (i = 0; i < length; ++i) FN(HistogramClear)(array + i);
}

static BROTLI_INLINE void FN(HistogramAdd)(FN(Histogram)* self, size_t val) {
  ++self->data_[val];
  ++self->total_count_;
}

static BROTLI_INLINE void FN(HistogramAddVector)(FN(Histogram)* self,
    const DataType* p, size_t n) {
  self->total_count_ += n;
  n += 1;
  while (--n) ++self->data_[*p++];
}

static BROTLI_INLINE void FN(HistogramAddHistogram)(FN(Histogram)* self,
    const FN(Histogram)* v) {
  size_t i;
  self->total_count_ += v->total_count_;
  for (i = 0; i < DATA_SIZE; ++i) {
    self->data_[i] += v->data_[i];
  }
}

static BROTLI_INLINE size_t FN(HistogramDataSize)(void) { return DATA_SIZE; }

#undef DataType
#undef DATA_SIZE
#undef FN

#define FN(X) X ## Command
#define DataType uint16_t
#define DATA_SIZE BROTLI_NUM_COMMAND_SYMBOLS

typedef struct FN(Histogram) {
  uint32_t data_[DATA_SIZE];
  size_t total_count_;
  double bit_cost_;
} FN(Histogram);

static BROTLI_INLINE void FN(HistogramClear)(FN(Histogram)* self) {
  memset(self->data_, 0, sizeof(self->data_));
  self->total_count_ = 0;
  self->bit_cost_ = HUGE_VAL;
}

static BROTLI_INLINE void FN(ClearHistograms)(
    FN(Histogram)* array, size_t length) {
  size_t i;
  for (i = 0; i < length; ++i) FN(HistogramClear)(array + i);
}

static BROTLI_INLINE void FN(HistogramAdd)(FN(Histogram)* self, size_t val) {
  ++self->data_[val];
  ++self->total_count_;
}

static BROTLI_INLINE void FN(HistogramAddVector)(FN(Histogram)* self,
    const DataType* p, size_t n) {
  self->total_count_ += n;
  n += 1;
  while (--n) ++self->data_[*p++];
}

static BROTLI_INLINE void FN(HistogramAddHistogram)(FN(Histogram)* self,
    const FN(Histogram)* v) {
  size_t i;
  self->total_count_ += v->total_count_;
  for (i = 0; i < DATA_SIZE; ++i) {
    self->data_[i] += v->data_[i];
  }
}

static BROTLI_INLINE size_t FN(HistogramDataSize)(void) { return DATA_SIZE; }

#undef DATA_SIZE
#undef FN

#define FN(X) X ## Distance
#define DATA_SIZE BROTLI_NUM_HISTOGRAM_DISTANCE_SYMBOLS

typedef struct FN(Histogram) {
  uint32_t data_[DATA_SIZE];
  size_t total_count_;
  double bit_cost_;
} FN(Histogram);

static BROTLI_INLINE void FN(HistogramClear)(FN(Histogram)* self) {
  memset(self->data_, 0, sizeof(self->data_));
  self->total_count_ = 0;
  self->bit_cost_ = HUGE_VAL;
}

static BROTLI_INLINE void FN(ClearHistograms)(
    FN(Histogram)* array, size_t length) {
  size_t i;
  for (i = 0; i < length; ++i) FN(HistogramClear)(array + i);
}

static BROTLI_INLINE void FN(HistogramAdd)(FN(Histogram)* self, size_t val) {
  ++self->data_[val];
  ++self->total_count_;
}

static BROTLI_INLINE void FN(HistogramAddVector)(FN(Histogram)* self,
    const DataType* p, size_t n) {
  self->total_count_ += n;
  n += 1;
  while (--n) ++self->data_[*p++];
}

static BROTLI_INLINE void FN(HistogramAddHistogram)(FN(Histogram)* self,
    const FN(Histogram)* v) {
  size_t i;
  self->total_count_ += v->total_count_;
  for (i = 0; i < DATA_SIZE; ++i) {
    self->data_[i] += v->data_[i];
  }
}

static BROTLI_INLINE size_t FN(HistogramDataSize)(void) { return DATA_SIZE; }

#undef DataType
#undef DATA_SIZE
#undef FN

BROTLI_INTERNAL void BrotliBuildHistogramsWithContext(
    const Command* cmds, const size_t num_commands,
    const BlockSplit* literal_split, const BlockSplit* insert_and_copy_split,
    const BlockSplit* dist_split, const uint8_t* ringbuffer, size_t pos,
    size_t mask, uint8_t prev_byte, uint8_t prev_byte2,
    const ContextType* context_modes, HistogramLiteral* literal_histograms,
    HistogramCommand* insert_and_copy_histograms,
    HistogramDistance* copy_dist_histograms);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL double BrotliBitsEntropy(
    const uint32_t* population, size_t size);
BROTLI_INTERNAL double BrotliPopulationCostLiteral(
    const HistogramLiteral* histogram);
BROTLI_INTERNAL double BrotliPopulationCostCommand(
    const HistogramCommand* histogram);
BROTLI_INTERNAL double BrotliPopulationCostDistance(
    const HistogramDistance* histogram);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_CLUSTER_H_
#define BROTLI_ENC_CLUSTER_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct HistogramPair {
  uint32_t idx1;
  uint32_t idx2;
  double cost_combo;
  double cost_diff;
} HistogramPair;

#define CODE(X) ;

#define FN(X) X ## Literal

#define HistogramType FN(Histogram)

BROTLI_INTERNAL void FN(BrotliCompareAndPushToQueue)(
    const HistogramType* out, HistogramType* tmp, const uint32_t* cluster_size,
    uint32_t idx1, uint32_t idx2, size_t max_num_pairs, HistogramPair* pairs,
    size_t* num_pairs) CODE({
  BROTLI_BOOL is_good_pair = BROTLI_FALSE;
  HistogramPair p;
  p.idx1 = p.idx2 = 0;
  p.cost_diff = p.cost_combo = 0;
  if (idx1 == idx2) {
    return;
  }
  if (idx2 < idx1) {
    uint32_t t = idx2;
    idx2 = idx1;
    idx1 = t;
  }
  p.idx1 = idx1;
  p.idx2 = idx2;
  p.cost_diff = 0.5 * ClusterCostDiff(cluster_size[idx1], cluster_size[idx2]);
  p.cost_diff -= out[idx1].bit_cost_;
  p.cost_diff -= out[idx2].bit_cost_;

  if (out[idx1].total_count_ == 0) {
    p.cost_combo = out[idx2].bit_cost_;
    is_good_pair = BROTLI_TRUE;
  } else if (out[idx2].total_count_ == 0) {
    p.cost_combo = out[idx1].bit_cost_;
    is_good_pair = BROTLI_TRUE;
  } else {
    double threshold = *num_pairs == 0 ? 1e99 :
        BROTLI_MAX(double, 0.0, pairs[0].cost_diff);
    double cost_combo;
    *tmp = out[idx1];
    FN(HistogramAddHistogram)(tmp, &out[idx2]);
    cost_combo = FN(BrotliPopulationCost)(tmp);
    if (cost_combo < threshold - p.cost_diff) {
      p.cost_combo = cost_combo;
      is_good_pair = BROTLI_TRUE;
    }
  }
  if (is_good_pair) {
    p.cost_diff += p.cost_combo;
    if (*num_pairs > 0 && HistogramPairIsLess(&pairs[0], &p)) {

      if (*num_pairs < max_num_pairs) {
        pairs[*num_pairs] = pairs[0];
        ++(*num_pairs);
      }
      pairs[0] = p;
    } else if (*num_pairs < max_num_pairs) {
      pairs[*num_pairs] = p;
      ++(*num_pairs);
    }
  }
})

BROTLI_INTERNAL size_t FN(BrotliHistogramCombine)(HistogramType* out,
                                                  HistogramType* tmp,
                                                  uint32_t* cluster_size,
                                                  uint32_t* symbols,
                                                  uint32_t* clusters,
                                                  HistogramPair* pairs,
                                                  size_t num_clusters,
                                                  size_t symbols_size,
                                                  size_t max_clusters,
                                                  size_t max_num_pairs) CODE({
  double cost_diff_threshold = 0.0;
  size_t min_cluster_size = 1;
  size_t num_pairs = 0;

  {

    size_t idx1;
    for (idx1 = 0; idx1 < num_clusters; ++idx1) {
      size_t idx2;
      for (idx2 = idx1 + 1; idx2 < num_clusters; ++idx2) {
        FN(BrotliCompareAndPushToQueue)(out, tmp, cluster_size, clusters[idx1],
            clusters[idx2], max_num_pairs, &pairs[0], &num_pairs);
      }
    }
  }

  while (num_clusters > min_cluster_size) {
    uint32_t best_idx1;
    uint32_t best_idx2;
    size_t i;
    if (pairs[0].cost_diff >= cost_diff_threshold) {
      cost_diff_threshold = 1e99;
      min_cluster_size = max_clusters;
      continue;
    }

    best_idx1 = pairs[0].idx1;
    best_idx2 = pairs[0].idx2;
    FN(HistogramAddHistogram)(&out[best_idx1], &out[best_idx2]);
    out[best_idx1].bit_cost_ = pairs[0].cost_combo;
    cluster_size[best_idx1] += cluster_size[best_idx2];
    for (i = 0; i < symbols_size; ++i) {
      if (symbols[i] == best_idx2) {
        symbols[i] = best_idx1;
      }
    }
    for (i = 0; i < num_clusters; ++i) {
      if (clusters[i] == best_idx2) {
        memmove(&clusters[i], &clusters[i + 1],
                (num_clusters - i - 1) * sizeof(clusters[0]));
        break;
      }
    }
    --num_clusters;
    {

      size_t copy_to_idx = 0;
      for (i = 0; i < num_pairs; ++i) {
        HistogramPair* p = &pairs[i];
        if (p->idx1 == best_idx1 || p->idx2 == best_idx1 ||
            p->idx1 == best_idx2 || p->idx2 == best_idx2) {

          continue;
        }
        if (HistogramPairIsLess(&pairs[0], p)) {

          HistogramPair front = pairs[0];
          pairs[0] = *p;
          pairs[copy_to_idx] = front;
        } else {
          pairs[copy_to_idx] = *p;
        }
        ++copy_to_idx;
      }
      num_pairs = copy_to_idx;
    }

    for (i = 0; i < num_clusters; ++i) {
      FN(BrotliCompareAndPushToQueue)(out, tmp, cluster_size, best_idx1,
          clusters[i], max_num_pairs, &pairs[0], &num_pairs);
    }
  }
  return num_clusters;
})

BROTLI_INTERNAL double FN(BrotliHistogramBitCostDistance)(
    const HistogramType* histogram, const HistogramType* candidate,
    HistogramType* tmp) CODE({
  if (histogram->total_count_ == 0) {
    return 0.0;
  } else {
    *tmp = *histogram;
    FN(HistogramAddHistogram)(tmp, candidate);
    return FN(BrotliPopulationCost)(tmp) - candidate->bit_cost_;
  }
})

BROTLI_INTERNAL void FN(BrotliHistogramRemap)(const HistogramType* in,
    size_t in_size, const uint32_t* clusters, size_t num_clusters,
    HistogramType* out, HistogramType* tmp, uint32_t* symbols) CODE({
  size_t i;
  for (i = 0; i < in_size; ++i) {
    uint32_t best_out = i == 0 ? symbols[0] : symbols[i - 1];
    double best_bits =
        FN(BrotliHistogramBitCostDistance)(&in[i], &out[best_out], tmp);
    size_t j;
    for (j = 0; j < num_clusters; ++j) {
      const double cur_bits =
          FN(BrotliHistogramBitCostDistance)(&in[i], &out[clusters[j]], tmp);
      if (cur_bits < best_bits) {
        best_bits = cur_bits;
        best_out = clusters[j];
      }
    }
    symbols[i] = best_out;
  }

  for (i = 0; i < num_clusters; ++i) {
    FN(HistogramClear)(&out[clusters[i]]);
  }
  for (i = 0; i < in_size; ++i) {
    FN(HistogramAddHistogram)(&out[symbols[i]], &in[i]);
  }
})

BROTLI_INTERNAL size_t FN(BrotliHistogramReindex)(MemoryManager* m,
    HistogramType* out, uint32_t* symbols, size_t length) CODE({
  static const uint32_t kInvalidIndex = BROTLI_UINT32_MAX;
  uint32_t* new_index = BROTLI_ALLOC(m, uint32_t, length);
  uint32_t next_index;
  HistogramType* tmp;
  size_t i;
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(new_index)) return 0;
  for (i = 0; i < length; ++i) {
      new_index[i] = kInvalidIndex;
  }
  next_index = 0;
  for (i = 0; i < length; ++i) {
    if (new_index[symbols[i]] == kInvalidIndex) {
      new_index[symbols[i]] = next_index;
      ++next_index;
    }
  }

  tmp = BROTLI_ALLOC(m, HistogramType, next_index);
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(tmp)) return 0;
  next_index = 0;
  for (i = 0; i < length; ++i) {
    if (new_index[symbols[i]] == next_index) {
      tmp[next_index] = out[symbols[i]];
      ++next_index;
    }
    symbols[i] = new_index[symbols[i]];
  }
  BROTLI_FREE(m, new_index);
  for (i = 0; i < next_index; ++i) {
    out[i] = tmp[i];
  }
  BROTLI_FREE(m, tmp);
  return next_index;
})

BROTLI_INTERNAL void FN(BrotliClusterHistograms)(
    MemoryManager* m, const HistogramType* in, const size_t in_size,
    size_t max_histograms, HistogramType* out, size_t* out_size,
    uint32_t* histogram_symbols) CODE({
  uint32_t* cluster_size = BROTLI_ALLOC(m, uint32_t, in_size);
  uint32_t* clusters = BROTLI_ALLOC(m, uint32_t, in_size);
  size_t num_clusters = 0;
  const size_t max_input_histograms = 64;
  size_t pairs_capacity = max_input_histograms * max_input_histograms / 2;

  HistogramPair* pairs = BROTLI_ALLOC(m, HistogramPair, pairs_capacity + 1);

  HistogramType* tmp = BROTLI_ALLOC(m, HistogramType, 1);
  size_t i;

  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(cluster_size) ||
      BROTLI_IS_NULL(clusters) || BROTLI_IS_NULL(pairs)|| BROTLI_IS_NULL(tmp)) {
    return;
  }

  for (i = 0; i < in_size; ++i) {
    cluster_size[i] = 1;
  }

  for (i = 0; i < in_size; ++i) {
    out[i] = in[i];
    out[i].bit_cost_ = FN(BrotliPopulationCost)(&in[i]);
    histogram_symbols[i] = (uint32_t)i;
  }

  for (i = 0; i < in_size; i += max_input_histograms) {
    size_t num_to_combine =
        BROTLI_MIN(size_t, in_size - i, max_input_histograms);
    size_t num_new_clusters;
    size_t j;
    for (j = 0; j < num_to_combine; ++j) {
      clusters[num_clusters + j] = (uint32_t)(i + j);
    }
    num_new_clusters =
        FN(BrotliHistogramCombine)(out, tmp, cluster_size,
                                   &histogram_symbols[i],
                                   &clusters[num_clusters], pairs,
                                   num_to_combine, num_to_combine,
                                   max_histograms, pairs_capacity);
    num_clusters += num_new_clusters;
  }

  {

    size_t max_num_pairs = BROTLI_MIN(size_t,
        64 * num_clusters, (num_clusters / 2) * num_clusters);
    BROTLI_ENSURE_CAPACITY(
        m, HistogramPair, pairs, pairs_capacity, max_num_pairs + 1);
    if (BROTLI_IS_OOM(m)) return;

    num_clusters = FN(BrotliHistogramCombine)(out, tmp, cluster_size,
                                              histogram_symbols, clusters,
                                              pairs, num_clusters, in_size,
                                              max_histograms, max_num_pairs);
  }
  BROTLI_FREE(m, pairs);
  BROTLI_FREE(m, cluster_size);

  FN(BrotliHistogramRemap)(in, in_size, clusters, num_clusters,
                           out, tmp, histogram_symbols);
  BROTLI_FREE(m, tmp);
  BROTLI_FREE(m, clusters);

  *out_size = FN(BrotliHistogramReindex)(m, out, histogram_symbols, in_size);
  if (BROTLI_IS_OOM(m)) return;
})

#undef HistogramType

#undef FN

#define FN(X) X ## Command

#define HistogramType FN(Histogram)

BROTLI_INTERNAL void FN(BrotliCompareAndPushToQueue)(
    const HistogramType* out, HistogramType* tmp, const uint32_t* cluster_size,
    uint32_t idx1, uint32_t idx2, size_t max_num_pairs, HistogramPair* pairs,
    size_t* num_pairs) CODE({
  BROTLI_BOOL is_good_pair = BROTLI_FALSE;
  HistogramPair p;
  p.idx1 = p.idx2 = 0;
  p.cost_diff = p.cost_combo = 0;
  if (idx1 == idx2) {
    return;
  }
  if (idx2 < idx1) {
    uint32_t t = idx2;
    idx2 = idx1;
    idx1 = t;
  }
  p.idx1 = idx1;
  p.idx2 = idx2;
  p.cost_diff = 0.5 * ClusterCostDiff(cluster_size[idx1], cluster_size[idx2]);
  p.cost_diff -= out[idx1].bit_cost_;
  p.cost_diff -= out[idx2].bit_cost_;

  if (out[idx1].total_count_ == 0) {
    p.cost_combo = out[idx2].bit_cost_;
    is_good_pair = BROTLI_TRUE;
  } else if (out[idx2].total_count_ == 0) {
    p.cost_combo = out[idx1].bit_cost_;
    is_good_pair = BROTLI_TRUE;
  } else {
    double threshold = *num_pairs == 0 ? 1e99 :
        BROTLI_MAX(double, 0.0, pairs[0].cost_diff);
    double cost_combo;
    *tmp = out[idx1];
    FN(HistogramAddHistogram)(tmp, &out[idx2]);
    cost_combo = FN(BrotliPopulationCost)(tmp);
    if (cost_combo < threshold - p.cost_diff) {
      p.cost_combo = cost_combo;
      is_good_pair = BROTLI_TRUE;
    }
  }
  if (is_good_pair) {
    p.cost_diff += p.cost_combo;
    if (*num_pairs > 0 && HistogramPairIsLess(&pairs[0], &p)) {

      if (*num_pairs < max_num_pairs) {
        pairs[*num_pairs] = pairs[0];
        ++(*num_pairs);
      }
      pairs[0] = p;
    } else if (*num_pairs < max_num_pairs) {
      pairs[*num_pairs] = p;
      ++(*num_pairs);
    }
  }
})

BROTLI_INTERNAL size_t FN(BrotliHistogramCombine)(HistogramType* out,
                                                  HistogramType* tmp,
                                                  uint32_t* cluster_size,
                                                  uint32_t* symbols,
                                                  uint32_t* clusters,
                                                  HistogramPair* pairs,
                                                  size_t num_clusters,
                                                  size_t symbols_size,
                                                  size_t max_clusters,
                                                  size_t max_num_pairs) CODE({
  double cost_diff_threshold = 0.0;
  size_t min_cluster_size = 1;
  size_t num_pairs = 0;

  {

    size_t idx1;
    for (idx1 = 0; idx1 < num_clusters; ++idx1) {
      size_t idx2;
      for (idx2 = idx1 + 1; idx2 < num_clusters; ++idx2) {
        FN(BrotliCompareAndPushToQueue)(out, tmp, cluster_size, clusters[idx1],
            clusters[idx2], max_num_pairs, &pairs[0], &num_pairs);
      }
    }
  }

  while (num_clusters > min_cluster_size) {
    uint32_t best_idx1;
    uint32_t best_idx2;
    size_t i;
    if (pairs[0].cost_diff >= cost_diff_threshold) {
      cost_diff_threshold = 1e99;
      min_cluster_size = max_clusters;
      continue;
    }

    best_idx1 = pairs[0].idx1;
    best_idx2 = pairs[0].idx2;
    FN(HistogramAddHistogram)(&out[best_idx1], &out[best_idx2]);
    out[best_idx1].bit_cost_ = pairs[0].cost_combo;
    cluster_size[best_idx1] += cluster_size[best_idx2];
    for (i = 0; i < symbols_size; ++i) {
      if (symbols[i] == best_idx2) {
        symbols[i] = best_idx1;
      }
    }
    for (i = 0; i < num_clusters; ++i) {
      if (clusters[i] == best_idx2) {
        memmove(&clusters[i], &clusters[i + 1],
                (num_clusters - i - 1) * sizeof(clusters[0]));
        break;
      }
    }
    --num_clusters;
    {

      size_t copy_to_idx = 0;
      for (i = 0; i < num_pairs; ++i) {
        HistogramPair* p = &pairs[i];
        if (p->idx1 == best_idx1 || p->idx2 == best_idx1 ||
            p->idx1 == best_idx2 || p->idx2 == best_idx2) {

          continue;
        }
        if (HistogramPairIsLess(&pairs[0], p)) {

          HistogramPair front = pairs[0];
          pairs[0] = *p;
          pairs[copy_to_idx] = front;
        } else {
          pairs[copy_to_idx] = *p;
        }
        ++copy_to_idx;
      }
      num_pairs = copy_to_idx;
    }

    for (i = 0; i < num_clusters; ++i) {
      FN(BrotliCompareAndPushToQueue)(out, tmp, cluster_size, best_idx1,
          clusters[i], max_num_pairs, &pairs[0], &num_pairs);
    }
  }
  return num_clusters;
})

BROTLI_INTERNAL double FN(BrotliHistogramBitCostDistance)(
    const HistogramType* histogram, const HistogramType* candidate,
    HistogramType* tmp) CODE({
  if (histogram->total_count_ == 0) {
    return 0.0;
  } else {
    *tmp = *histogram;
    FN(HistogramAddHistogram)(tmp, candidate);
    return FN(BrotliPopulationCost)(tmp) - candidate->bit_cost_;
  }
})

BROTLI_INTERNAL void FN(BrotliHistogramRemap)(const HistogramType* in,
    size_t in_size, const uint32_t* clusters, size_t num_clusters,
    HistogramType* out, HistogramType* tmp, uint32_t* symbols) CODE({
  size_t i;
  for (i = 0; i < in_size; ++i) {
    uint32_t best_out = i == 0 ? symbols[0] : symbols[i - 1];
    double best_bits =
        FN(BrotliHistogramBitCostDistance)(&in[i], &out[best_out], tmp);
    size_t j;
    for (j = 0; j < num_clusters; ++j) {
      const double cur_bits =
          FN(BrotliHistogramBitCostDistance)(&in[i], &out[clusters[j]], tmp);
      if (cur_bits < best_bits) {
        best_bits = cur_bits;
        best_out = clusters[j];
      }
    }
    symbols[i] = best_out;
  }

  for (i = 0; i < num_clusters; ++i) {
    FN(HistogramClear)(&out[clusters[i]]);
  }
  for (i = 0; i < in_size; ++i) {
    FN(HistogramAddHistogram)(&out[symbols[i]], &in[i]);
  }
})

BROTLI_INTERNAL size_t FN(BrotliHistogramReindex)(MemoryManager* m,
    HistogramType* out, uint32_t* symbols, size_t length) CODE({
  static const uint32_t kInvalidIndex = BROTLI_UINT32_MAX;
  uint32_t* new_index = BROTLI_ALLOC(m, uint32_t, length);
  uint32_t next_index;
  HistogramType* tmp;
  size_t i;
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(new_index)) return 0;
  for (i = 0; i < length; ++i) {
      new_index[i] = kInvalidIndex;
  }
  next_index = 0;
  for (i = 0; i < length; ++i) {
    if (new_index[symbols[i]] == kInvalidIndex) {
      new_index[symbols[i]] = next_index;
      ++next_index;
    }
  }

  tmp = BROTLI_ALLOC(m, HistogramType, next_index);
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(tmp)) return 0;
  next_index = 0;
  for (i = 0; i < length; ++i) {
    if (new_index[symbols[i]] == next_index) {
      tmp[next_index] = out[symbols[i]];
      ++next_index;
    }
    symbols[i] = new_index[symbols[i]];
  }
  BROTLI_FREE(m, new_index);
  for (i = 0; i < next_index; ++i) {
    out[i] = tmp[i];
  }
  BROTLI_FREE(m, tmp);
  return next_index;
})

BROTLI_INTERNAL void FN(BrotliClusterHistograms)(
    MemoryManager* m, const HistogramType* in, const size_t in_size,
    size_t max_histograms, HistogramType* out, size_t* out_size,
    uint32_t* histogram_symbols) CODE({
  uint32_t* cluster_size = BROTLI_ALLOC(m, uint32_t, in_size);
  uint32_t* clusters = BROTLI_ALLOC(m, uint32_t, in_size);
  size_t num_clusters = 0;
  const size_t max_input_histograms = 64;
  size_t pairs_capacity = max_input_histograms * max_input_histograms / 2;

  HistogramPair* pairs = BROTLI_ALLOC(m, HistogramPair, pairs_capacity + 1);

  HistogramType* tmp = BROTLI_ALLOC(m, HistogramType, 1);
  size_t i;

  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(cluster_size) ||
      BROTLI_IS_NULL(clusters) || BROTLI_IS_NULL(pairs)|| BROTLI_IS_NULL(tmp)) {
    return;
  }

  for (i = 0; i < in_size; ++i) {
    cluster_size[i] = 1;
  }

  for (i = 0; i < in_size; ++i) {
    out[i] = in[i];
    out[i].bit_cost_ = FN(BrotliPopulationCost)(&in[i]);
    histogram_symbols[i] = (uint32_t)i;
  }

  for (i = 0; i < in_size; i += max_input_histograms) {
    size_t num_to_combine =
        BROTLI_MIN(size_t, in_size - i, max_input_histograms);
    size_t num_new_clusters;
    size_t j;
    for (j = 0; j < num_to_combine; ++j) {
      clusters[num_clusters + j] = (uint32_t)(i + j);
    }
    num_new_clusters =
        FN(BrotliHistogramCombine)(out, tmp, cluster_size,
                                   &histogram_symbols[i],
                                   &clusters[num_clusters], pairs,
                                   num_to_combine, num_to_combine,
                                   max_histograms, pairs_capacity);
    num_clusters += num_new_clusters;
  }

  {

    size_t max_num_pairs = BROTLI_MIN(size_t,
        64 * num_clusters, (num_clusters / 2) * num_clusters);
    BROTLI_ENSURE_CAPACITY(
        m, HistogramPair, pairs, pairs_capacity, max_num_pairs + 1);
    if (BROTLI_IS_OOM(m)) return;

    num_clusters = FN(BrotliHistogramCombine)(out, tmp, cluster_size,
                                              histogram_symbols, clusters,
                                              pairs, num_clusters, in_size,
                                              max_histograms, max_num_pairs);
  }
  BROTLI_FREE(m, pairs);
  BROTLI_FREE(m, cluster_size);

  FN(BrotliHistogramRemap)(in, in_size, clusters, num_clusters,
                           out, tmp, histogram_symbols);
  BROTLI_FREE(m, tmp);
  BROTLI_FREE(m, clusters);

  *out_size = FN(BrotliHistogramReindex)(m, out, histogram_symbols, in_size);
  if (BROTLI_IS_OOM(m)) return;
})

#undef HistogramType

#undef FN

#define FN(X) X ## Distance

#define HistogramType FN(Histogram)

BROTLI_INTERNAL void FN(BrotliCompareAndPushToQueue)(
    const HistogramType* out, HistogramType* tmp, const uint32_t* cluster_size,
    uint32_t idx1, uint32_t idx2, size_t max_num_pairs, HistogramPair* pairs,
    size_t* num_pairs) CODE({
  BROTLI_BOOL is_good_pair = BROTLI_FALSE;
  HistogramPair p;
  p.idx1 = p.idx2 = 0;
  p.cost_diff = p.cost_combo = 0;
  if (idx1 == idx2) {
    return;
  }
  if (idx2 < idx1) {
    uint32_t t = idx2;
    idx2 = idx1;
    idx1 = t;
  }
  p.idx1 = idx1;
  p.idx2 = idx2;
  p.cost_diff = 0.5 * ClusterCostDiff(cluster_size[idx1], cluster_size[idx2]);
  p.cost_diff -= out[idx1].bit_cost_;
  p.cost_diff -= out[idx2].bit_cost_;

  if (out[idx1].total_count_ == 0) {
    p.cost_combo = out[idx2].bit_cost_;
    is_good_pair = BROTLI_TRUE;
  } else if (out[idx2].total_count_ == 0) {
    p.cost_combo = out[idx1].bit_cost_;
    is_good_pair = BROTLI_TRUE;
  } else {
    double threshold = *num_pairs == 0 ? 1e99 :
        BROTLI_MAX(double, 0.0, pairs[0].cost_diff);
    double cost_combo;
    *tmp = out[idx1];
    FN(HistogramAddHistogram)(tmp, &out[idx2]);
    cost_combo = FN(BrotliPopulationCost)(tmp);
    if (cost_combo < threshold - p.cost_diff) {
      p.cost_combo = cost_combo;
      is_good_pair = BROTLI_TRUE;
    }
  }
  if (is_good_pair) {
    p.cost_diff += p.cost_combo;
    if (*num_pairs > 0 && HistogramPairIsLess(&pairs[0], &p)) {

      if (*num_pairs < max_num_pairs) {
        pairs[*num_pairs] = pairs[0];
        ++(*num_pairs);
      }
      pairs[0] = p;
    } else if (*num_pairs < max_num_pairs) {
      pairs[*num_pairs] = p;
      ++(*num_pairs);
    }
  }
})

BROTLI_INTERNAL size_t FN(BrotliHistogramCombine)(HistogramType* out,
                                                  HistogramType* tmp,
                                                  uint32_t* cluster_size,
                                                  uint32_t* symbols,
                                                  uint32_t* clusters,
                                                  HistogramPair* pairs,
                                                  size_t num_clusters,
                                                  size_t symbols_size,
                                                  size_t max_clusters,
                                                  size_t max_num_pairs) CODE({
  double cost_diff_threshold = 0.0;
  size_t min_cluster_size = 1;
  size_t num_pairs = 0;

  {

    size_t idx1;
    for (idx1 = 0; idx1 < num_clusters; ++idx1) {
      size_t idx2;
      for (idx2 = idx1 + 1; idx2 < num_clusters; ++idx2) {
        FN(BrotliCompareAndPushToQueue)(out, tmp, cluster_size, clusters[idx1],
            clusters[idx2], max_num_pairs, &pairs[0], &num_pairs);
      }
    }
  }

  while (num_clusters > min_cluster_size) {
    uint32_t best_idx1;
    uint32_t best_idx2;
    size_t i;
    if (pairs[0].cost_diff >= cost_diff_threshold) {
      cost_diff_threshold = 1e99;
      min_cluster_size = max_clusters;
      continue;
    }

    best_idx1 = pairs[0].idx1;
    best_idx2 = pairs[0].idx2;
    FN(HistogramAddHistogram)(&out[best_idx1], &out[best_idx2]);
    out[best_idx1].bit_cost_ = pairs[0].cost_combo;
    cluster_size[best_idx1] += cluster_size[best_idx2];
    for (i = 0; i < symbols_size; ++i) {
      if (symbols[i] == best_idx2) {
        symbols[i] = best_idx1;
      }
    }
    for (i = 0; i < num_clusters; ++i) {
      if (clusters[i] == best_idx2) {
        memmove(&clusters[i], &clusters[i + 1],
                (num_clusters - i - 1) * sizeof(clusters[0]));
        break;
      }
    }
    --num_clusters;
    {

      size_t copy_to_idx = 0;
      for (i = 0; i < num_pairs; ++i) {
        HistogramPair* p = &pairs[i];
        if (p->idx1 == best_idx1 || p->idx2 == best_idx1 ||
            p->idx1 == best_idx2 || p->idx2 == best_idx2) {

          continue;
        }
        if (HistogramPairIsLess(&pairs[0], p)) {

          HistogramPair front = pairs[0];
          pairs[0] = *p;
          pairs[copy_to_idx] = front;
        } else {
          pairs[copy_to_idx] = *p;
        }
        ++copy_to_idx;
      }
      num_pairs = copy_to_idx;
    }

    for (i = 0; i < num_clusters; ++i) {
      FN(BrotliCompareAndPushToQueue)(out, tmp, cluster_size, best_idx1,
          clusters[i], max_num_pairs, &pairs[0], &num_pairs);
    }
  }
  return num_clusters;
})

BROTLI_INTERNAL double FN(BrotliHistogramBitCostDistance)(
    const HistogramType* histogram, const HistogramType* candidate,
    HistogramType* tmp) CODE({
  if (histogram->total_count_ == 0) {
    return 0.0;
  } else {
    *tmp = *histogram;
    FN(HistogramAddHistogram)(tmp, candidate);
    return FN(BrotliPopulationCost)(tmp) - candidate->bit_cost_;
  }
})

BROTLI_INTERNAL void FN(BrotliHistogramRemap)(const HistogramType* in,
    size_t in_size, const uint32_t* clusters, size_t num_clusters,
    HistogramType* out, HistogramType* tmp, uint32_t* symbols) CODE({
  size_t i;
  for (i = 0; i < in_size; ++i) {
    uint32_t best_out = i == 0 ? symbols[0] : symbols[i - 1];
    double best_bits =
        FN(BrotliHistogramBitCostDistance)(&in[i], &out[best_out], tmp);
    size_t j;
    for (j = 0; j < num_clusters; ++j) {
      const double cur_bits =
          FN(BrotliHistogramBitCostDistance)(&in[i], &out[clusters[j]], tmp);
      if (cur_bits < best_bits) {
        best_bits = cur_bits;
        best_out = clusters[j];
      }
    }
    symbols[i] = best_out;
  }

  for (i = 0; i < num_clusters; ++i) {
    FN(HistogramClear)(&out[clusters[i]]);
  }
  for (i = 0; i < in_size; ++i) {
    FN(HistogramAddHistogram)(&out[symbols[i]], &in[i]);
  }
})

BROTLI_INTERNAL size_t FN(BrotliHistogramReindex)(MemoryManager* m,
    HistogramType* out, uint32_t* symbols, size_t length) CODE({
  static const uint32_t kInvalidIndex = BROTLI_UINT32_MAX;
  uint32_t* new_index = BROTLI_ALLOC(m, uint32_t, length);
  uint32_t next_index;
  HistogramType* tmp;
  size_t i;
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(new_index)) return 0;
  for (i = 0; i < length; ++i) {
      new_index[i] = kInvalidIndex;
  }
  next_index = 0;
  for (i = 0; i < length; ++i) {
    if (new_index[symbols[i]] == kInvalidIndex) {
      new_index[symbols[i]] = next_index;
      ++next_index;
    }
  }

  tmp = BROTLI_ALLOC(m, HistogramType, next_index);
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(tmp)) return 0;
  next_index = 0;
  for (i = 0; i < length; ++i) {
    if (new_index[symbols[i]] == next_index) {
      tmp[next_index] = out[symbols[i]];
      ++next_index;
    }
    symbols[i] = new_index[symbols[i]];
  }
  BROTLI_FREE(m, new_index);
  for (i = 0; i < next_index; ++i) {
    out[i] = tmp[i];
  }
  BROTLI_FREE(m, tmp);
  return next_index;
})

BROTLI_INTERNAL void FN(BrotliClusterHistograms)(
    MemoryManager* m, const HistogramType* in, const size_t in_size,
    size_t max_histograms, HistogramType* out, size_t* out_size,
    uint32_t* histogram_symbols) CODE({
  uint32_t* cluster_size = BROTLI_ALLOC(m, uint32_t, in_size);
  uint32_t* clusters = BROTLI_ALLOC(m, uint32_t, in_size);
  size_t num_clusters = 0;
  const size_t max_input_histograms = 64;
  size_t pairs_capacity = max_input_histograms * max_input_histograms / 2;

  HistogramPair* pairs = BROTLI_ALLOC(m, HistogramPair, pairs_capacity + 1);

  HistogramType* tmp = BROTLI_ALLOC(m, HistogramType, 1);
  size_t i;

  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(cluster_size) ||
      BROTLI_IS_NULL(clusters) || BROTLI_IS_NULL(pairs)|| BROTLI_IS_NULL(tmp)) {
    return;
  }

  for (i = 0; i < in_size; ++i) {
    cluster_size[i] = 1;
  }

  for (i = 0; i < in_size; ++i) {
    out[i] = in[i];
    out[i].bit_cost_ = FN(BrotliPopulationCost)(&in[i]);
    histogram_symbols[i] = (uint32_t)i;
  }

  for (i = 0; i < in_size; i += max_input_histograms) {
    size_t num_to_combine =
        BROTLI_MIN(size_t, in_size - i, max_input_histograms);
    size_t num_new_clusters;
    size_t j;
    for (j = 0; j < num_to_combine; ++j) {
      clusters[num_clusters + j] = (uint32_t)(i + j);
    }
    num_new_clusters =
        FN(BrotliHistogramCombine)(out, tmp, cluster_size,
                                   &histogram_symbols[i],
                                   &clusters[num_clusters], pairs,
                                   num_to_combine, num_to_combine,
                                   max_histograms, pairs_capacity);
    num_clusters += num_new_clusters;
  }

  {

    size_t max_num_pairs = BROTLI_MIN(size_t,
        64 * num_clusters, (num_clusters / 2) * num_clusters);
    BROTLI_ENSURE_CAPACITY(
        m, HistogramPair, pairs, pairs_capacity, max_num_pairs + 1);
    if (BROTLI_IS_OOM(m)) return;

    num_clusters = FN(BrotliHistogramCombine)(out, tmp, cluster_size,
                                              histogram_symbols, clusters,
                                              pairs, num_clusters, in_size,
                                              max_histograms, max_num_pairs);
  }
  BROTLI_FREE(m, pairs);
  BROTLI_FREE(m, cluster_size);

  FN(BrotliHistogramRemap)(in, in_size, clusters, num_clusters,
                           out, tmp, histogram_symbols);
  BROTLI_FREE(m, tmp);
  BROTLI_FREE(m, clusters);

  *out_size = FN(BrotliHistogramReindex)(m, out, histogram_symbols, in_size);
  if (BROTLI_IS_OOM(m)) return;
})

#undef HistogramType

#undef FN

#undef CODE

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_BROTLI_BIT_STREAM_H_
#define BROTLI_ENC_BROTLI_BIT_STREAM_H_

#ifndef BROTLI_ENC_ENTROPY_ENCODE_H_
#define BROTLI_ENC_ENTROPY_ENCODE_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct HuffmanTree {
  uint32_t total_count_;
  int16_t index_left_;
  int16_t index_right_or_value_;
} HuffmanTree;

static BROTLI_INLINE void InitHuffmanTree(HuffmanTree* self, uint32_t count,
    int16_t left, int16_t right) {
  self->total_count_ = count;
  self->index_left_ = left;
  self->index_right_or_value_ = right;
}

BROTLI_INTERNAL BROTLI_BOOL BrotliSetDepth(
    int p, HuffmanTree* pool, uint8_t* depth, int max_depth);

BROTLI_INTERNAL void BrotliCreateHuffmanTree(const uint32_t* data,
                                             const size_t length,
                                             const int tree_limit,
                                             HuffmanTree* tree,
                                             uint8_t* depth);

BROTLI_INTERNAL void BrotliOptimizeHuffmanCountsForRle(
    size_t length, uint32_t* counts, uint8_t* good_for_rle);

BROTLI_INTERNAL void BrotliWriteHuffmanTree(const uint8_t* depth,
                                            size_t length,
                                            size_t* tree_size,
                                            uint8_t* tree,
                                            uint8_t* extra_bits_data);

BROTLI_INTERNAL void BrotliConvertBitDepthsToSymbols(const uint8_t* depth,
                                                     size_t len,
                                                     uint16_t* bits);

BROTLI_INTERNAL extern BROTLI_MODEL("small") const size_t kBrotliShellGaps[6];

typedef BROTLI_BOOL (*HuffmanTreeComparator)(
    const HuffmanTree*, const HuffmanTree*);
static BROTLI_INLINE void SortHuffmanTreeItems(HuffmanTree* items,
    const size_t n, HuffmanTreeComparator comparator) {
  if (n < 13) {

    size_t i;
    for (i = 1; i < n; ++i) {
      HuffmanTree tmp = items[i];
      size_t k = i;
      size_t j = i - 1;
      while (comparator(&tmp, &items[j])) {
        items[k] = items[j];
        k = j;
        if (!j--) break;
      }
      items[k] = tmp;
    }
    return;
  } else {

    int g = n < 57 ? 2 : 0;
    for (; g < 6; ++g) {
      size_t gap = kBrotliShellGaps[g];
      size_t i;
      for (i = gap; i < n; ++i) {
        size_t j = i;
        HuffmanTree tmp = items[i];
        for (; j >= gap && comparator(&tmp, &items[j - gap]); j -= gap) {
          items[j] = items[j - gap];
        }
        items[j] = tmp;
      }
    }
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_METABLOCK_H_
#define BROTLI_ENC_METABLOCK_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_MAX_STATIC_CONTEXTS 13

typedef struct MetaBlockSplit {
  BlockSplit literal_split;
  BlockSplit command_split;
  BlockSplit distance_split;
  uint32_t* literal_context_map;
  size_t literal_context_map_size;
  uint32_t* distance_context_map;
  size_t distance_context_map_size;
  HistogramLiteral* literal_histograms;
  size_t literal_histograms_size;
  HistogramCommand* command_histograms;
  size_t command_histograms_size;
  HistogramDistance* distance_histograms;
  size_t distance_histograms_size;
  uint8_t literal_is_base64[32];
} MetaBlockSplit;

static BROTLI_INLINE void InitMetaBlockSplit(MetaBlockSplit* mb) {
  BrotliInitBlockSplit(&mb->literal_split);
  BrotliInitBlockSplit(&mb->command_split);
  BrotliInitBlockSplit(&mb->distance_split);
  mb->literal_context_map = 0;
  mb->literal_context_map_size = 0;
  mb->distance_context_map = 0;
  mb->distance_context_map_size = 0;
  mb->literal_histograms = 0;
  mb->literal_histograms_size = 0;
  mb->command_histograms = 0;
  mb->command_histograms_size = 0;
  mb->distance_histograms = 0;
  mb->distance_histograms_size = 0;
  memset(mb->literal_is_base64, 0, sizeof(mb->literal_is_base64));
}

static BROTLI_INLINE void DestroyMetaBlockSplit(
    MemoryManager* m, MetaBlockSplit* mb) {
  BrotliDestroyBlockSplit(m, &mb->literal_split);
  BrotliDestroyBlockSplit(m, &mb->command_split);
  BrotliDestroyBlockSplit(m, &mb->distance_split);
  BROTLI_FREE(m, mb->literal_context_map);
  BROTLI_FREE(m, mb->distance_context_map);
  BROTLI_FREE(m, mb->literal_histograms);
  BROTLI_FREE(m, mb->command_histograms);
  BROTLI_FREE(m, mb->distance_histograms);
}

BROTLI_INTERNAL void BrotliBuildMetaBlock(
    MemoryManager* m, const uint8_t* ringbuffer, const size_t pos,
    const size_t mask, const Base64Region* base64_regions,
    size_t num_base64_regions, BrotliEncoderParams* params, uint8_t prev_byte,
    uint8_t prev_byte2, Command* cmds, size_t num_commands,
    ContextType literal_context_mode, MetaBlockSplit* mb);

BROTLI_INTERNAL void BrotliBuildMetaBlockGreedy(
    MemoryManager* m, const uint8_t* ringbuffer, size_t pos, size_t mask,
    const Base64Region* base64_regions, size_t num_base64_regions,
    uint8_t prev_byte, uint8_t prev_byte2, ContextLut literal_context_lut,
    size_t num_contexts, const uint32_t* static_context_map,
    const Command* commands, size_t n_commands, MetaBlockSplit* mb);

BROTLI_INTERNAL void BrotliOptimizeHistograms(uint32_t num_distance_codes,
                                              MetaBlockSplit* mb);

BROTLI_INTERNAL void BrotliInitDistanceParams(BrotliDistanceParams* params,
    uint32_t npostfix, uint32_t ndirect, BROTLI_BOOL large_window);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL void BrotliStoreHuffmanTree(const uint8_t* depths, size_t num,
    HuffmanTree* tree, size_t* storage_ix, uint8_t* storage);

BROTLI_INTERNAL void BrotliBuildAndStoreHuffmanTreeFast(
    HuffmanTree* tree, const uint32_t* histogram, const size_t histogram_total,
    const size_t max_bits, uint8_t* depth, uint16_t* bits, size_t* storage_ix,
    uint8_t* storage);

BROTLI_INTERNAL void BrotliStoreMetaBlock(MemoryManager* m,
    const uint8_t* input, size_t start_pos, size_t length, size_t mask,
    uint8_t prev_byte, uint8_t prev_byte2, BROTLI_BOOL is_last,
    const BrotliEncoderParams* params, ContextType literal_context_mode,
    const Command* commands, size_t n_commands, const MetaBlockSplit* mb,
    size_t* storage_ix, uint8_t* storage);

BROTLI_INTERNAL void BrotliStoreMetaBlockTrivial(MemoryManager* m,
    const uint8_t* input, size_t start_pos, size_t length, size_t mask,
    BROTLI_BOOL is_last, const BrotliEncoderParams* params,
    const Command* commands, size_t n_commands,
    size_t* storage_ix, uint8_t* storage);

BROTLI_INTERNAL void BrotliStoreMetaBlockFast(MemoryManager* m,
    const uint8_t* input, size_t start_pos, size_t length, size_t mask,
    BROTLI_BOOL is_last, const BrotliEncoderParams* params,
    const Command* commands, size_t n_commands,
    size_t* storage_ix, uint8_t* storage);

BROTLI_INTERNAL void BrotliStoreUncompressedMetaBlock(
    BROTLI_BOOL is_final_block, const uint8_t* BROTLI_RESTRICT input,
    size_t position, size_t mask, size_t len,
    size_t* BROTLI_RESTRICT storage_ix, uint8_t* BROTLI_RESTRICT storage);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_ENTROPY_ENCODE_STATIC_H_
#define BROTLI_ENC_ENTROPY_ENCODE_STATIC_H_

#ifndef BROTLI_ENC_WRITE_BITS_H_
#define BROTLI_ENC_WRITE_BITS_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static BROTLI_INLINE void BrotliWriteBits(size_t n_bits,
                                          uint64_t bits,
                                          size_t* BROTLI_RESTRICT pos,
                                          uint8_t* BROTLI_RESTRICT array) {
  BROTLI_LOG(("WriteBits  %2d  0x%08x%08x  %10d\n", (int)n_bits,
      (uint32_t)(bits >> 32), (uint32_t)(bits & 0xFFFFFFFF),
      (int)*pos));
  BROTLI_DCHECK((bits >> n_bits) == 0);
  BROTLI_DCHECK(n_bits <= 56);
#if defined(BROTLI_LITTLE_ENDIAN)

  {
    uint8_t* p = &array[*pos >> 3];
    uint64_t v = (uint64_t)(*p);
    v |= bits << (*pos & 7);
    BROTLI_UNALIGNED_STORE64LE(p, v);
    *pos += n_bits;
  }
#else

  {
    uint8_t* array_pos = &array[*pos >> 3];
    const size_t bits_reserved_in_first_byte = (*pos & 7);
    size_t bits_left_to_write;
    bits <<= bits_reserved_in_first_byte;
    *array_pos++ |= (uint8_t)bits;
    for (bits_left_to_write = n_bits + bits_reserved_in_first_byte;
         bits_left_to_write >= 9;
         bits_left_to_write -= 8) {
      bits >>= 8;
      *array_pos++ = (uint8_t)bits;
    }
    *array_pos = 0;
    *pos += n_bits;
  }
#endif
}

static BROTLI_INLINE void BrotliWriteBitsPrepareStorage(
    size_t pos, uint8_t* array) {
  BROTLI_LOG(("WriteBitsPrepareStorage            %10d\n", (int)pos));
  BROTLI_DCHECK((pos & 7) == 0);
  array[pos >> 3] = 0;
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static const BROTLI_MODEL("small") uint8_t kCodeLengthDepth[18] = {
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 0, 4, 4,
};

static const BROTLI_MODEL("small")
uint8_t kStaticCommandCodeDepth[BROTLI_NUM_COMMAND_SYMBOLS] = {
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
   9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
};

static const BROTLI_MODEL("small")
uint8_t kStaticDistanceCodeDepth[64] = {
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
};

static const BROTLI_MODEL("small")
uint32_t kCodeLengthBits[18] = {
  0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 15, 31, 0, 11, 7,
};

static BROTLI_INLINE void StoreStaticCodeLengthCode(
    size_t* storage_ix, uint8_t* storage) {
  BrotliWriteBits(
      40, BROTLI_MAKE_UINT64_T(0x0000FFu, 0x55555554u), storage_ix, storage);
}

static const BROTLI_MODEL("small")
uint64_t kZeroRepsBits[BROTLI_NUM_COMMAND_SYMBOLS] = {
  0x00000000, 0x00000000, 0x00000000, 0x00000007, 0x00000017, 0x00000027,
  0x00000037, 0x00000047, 0x00000057, 0x00000067, 0x00000077, 0x00000770,
  0x00000b87, 0x00001387, 0x00001b87, 0x00002387, 0x00002b87, 0x00003387,
  0x00003b87, 0x00000397, 0x00000b97, 0x00001397, 0x00001b97, 0x00002397,
  0x00002b97, 0x00003397, 0x00003b97, 0x000003a7, 0x00000ba7, 0x000013a7,
  0x00001ba7, 0x000023a7, 0x00002ba7, 0x000033a7, 0x00003ba7, 0x000003b7,
  0x00000bb7, 0x000013b7, 0x00001bb7, 0x000023b7, 0x00002bb7, 0x000033b7,
  0x00003bb7, 0x000003c7, 0x00000bc7, 0x000013c7, 0x00001bc7, 0x000023c7,
  0x00002bc7, 0x000033c7, 0x00003bc7, 0x000003d7, 0x00000bd7, 0x000013d7,
  0x00001bd7, 0x000023d7, 0x00002bd7, 0x000033d7, 0x00003bd7, 0x000003e7,
  0x00000be7, 0x000013e7, 0x00001be7, 0x000023e7, 0x00002be7, 0x000033e7,
  0x00003be7, 0x000003f7, 0x00000bf7, 0x000013f7, 0x00001bf7, 0x000023f7,
  0x00002bf7, 0x000033f7, 0x00003bf7, 0x0001c387, 0x0005c387, 0x0009c387,
  0x000dc387, 0x0011c387, 0x0015c387, 0x0019c387, 0x001dc387, 0x0001cb87,
  0x0005cb87, 0x0009cb87, 0x000dcb87, 0x0011cb87, 0x0015cb87, 0x0019cb87,
  0x001dcb87, 0x0001d387, 0x0005d387, 0x0009d387, 0x000dd387, 0x0011d387,
  0x0015d387, 0x0019d387, 0x001dd387, 0x0001db87, 0x0005db87, 0x0009db87,
  0x000ddb87, 0x0011db87, 0x0015db87, 0x0019db87, 0x001ddb87, 0x0001e387,
  0x0005e387, 0x0009e387, 0x000de387, 0x0011e387, 0x0015e387, 0x0019e387,
  0x001de387, 0x0001eb87, 0x0005eb87, 0x0009eb87, 0x000deb87, 0x0011eb87,
  0x0015eb87, 0x0019eb87, 0x001deb87, 0x0001f387, 0x0005f387, 0x0009f387,
  0x000df387, 0x0011f387, 0x0015f387, 0x0019f387, 0x001df387, 0x0001fb87,
  0x0005fb87, 0x0009fb87, 0x000dfb87, 0x0011fb87, 0x0015fb87, 0x0019fb87,
  0x001dfb87, 0x0001c397, 0x0005c397, 0x0009c397, 0x000dc397, 0x0011c397,
  0x0015c397, 0x0019c397, 0x001dc397, 0x0001cb97, 0x0005cb97, 0x0009cb97,
  0x000dcb97, 0x0011cb97, 0x0015cb97, 0x0019cb97, 0x001dcb97, 0x0001d397,
  0x0005d397, 0x0009d397, 0x000dd397, 0x0011d397, 0x0015d397, 0x0019d397,
  0x001dd397, 0x0001db97, 0x0005db97, 0x0009db97, 0x000ddb97, 0x0011db97,
  0x0015db97, 0x0019db97, 0x001ddb97, 0x0001e397, 0x0005e397, 0x0009e397,
  0x000de397, 0x0011e397, 0x0015e397, 0x0019e397, 0x001de397, 0x0001eb97,
  0x0005eb97, 0x0009eb97, 0x000deb97, 0x0011eb97, 0x0015eb97, 0x0019eb97,
  0x001deb97, 0x0001f397, 0x0005f397, 0x0009f397, 0x000df397, 0x0011f397,
  0x0015f397, 0x0019f397, 0x001df397, 0x0001fb97, 0x0005fb97, 0x0009fb97,
  0x000dfb97, 0x0011fb97, 0x0015fb97, 0x0019fb97, 0x001dfb97, 0x0001c3a7,
  0x0005c3a7, 0x0009c3a7, 0x000dc3a7, 0x0011c3a7, 0x0015c3a7, 0x0019c3a7,
  0x001dc3a7, 0x0001cba7, 0x0005cba7, 0x0009cba7, 0x000dcba7, 0x0011cba7,
  0x0015cba7, 0x0019cba7, 0x001dcba7, 0x0001d3a7, 0x0005d3a7, 0x0009d3a7,
  0x000dd3a7, 0x0011d3a7, 0x0015d3a7, 0x0019d3a7, 0x001dd3a7, 0x0001dba7,
  0x0005dba7, 0x0009dba7, 0x000ddba7, 0x0011dba7, 0x0015dba7, 0x0019dba7,
  0x001ddba7, 0x0001e3a7, 0x0005e3a7, 0x0009e3a7, 0x000de3a7, 0x0011e3a7,
  0x0015e3a7, 0x0019e3a7, 0x001de3a7, 0x0001eba7, 0x0005eba7, 0x0009eba7,
  0x000deba7, 0x0011eba7, 0x0015eba7, 0x0019eba7, 0x001deba7, 0x0001f3a7,
  0x0005f3a7, 0x0009f3a7, 0x000df3a7, 0x0011f3a7, 0x0015f3a7, 0x0019f3a7,
  0x001df3a7, 0x0001fba7, 0x0005fba7, 0x0009fba7, 0x000dfba7, 0x0011fba7,
  0x0015fba7, 0x0019fba7, 0x001dfba7, 0x0001c3b7, 0x0005c3b7, 0x0009c3b7,
  0x000dc3b7, 0x0011c3b7, 0x0015c3b7, 0x0019c3b7, 0x001dc3b7, 0x0001cbb7,
  0x0005cbb7, 0x0009cbb7, 0x000dcbb7, 0x0011cbb7, 0x0015cbb7, 0x0019cbb7,
  0x001dcbb7, 0x0001d3b7, 0x0005d3b7, 0x0009d3b7, 0x000dd3b7, 0x0011d3b7,
  0x0015d3b7, 0x0019d3b7, 0x001dd3b7, 0x0001dbb7, 0x0005dbb7, 0x0009dbb7,
  0x000ddbb7, 0x0011dbb7, 0x0015dbb7, 0x0019dbb7, 0x001ddbb7, 0x0001e3b7,
  0x0005e3b7, 0x0009e3b7, 0x000de3b7, 0x0011e3b7, 0x0015e3b7, 0x0019e3b7,
  0x001de3b7, 0x0001ebb7, 0x0005ebb7, 0x0009ebb7, 0x000debb7, 0x0011ebb7,
  0x0015ebb7, 0x0019ebb7, 0x001debb7, 0x0001f3b7, 0x0005f3b7, 0x0009f3b7,
  0x000df3b7, 0x0011f3b7, 0x0015f3b7, 0x0019f3b7, 0x001df3b7, 0x0001fbb7,
  0x0005fbb7, 0x0009fbb7, 0x000dfbb7, 0x0011fbb7, 0x0015fbb7, 0x0019fbb7,
  0x001dfbb7, 0x0001c3c7, 0x0005c3c7, 0x0009c3c7, 0x000dc3c7, 0x0011c3c7,
  0x0015c3c7, 0x0019c3c7, 0x001dc3c7, 0x0001cbc7, 0x0005cbc7, 0x0009cbc7,
  0x000dcbc7, 0x0011cbc7, 0x0015cbc7, 0x0019cbc7, 0x001dcbc7, 0x0001d3c7,
  0x0005d3c7, 0x0009d3c7, 0x000dd3c7, 0x0011d3c7, 0x0015d3c7, 0x0019d3c7,
  0x001dd3c7, 0x0001dbc7, 0x0005dbc7, 0x0009dbc7, 0x000ddbc7, 0x0011dbc7,
  0x0015dbc7, 0x0019dbc7, 0x001ddbc7, 0x0001e3c7, 0x0005e3c7, 0x0009e3c7,
  0x000de3c7, 0x0011e3c7, 0x0015e3c7, 0x0019e3c7, 0x001de3c7, 0x0001ebc7,
  0x0005ebc7, 0x0009ebc7, 0x000debc7, 0x0011ebc7, 0x0015ebc7, 0x0019ebc7,
  0x001debc7, 0x0001f3c7, 0x0005f3c7, 0x0009f3c7, 0x000df3c7, 0x0011f3c7,
  0x0015f3c7, 0x0019f3c7, 0x001df3c7, 0x0001fbc7, 0x0005fbc7, 0x0009fbc7,
  0x000dfbc7, 0x0011fbc7, 0x0015fbc7, 0x0019fbc7, 0x001dfbc7, 0x0001c3d7,
  0x0005c3d7, 0x0009c3d7, 0x000dc3d7, 0x0011c3d7, 0x0015c3d7, 0x0019c3d7,
  0x001dc3d7, 0x0001cbd7, 0x0005cbd7, 0x0009cbd7, 0x000dcbd7, 0x0011cbd7,
  0x0015cbd7, 0x0019cbd7, 0x001dcbd7, 0x0001d3d7, 0x0005d3d7, 0x0009d3d7,
  0x000dd3d7, 0x0011d3d7, 0x0015d3d7, 0x0019d3d7, 0x001dd3d7, 0x0001dbd7,
  0x0005dbd7, 0x0009dbd7, 0x000ddbd7, 0x0011dbd7, 0x0015dbd7, 0x0019dbd7,
  0x001ddbd7, 0x0001e3d7, 0x0005e3d7, 0x0009e3d7, 0x000de3d7, 0x0011e3d7,
  0x0015e3d7, 0x0019e3d7, 0x001de3d7, 0x0001ebd7, 0x0005ebd7, 0x0009ebd7,
  0x000debd7, 0x0011ebd7, 0x0015ebd7, 0x0019ebd7, 0x001debd7, 0x0001f3d7,
  0x0005f3d7, 0x0009f3d7, 0x000df3d7, 0x0011f3d7, 0x0015f3d7, 0x0019f3d7,
  0x001df3d7, 0x0001fbd7, 0x0005fbd7, 0x0009fbd7, 0x000dfbd7, 0x0011fbd7,
  0x0015fbd7, 0x0019fbd7, 0x001dfbd7, 0x0001c3e7, 0x0005c3e7, 0x0009c3e7,
  0x000dc3e7, 0x0011c3e7, 0x0015c3e7, 0x0019c3e7, 0x001dc3e7, 0x0001cbe7,
  0x0005cbe7, 0x0009cbe7, 0x000dcbe7, 0x0011cbe7, 0x0015cbe7, 0x0019cbe7,
  0x001dcbe7, 0x0001d3e7, 0x0005d3e7, 0x0009d3e7, 0x000dd3e7, 0x0011d3e7,
  0x0015d3e7, 0x0019d3e7, 0x001dd3e7, 0x0001dbe7, 0x0005dbe7, 0x0009dbe7,
  0x000ddbe7, 0x0011dbe7, 0x0015dbe7, 0x0019dbe7, 0x001ddbe7, 0x0001e3e7,
  0x0005e3e7, 0x0009e3e7, 0x000de3e7, 0x0011e3e7, 0x0015e3e7, 0x0019e3e7,
  0x001de3e7, 0x0001ebe7, 0x0005ebe7, 0x0009ebe7, 0x000debe7, 0x0011ebe7,
  0x0015ebe7, 0x0019ebe7, 0x001debe7, 0x0001f3e7, 0x0005f3e7, 0x0009f3e7,
  0x000df3e7, 0x0011f3e7, 0x0015f3e7, 0x0019f3e7, 0x001df3e7, 0x0001fbe7,
  0x0005fbe7, 0x0009fbe7, 0x000dfbe7, 0x0011fbe7, 0x0015fbe7, 0x0019fbe7,
  0x001dfbe7, 0x0001c3f7, 0x0005c3f7, 0x0009c3f7, 0x000dc3f7, 0x0011c3f7,
  0x0015c3f7, 0x0019c3f7, 0x001dc3f7, 0x0001cbf7, 0x0005cbf7, 0x0009cbf7,
  0x000dcbf7, 0x0011cbf7, 0x0015cbf7, 0x0019cbf7, 0x001dcbf7, 0x0001d3f7,
  0x0005d3f7, 0x0009d3f7, 0x000dd3f7, 0x0011d3f7, 0x0015d3f7, 0x0019d3f7,
  0x001dd3f7, 0x0001dbf7, 0x0005dbf7, 0x0009dbf7, 0x000ddbf7, 0x0011dbf7,
  0x0015dbf7, 0x0019dbf7, 0x001ddbf7, 0x0001e3f7, 0x0005e3f7, 0x0009e3f7,
  0x000de3f7, 0x0011e3f7, 0x0015e3f7, 0x0019e3f7, 0x001de3f7, 0x0001ebf7,
  0x0005ebf7, 0x0009ebf7, 0x000debf7, 0x0011ebf7, 0x0015ebf7, 0x0019ebf7,
  0x001debf7, 0x0001f3f7, 0x0005f3f7, 0x0009f3f7, 0x000df3f7, 0x0011f3f7,
  0x0015f3f7, 0x0019f3f7, 0x001df3f7, 0x0001fbf7, 0x0005fbf7, 0x0009fbf7,
  0x000dfbf7, 0x0011fbf7, 0x0015fbf7, 0x0019fbf7, 0x001dfbf7, 0x00e1c387,
  0x02e1c387, 0x04e1c387, 0x06e1c387, 0x08e1c387, 0x0ae1c387, 0x0ce1c387,
  0x0ee1c387, 0x00e5c387, 0x02e5c387, 0x04e5c387, 0x06e5c387, 0x08e5c387,
  0x0ae5c387, 0x0ce5c387, 0x0ee5c387, 0x00e9c387, 0x02e9c387, 0x04e9c387,
  0x06e9c387, 0x08e9c387, 0x0ae9c387, 0x0ce9c387, 0x0ee9c387, 0x00edc387,
  0x02edc387, 0x04edc387, 0x06edc387, 0x08edc387, 0x0aedc387, 0x0cedc387,
  0x0eedc387, 0x00f1c387, 0x02f1c387, 0x04f1c387, 0x06f1c387, 0x08f1c387,
  0x0af1c387, 0x0cf1c387, 0x0ef1c387, 0x00f5c387, 0x02f5c387, 0x04f5c387,
  0x06f5c387, 0x08f5c387, 0x0af5c387, 0x0cf5c387, 0x0ef5c387, 0x00f9c387,
  0x02f9c387, 0x04f9c387, 0x06f9c387, 0x08f9c387, 0x0af9c387, 0x0cf9c387,
  0x0ef9c387, 0x00fdc387, 0x02fdc387, 0x04fdc387, 0x06fdc387, 0x08fdc387,
  0x0afdc387, 0x0cfdc387, 0x0efdc387, 0x00e1cb87, 0x02e1cb87, 0x04e1cb87,
  0x06e1cb87, 0x08e1cb87, 0x0ae1cb87, 0x0ce1cb87, 0x0ee1cb87, 0x00e5cb87,
  0x02e5cb87, 0x04e5cb87, 0x06e5cb87, 0x08e5cb87, 0x0ae5cb87, 0x0ce5cb87,
  0x0ee5cb87, 0x00e9cb87, 0x02e9cb87, 0x04e9cb87, 0x06e9cb87, 0x08e9cb87,
  0x0ae9cb87, 0x0ce9cb87, 0x0ee9cb87, 0x00edcb87, 0x02edcb87, 0x04edcb87,
  0x06edcb87, 0x08edcb87, 0x0aedcb87, 0x0cedcb87, 0x0eedcb87, 0x00f1cb87,
  0x02f1cb87, 0x04f1cb87, 0x06f1cb87, 0x08f1cb87, 0x0af1cb87, 0x0cf1cb87,
  0x0ef1cb87, 0x00f5cb87, 0x02f5cb87, 0x04f5cb87, 0x06f5cb87, 0x08f5cb87,
  0x0af5cb87, 0x0cf5cb87, 0x0ef5cb87, 0x00f9cb87, 0x02f9cb87, 0x04f9cb87,
  0x06f9cb87, 0x08f9cb87,
};

static const BROTLI_MODEL("small")
uint32_t kZeroRepsDepth[BROTLI_NUM_COMMAND_SYMBOLS] = {
   0,  4,  8,  7,  7,  7,  7,  7,  7,  7,  7, 11, 14, 14, 14, 14,
  14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
  14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
  14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
  14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
};

static const BROTLI_MODEL("small")
uint64_t kNonZeroRepsBits[BROTLI_NUM_COMMAND_SYMBOLS] = {
  0x0000000b, 0x0000001b, 0x0000002b, 0x0000003b, 0x000002cb, 0x000006cb,
  0x00000acb, 0x00000ecb, 0x000002db, 0x000006db, 0x00000adb, 0x00000edb,
  0x000002eb, 0x000006eb, 0x00000aeb, 0x00000eeb, 0x000002fb, 0x000006fb,
  0x00000afb, 0x00000efb, 0x0000b2cb, 0x0001b2cb, 0x0002b2cb, 0x0003b2cb,
  0x0000b6cb, 0x0001b6cb, 0x0002b6cb, 0x0003b6cb, 0x0000bacb, 0x0001bacb,
  0x0002bacb, 0x0003bacb, 0x0000becb, 0x0001becb, 0x0002becb, 0x0003becb,
  0x0000b2db, 0x0001b2db, 0x0002b2db, 0x0003b2db, 0x0000b6db, 0x0001b6db,
  0x0002b6db, 0x0003b6db, 0x0000badb, 0x0001badb, 0x0002badb, 0x0003badb,
  0x0000bedb, 0x0001bedb, 0x0002bedb, 0x0003bedb, 0x0000b2eb, 0x0001b2eb,
  0x0002b2eb, 0x0003b2eb, 0x0000b6eb, 0x0001b6eb, 0x0002b6eb, 0x0003b6eb,
  0x0000baeb, 0x0001baeb, 0x0002baeb, 0x0003baeb, 0x0000beeb, 0x0001beeb,
  0x0002beeb, 0x0003beeb, 0x0000b2fb, 0x0001b2fb, 0x0002b2fb, 0x0003b2fb,
  0x0000b6fb, 0x0001b6fb, 0x0002b6fb, 0x0003b6fb, 0x0000bafb, 0x0001bafb,
  0x0002bafb, 0x0003bafb, 0x0000befb, 0x0001befb, 0x0002befb, 0x0003befb,
  0x002cb2cb, 0x006cb2cb, 0x00acb2cb, 0x00ecb2cb, 0x002db2cb, 0x006db2cb,
  0x00adb2cb, 0x00edb2cb, 0x002eb2cb, 0x006eb2cb, 0x00aeb2cb, 0x00eeb2cb,
  0x002fb2cb, 0x006fb2cb, 0x00afb2cb, 0x00efb2cb, 0x002cb6cb, 0x006cb6cb,
  0x00acb6cb, 0x00ecb6cb, 0x002db6cb, 0x006db6cb, 0x00adb6cb, 0x00edb6cb,
  0x002eb6cb, 0x006eb6cb, 0x00aeb6cb, 0x00eeb6cb, 0x002fb6cb, 0x006fb6cb,
  0x00afb6cb, 0x00efb6cb, 0x002cbacb, 0x006cbacb, 0x00acbacb, 0x00ecbacb,
  0x002dbacb, 0x006dbacb, 0x00adbacb, 0x00edbacb, 0x002ebacb, 0x006ebacb,
  0x00aebacb, 0x00eebacb, 0x002fbacb, 0x006fbacb, 0x00afbacb, 0x00efbacb,
  0x002cbecb, 0x006cbecb, 0x00acbecb, 0x00ecbecb, 0x002dbecb, 0x006dbecb,
  0x00adbecb, 0x00edbecb, 0x002ebecb, 0x006ebecb, 0x00aebecb, 0x00eebecb,
  0x002fbecb, 0x006fbecb, 0x00afbecb, 0x00efbecb, 0x002cb2db, 0x006cb2db,
  0x00acb2db, 0x00ecb2db, 0x002db2db, 0x006db2db, 0x00adb2db, 0x00edb2db,
  0x002eb2db, 0x006eb2db, 0x00aeb2db, 0x00eeb2db, 0x002fb2db, 0x006fb2db,
  0x00afb2db, 0x00efb2db, 0x002cb6db, 0x006cb6db, 0x00acb6db, 0x00ecb6db,
  0x002db6db, 0x006db6db, 0x00adb6db, 0x00edb6db, 0x002eb6db, 0x006eb6db,
  0x00aeb6db, 0x00eeb6db, 0x002fb6db, 0x006fb6db, 0x00afb6db, 0x00efb6db,
  0x002cbadb, 0x006cbadb, 0x00acbadb, 0x00ecbadb, 0x002dbadb, 0x006dbadb,
  0x00adbadb, 0x00edbadb, 0x002ebadb, 0x006ebadb, 0x00aebadb, 0x00eebadb,
  0x002fbadb, 0x006fbadb, 0x00afbadb, 0x00efbadb, 0x002cbedb, 0x006cbedb,
  0x00acbedb, 0x00ecbedb, 0x002dbedb, 0x006dbedb, 0x00adbedb, 0x00edbedb,
  0x002ebedb, 0x006ebedb, 0x00aebedb, 0x00eebedb, 0x002fbedb, 0x006fbedb,
  0x00afbedb, 0x00efbedb, 0x002cb2eb, 0x006cb2eb, 0x00acb2eb, 0x00ecb2eb,
  0x002db2eb, 0x006db2eb, 0x00adb2eb, 0x00edb2eb, 0x002eb2eb, 0x006eb2eb,
  0x00aeb2eb, 0x00eeb2eb, 0x002fb2eb, 0x006fb2eb, 0x00afb2eb, 0x00efb2eb,
  0x002cb6eb, 0x006cb6eb, 0x00acb6eb, 0x00ecb6eb, 0x002db6eb, 0x006db6eb,
  0x00adb6eb, 0x00edb6eb, 0x002eb6eb, 0x006eb6eb, 0x00aeb6eb, 0x00eeb6eb,
  0x002fb6eb, 0x006fb6eb, 0x00afb6eb, 0x00efb6eb, 0x002cbaeb, 0x006cbaeb,
  0x00acbaeb, 0x00ecbaeb, 0x002dbaeb, 0x006dbaeb, 0x00adbaeb, 0x00edbaeb,
  0x002ebaeb, 0x006ebaeb, 0x00aebaeb, 0x00eebaeb, 0x002fbaeb, 0x006fbaeb,
  0x00afbaeb, 0x00efbaeb, 0x002cbeeb, 0x006cbeeb, 0x00acbeeb, 0x00ecbeeb,
  0x002dbeeb, 0x006dbeeb, 0x00adbeeb, 0x00edbeeb, 0x002ebeeb, 0x006ebeeb,
  0x00aebeeb, 0x00eebeeb, 0x002fbeeb, 0x006fbeeb, 0x00afbeeb, 0x00efbeeb,
  0x002cb2fb, 0x006cb2fb, 0x00acb2fb, 0x00ecb2fb, 0x002db2fb, 0x006db2fb,
  0x00adb2fb, 0x00edb2fb, 0x002eb2fb, 0x006eb2fb, 0x00aeb2fb, 0x00eeb2fb,
  0x002fb2fb, 0x006fb2fb, 0x00afb2fb, 0x00efb2fb, 0x002cb6fb, 0x006cb6fb,
  0x00acb6fb, 0x00ecb6fb, 0x002db6fb, 0x006db6fb, 0x00adb6fb, 0x00edb6fb,
  0x002eb6fb, 0x006eb6fb, 0x00aeb6fb, 0x00eeb6fb, 0x002fb6fb, 0x006fb6fb,
  0x00afb6fb, 0x00efb6fb, 0x002cbafb, 0x006cbafb, 0x00acbafb, 0x00ecbafb,
  0x002dbafb, 0x006dbafb, 0x00adbafb, 0x00edbafb, 0x002ebafb, 0x006ebafb,
  0x00aebafb, 0x00eebafb, 0x002fbafb, 0x006fbafb, 0x00afbafb, 0x00efbafb,
  0x002cbefb, 0x006cbefb, 0x00acbefb, 0x00ecbefb, 0x002dbefb, 0x006dbefb,
  0x00adbefb, 0x00edbefb, 0x002ebefb, 0x006ebefb, 0x00aebefb, 0x00eebefb,
  0x002fbefb, 0x006fbefb, 0x00afbefb, 0x00efbefb, 0x0b2cb2cb, 0x1b2cb2cb,
  0x2b2cb2cb, 0x3b2cb2cb, 0x0b6cb2cb, 0x1b6cb2cb, 0x2b6cb2cb, 0x3b6cb2cb,
  0x0bacb2cb, 0x1bacb2cb, 0x2bacb2cb, 0x3bacb2cb, 0x0becb2cb, 0x1becb2cb,
  0x2becb2cb, 0x3becb2cb, 0x0b2db2cb, 0x1b2db2cb, 0x2b2db2cb, 0x3b2db2cb,
  0x0b6db2cb, 0x1b6db2cb, 0x2b6db2cb, 0x3b6db2cb, 0x0badb2cb, 0x1badb2cb,
  0x2badb2cb, 0x3badb2cb, 0x0bedb2cb, 0x1bedb2cb, 0x2bedb2cb, 0x3bedb2cb,
  0x0b2eb2cb, 0x1b2eb2cb, 0x2b2eb2cb, 0x3b2eb2cb, 0x0b6eb2cb, 0x1b6eb2cb,
  0x2b6eb2cb, 0x3b6eb2cb, 0x0baeb2cb, 0x1baeb2cb, 0x2baeb2cb, 0x3baeb2cb,
  0x0beeb2cb, 0x1beeb2cb, 0x2beeb2cb, 0x3beeb2cb, 0x0b2fb2cb, 0x1b2fb2cb,
  0x2b2fb2cb, 0x3b2fb2cb, 0x0b6fb2cb, 0x1b6fb2cb, 0x2b6fb2cb, 0x3b6fb2cb,
  0x0bafb2cb, 0x1bafb2cb, 0x2bafb2cb, 0x3bafb2cb, 0x0befb2cb, 0x1befb2cb,
  0x2befb2cb, 0x3befb2cb, 0x0b2cb6cb, 0x1b2cb6cb, 0x2b2cb6cb, 0x3b2cb6cb,
  0x0b6cb6cb, 0x1b6cb6cb, 0x2b6cb6cb, 0x3b6cb6cb, 0x0bacb6cb, 0x1bacb6cb,
  0x2bacb6cb, 0x3bacb6cb, 0x0becb6cb, 0x1becb6cb, 0x2becb6cb, 0x3becb6cb,
  0x0b2db6cb, 0x1b2db6cb, 0x2b2db6cb, 0x3b2db6cb, 0x0b6db6cb, 0x1b6db6cb,
  0x2b6db6cb, 0x3b6db6cb, 0x0badb6cb, 0x1badb6cb, 0x2badb6cb, 0x3badb6cb,
  0x0bedb6cb, 0x1bedb6cb, 0x2bedb6cb, 0x3bedb6cb, 0x0b2eb6cb, 0x1b2eb6cb,
  0x2b2eb6cb, 0x3b2eb6cb, 0x0b6eb6cb, 0x1b6eb6cb, 0x2b6eb6cb, 0x3b6eb6cb,
  0x0baeb6cb, 0x1baeb6cb, 0x2baeb6cb, 0x3baeb6cb, 0x0beeb6cb, 0x1beeb6cb,
  0x2beeb6cb, 0x3beeb6cb, 0x0b2fb6cb, 0x1b2fb6cb, 0x2b2fb6cb, 0x3b2fb6cb,
  0x0b6fb6cb, 0x1b6fb6cb, 0x2b6fb6cb, 0x3b6fb6cb, 0x0bafb6cb, 0x1bafb6cb,
  0x2bafb6cb, 0x3bafb6cb, 0x0befb6cb, 0x1befb6cb, 0x2befb6cb, 0x3befb6cb,
  0x0b2cbacb, 0x1b2cbacb, 0x2b2cbacb, 0x3b2cbacb, 0x0b6cbacb, 0x1b6cbacb,
  0x2b6cbacb, 0x3b6cbacb, 0x0bacbacb, 0x1bacbacb, 0x2bacbacb, 0x3bacbacb,
  0x0becbacb, 0x1becbacb, 0x2becbacb, 0x3becbacb, 0x0b2dbacb, 0x1b2dbacb,
  0x2b2dbacb, 0x3b2dbacb, 0x0b6dbacb, 0x1b6dbacb, 0x2b6dbacb, 0x3b6dbacb,
  0x0badbacb, 0x1badbacb, 0x2badbacb, 0x3badbacb, 0x0bedbacb, 0x1bedbacb,
  0x2bedbacb, 0x3bedbacb, 0x0b2ebacb, 0x1b2ebacb, 0x2b2ebacb, 0x3b2ebacb,
  0x0b6ebacb, 0x1b6ebacb, 0x2b6ebacb, 0x3b6ebacb, 0x0baebacb, 0x1baebacb,
  0x2baebacb, 0x3baebacb, 0x0beebacb, 0x1beebacb, 0x2beebacb, 0x3beebacb,
  0x0b2fbacb, 0x1b2fbacb, 0x2b2fbacb, 0x3b2fbacb, 0x0b6fbacb, 0x1b6fbacb,
  0x2b6fbacb, 0x3b6fbacb, 0x0bafbacb, 0x1bafbacb, 0x2bafbacb, 0x3bafbacb,
  0x0befbacb, 0x1befbacb, 0x2befbacb, 0x3befbacb, 0x0b2cbecb, 0x1b2cbecb,
  0x2b2cbecb, 0x3b2cbecb, 0x0b6cbecb, 0x1b6cbecb, 0x2b6cbecb, 0x3b6cbecb,
  0x0bacbecb, 0x1bacbecb, 0x2bacbecb, 0x3bacbecb, 0x0becbecb, 0x1becbecb,
  0x2becbecb, 0x3becbecb, 0x0b2dbecb, 0x1b2dbecb, 0x2b2dbecb, 0x3b2dbecb,
  0x0b6dbecb, 0x1b6dbecb, 0x2b6dbecb, 0x3b6dbecb, 0x0badbecb, 0x1badbecb,
  0x2badbecb, 0x3badbecb, 0x0bedbecb, 0x1bedbecb, 0x2bedbecb, 0x3bedbecb,
  0x0b2ebecb, 0x1b2ebecb, 0x2b2ebecb, 0x3b2ebecb, 0x0b6ebecb, 0x1b6ebecb,
  0x2b6ebecb, 0x3b6ebecb, 0x0baebecb, 0x1baebecb, 0x2baebecb, 0x3baebecb,
  0x0beebecb, 0x1beebecb, 0x2beebecb, 0x3beebecb, 0x0b2fbecb, 0x1b2fbecb,
  0x2b2fbecb, 0x3b2fbecb, 0x0b6fbecb, 0x1b6fbecb, 0x2b6fbecb, 0x3b6fbecb,
  0x0bafbecb, 0x1bafbecb, 0x2bafbecb, 0x3bafbecb, 0x0befbecb, 0x1befbecb,
  0x2befbecb, 0x3befbecb, 0x0b2cb2db, 0x1b2cb2db, 0x2b2cb2db, 0x3b2cb2db,
  0x0b6cb2db, 0x1b6cb2db, 0x2b6cb2db, 0x3b6cb2db, 0x0bacb2db, 0x1bacb2db,
  0x2bacb2db, 0x3bacb2db, 0x0becb2db, 0x1becb2db, 0x2becb2db, 0x3becb2db,
  0x0b2db2db, 0x1b2db2db, 0x2b2db2db, 0x3b2db2db, 0x0b6db2db, 0x1b6db2db,
  0x2b6db2db, 0x3b6db2db, 0x0badb2db, 0x1badb2db, 0x2badb2db, 0x3badb2db,
  0x0bedb2db, 0x1bedb2db, 0x2bedb2db, 0x3bedb2db, 0x0b2eb2db, 0x1b2eb2db,
  0x2b2eb2db, 0x3b2eb2db, 0x0b6eb2db, 0x1b6eb2db, 0x2b6eb2db, 0x3b6eb2db,
  0x0baeb2db, 0x1baeb2db, 0x2baeb2db, 0x3baeb2db, 0x0beeb2db, 0x1beeb2db,
  0x2beeb2db, 0x3beeb2db, 0x0b2fb2db, 0x1b2fb2db, 0x2b2fb2db, 0x3b2fb2db,
  0x0b6fb2db, 0x1b6fb2db, 0x2b6fb2db, 0x3b6fb2db, 0x0bafb2db, 0x1bafb2db,
  0x2bafb2db, 0x3bafb2db, 0x0befb2db, 0x1befb2db, 0x2befb2db, 0x3befb2db,
  0x0b2cb6db, 0x1b2cb6db, 0x2b2cb6db, 0x3b2cb6db, 0x0b6cb6db, 0x1b6cb6db,
  0x2b6cb6db, 0x3b6cb6db, 0x0bacb6db, 0x1bacb6db, 0x2bacb6db, 0x3bacb6db,
  0x0becb6db, 0x1becb6db, 0x2becb6db, 0x3becb6db, 0x0b2db6db, 0x1b2db6db,
  0x2b2db6db, 0x3b2db6db, 0x0b6db6db, 0x1b6db6db, 0x2b6db6db, 0x3b6db6db,
  0x0badb6db, 0x1badb6db, 0x2badb6db, 0x3badb6db, 0x0bedb6db, 0x1bedb6db,
  0x2bedb6db, 0x3bedb6db, 0x0b2eb6db, 0x1b2eb6db, 0x2b2eb6db, 0x3b2eb6db,
  0x0b6eb6db, 0x1b6eb6db, 0x2b6eb6db, 0x3b6eb6db, 0x0baeb6db, 0x1baeb6db,
  0x2baeb6db, 0x3baeb6db,
};

static const BROTLI_MODEL("small")
uint32_t kNonZeroRepsDepth[BROTLI_NUM_COMMAND_SYMBOLS] = {
   6,  6,  6,  6, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
  12, 12, 12, 12, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 18, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
};

static const BROTLI_MODEL("small")
uint16_t kStaticCommandCodeBits[BROTLI_NUM_COMMAND_SYMBOLS] = {
    0,  256,  128,  384,   64,  320,  192,  448,
   32,  288,  160,  416,   96,  352,  224,  480,
   16,  272,  144,  400,   80,  336,  208,  464,
   48,  304,  176,  432,  112,  368,  240,  496,
    8,  264,  136,  392,   72,  328,  200,  456,
   40,  296,  168,  424,  104,  360,  232,  488,
   24,  280,  152,  408,   88,  344,  216,  472,
   56,  312,  184,  440,  120,  376,  248,  504,
    4,  260,  132,  388,   68,  324,  196,  452,
   36,  292,  164,  420,  100,  356,  228,  484,
   20,  276,  148,  404,   84,  340,  212,  468,
   52,  308,  180,  436,  116,  372,  244,  500,
   12,  268,  140,  396,   76,  332,  204,  460,
   44,  300,  172,  428,  108,  364,  236,  492,
   28,  284,  156,  412,   92,  348,  220,  476,
   60,  316,  188,  444,  124,  380,  252,  508,
    2,  258,  130,  386,   66,  322,  194,  450,
   34,  290,  162,  418,   98,  354,  226,  482,
   18,  274,  146,  402,   82,  338,  210,  466,
   50,  306,  178,  434,  114,  370,  242,  498,
   10,  266,  138,  394,   74,  330,  202,  458,
   42,  298,  170,  426,  106,  362,  234,  490,
   26,  282,  154,  410,   90,  346,  218,  474,
   58,  314,  186,  442,  122,  378,  250,  506,
    6,  262,  134,  390,   70,  326,  198,  454,
   38,  294,  166,  422,  102,  358,  230,  486,
   22,  278,  150,  406,   86,  342,  214,  470,
   54,  310,  182,  438,  118,  374,  246,  502,
   14,  270,  142,  398,   78,  334,  206,  462,
   46,  302,  174,  430,  110,  366,  238,  494,
   30,  286,  158,  414,   94,  350,  222,  478,
   62,  318,  190,  446,  126,  382,  254,  510,
    1,  257,  129,  385,   65,  321,  193,  449,
   33,  289,  161,  417,   97,  353,  225,  481,
   17,  273,  145,  401,   81,  337,  209,  465,
   49,  305,  177,  433,  113,  369,  241,  497,
    9,  265,  137,  393,   73,  329,  201,  457,
   41,  297,  169,  425,  105,  361,  233,  489,
   25,  281,  153,  409,   89,  345,  217,  473,
   57,  313,  185,  441,  121,  377,  249,  505,
    5,  261,  133,  389,   69,  325,  197,  453,
   37,  293,  165,  421,  101,  357,  229,  485,
   21,  277,  149,  405,   85,  341,  213,  469,
   53,  309,  181,  437,  117,  373,  245,  501,
   13,  269,  141,  397,   77,  333,  205,  461,
   45,  301,  173,  429,  109,  365,  237,  493,
   29,  285,  157,  413,   93,  349,  221,  477,
   61,  317,  189,  445,  125,  381,  253,  509,
    3,  259,  131,  387,   67,  323,  195,  451,
   35,  291,  163,  419,   99,  355,  227,  483,
   19,  275,  147,  403,   83,  339,  211,  467,
   51,  307,  179,  435,  115,  371,  243,  499,
   11,  267,  139,  395,   75,  331,  203,  459,
   43,  299,  171,  427,  107,  363,  235,  491,
   27,  283,  155,  411,   91,  347,  219,  475,
   59,  315,  187,  443,  123,  379,  251,  507,
    7, 1031,  519, 1543,  263, 1287,  775, 1799,
  135, 1159,  647, 1671,  391, 1415,  903, 1927,
   71, 1095,  583, 1607,  327, 1351,  839, 1863,
  199, 1223,  711, 1735,  455, 1479,  967, 1991,
   39, 1063,  551, 1575,  295, 1319,  807, 1831,
  167, 1191,  679, 1703,  423, 1447,  935, 1959,
  103, 1127,  615, 1639,  359, 1383,  871, 1895,
  231, 1255,  743, 1767,  487, 1511,  999, 2023,
   23, 1047,  535, 1559,  279, 1303,  791, 1815,
  151, 1175,  663, 1687,  407, 1431,  919, 1943,
   87, 1111,  599, 1623,  343, 1367,  855, 1879,
  215, 1239,  727, 1751,  471, 1495,  983, 2007,
   55, 1079,  567, 1591,  311, 1335,  823, 1847,
  183, 1207,  695, 1719,  439, 1463,  951, 1975,
  119, 1143,  631, 1655,  375, 1399,  887, 1911,
  247, 1271,  759, 1783,  503, 1527, 1015, 2039,
   15, 1039,  527, 1551,  271, 1295,  783, 1807,
  143, 1167,  655, 1679,  399, 1423,  911, 1935,
   79, 1103,  591, 1615,  335, 1359,  847, 1871,
  207, 1231,  719, 1743,  463, 1487,  975, 1999,
   47, 1071,  559, 1583,  303, 1327,  815, 1839,
  175, 1199,  687, 1711,  431, 1455,  943, 1967,
  111, 1135,  623, 1647,  367, 1391,  879, 1903,
  239, 1263,  751, 1775,  495, 1519, 1007, 2031,
   31, 1055,  543, 1567,  287, 1311,  799, 1823,
  159, 1183,  671, 1695,  415, 1439,  927, 1951,
   95, 1119,  607, 1631,  351, 1375,  863, 1887,
  223, 1247,  735, 1759,  479, 1503,  991, 2015,
   63, 1087,  575, 1599,  319, 1343,  831, 1855,
  191, 1215,  703, 1727,  447, 1471,  959, 1983,
  127, 1151,  639, 1663,  383, 1407,  895, 1919,
  255, 1279,  767, 1791,  511, 1535, 1023, 2047,
};

static BROTLI_INLINE void StoreStaticCommandHuffmanTree(
    size_t* storage_ix, uint8_t* storage) {
  BrotliWriteBits(
      56, BROTLI_MAKE_UINT64_T(0x926244U, 0x16307003U), storage_ix, storage);
  BrotliWriteBits(3, 0x00000000U, storage_ix, storage);
}

static const BROTLI_MODEL("small") uint16_t kStaticDistanceCodeBits[64] = {
   0, 32, 16, 48,  8, 40, 24, 56,  4, 36, 20, 52, 12, 44, 28, 60,
   2, 34, 18, 50, 10, 42, 26, 58,  6, 38, 22, 54, 14, 46, 30, 62,
   1, 33, 17, 49,  9, 41, 25, 57,  5, 37, 21, 53, 13, 45, 29, 61,
   3, 35, 19, 51, 11, 43, 27, 59,  7, 39, 23, 55, 15, 47, 31, 63,
};

static BROTLI_INLINE void StoreStaticDistanceHuffmanTree(
    size_t* storage_ix, uint8_t* storage) {
  BrotliWriteBits(28, 0x0369DC03u, storage_ix, storage);
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_COMPRESS_FRAGMENT_TWO_PASS_H_
#define BROTLI_ENC_COMPRESS_FRAGMENT_TWO_PASS_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static const size_t kCompressFragmentTwoPassBlockSize = 1 << 17;

typedef struct BrotliTwoPassArena {
  uint32_t lit_histo[256];
  uint8_t lit_depth[256];
  uint16_t lit_bits[256];

  uint32_t cmd_histo[128];
  uint8_t cmd_depth[128];
  uint16_t cmd_bits[128];

  HuffmanTree tmp_tree[2 * BROTLI_NUM_LITERAL_SYMBOLS + 1];
  uint8_t tmp_depth[BROTLI_NUM_COMMAND_SYMBOLS];
  uint16_t tmp_bits[64];
} BrotliTwoPassArena;

BROTLI_INTERNAL void BrotliCompressFragmentTwoPass(BrotliTwoPassArena* s,
                                                   const uint8_t* input,
                                                   size_t input_size,
                                                   BROTLI_BOOL is_last,
                                                   uint32_t* command_buf,
                                                   uint8_t* literal_buf,
                                                   int* table,
                                                   size_t table_size,
                                                   size_t* storage_ix,
                                                   uint8_t* storage);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_COMPRESS_FRAGMENT_H_
#define BROTLI_ENC_COMPRESS_FRAGMENT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct BrotliOnePassArena {
  uint8_t lit_depth[256];
  uint16_t lit_bits[256];

  uint8_t cmd_depth[128];
  uint16_t cmd_bits[128];
  uint32_t cmd_histo[128];

  uint8_t cmd_code[512];
  size_t cmd_code_numbits;

  HuffmanTree tree[2 * BROTLI_NUM_LITERAL_SYMBOLS + 1];
  uint32_t histogram[256];
  uint8_t tmp_depth[BROTLI_NUM_COMMAND_SYMBOLS];
  uint16_t tmp_bits[64];
} BrotliOnePassArena;

BROTLI_INTERNAL void BrotliCompressFragmentFast(BrotliOnePassArena* s,
                                                const uint8_t* input,
                                                size_t input_size,
                                                BROTLI_BOOL is_last,
                                                int* table, size_t table_size,
                                                size_t* storage_ix,
                                                uint8_t* storage);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_DICTIONARY_HASH_H_
#define BROTLI_ENC_DICTIONARY_HASH_H_

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_ENC_NUM_HASH_BUCKETS 32768

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
BROTLI_BOOL BROTLI_INTERNAL BrotliEncoderInitDictionaryHash(
    const BrotliDictionary* dictionary, uint16_t* words, uint8_t* lengths);
BROTLI_INTERNAL extern BROTLI_MODEL("small") uint16_t
    kStaticDictionaryHashWords[BROTLI_ENC_NUM_HASH_BUCKETS];
BROTLI_INTERNAL extern BROTLI_MODEL("small") uint8_t
    kStaticDictionaryHashLengths[BROTLI_ENC_NUM_HASH_BUCKETS];
#else
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint16_t
    kStaticDictionaryHashWords[BROTLI_ENC_NUM_HASH_BUCKETS];
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint8_t
    kStaticDictionaryHashLengths[BROTLI_ENC_NUM_HASH_BUCKETS];
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_RINGBUFFER_H_
#define BROTLI_ENC_RINGBUFFER_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct RingBuffer {

  const uint32_t size_;
  const uint32_t mask_;
  const uint32_t tail_size_;
  const uint32_t total_size_;

  uint32_t cur_size_;

  uint32_t pos_;

  uint8_t* data_;

  uint8_t* buffer_;
} RingBuffer;

static BROTLI_INLINE void RingBufferInit(RingBuffer* rb) {
  rb->cur_size_ = 0;
  rb->pos_ = 0;
  rb->data_ = 0;
  rb->buffer_ = 0;
}

static BROTLI_INLINE void RingBufferSetup(
    const BrotliEncoderParams* params, RingBuffer* rb) {
  int window_bits = ComputeRbBits(params);
  int tail_bits = params->lgblock;
  *(uint32_t*)&rb->size_ = 1u << window_bits;
  *(uint32_t*)&rb->mask_ = (1u << window_bits) - 1;
  *(uint32_t*)&rb->tail_size_ = 1u << tail_bits;
  *(uint32_t*)&rb->total_size_ = rb->size_ + rb->tail_size_;
}

static BROTLI_INLINE void RingBufferFree(MemoryManager* m, RingBuffer* rb) {
  BROTLI_FREE(m, rb->data_);
}

static BROTLI_INLINE void RingBufferInitBuffer(
    MemoryManager* m, const uint32_t buflen, RingBuffer* rb) {
  static const size_t kSlackForEightByteHashingEverywhere = 7;
  uint8_t* new_data = BROTLI_ALLOC(
      m, uint8_t, 2 + buflen + kSlackForEightByteHashingEverywhere);
  size_t i;
  if (BROTLI_IS_OOM(m) || BROTLI_IS_NULL(new_data)) return;
  if (rb->data_) {
    memcpy(new_data, rb->data_,
        2 + rb->cur_size_ + kSlackForEightByteHashingEverywhere);
    BROTLI_FREE(m, rb->data_);
  }
  rb->data_ = new_data;
  rb->cur_size_ = buflen;
  rb->buffer_ = rb->data_ + 2;
  rb->buffer_[-2] = rb->buffer_[-1] = 0;
  for (i = 0; i < kSlackForEightByteHashingEverywhere; ++i) {
    rb->buffer_[rb->cur_size_ + i] = 0;
  }
}

static BROTLI_INLINE void RingBufferWriteTail(
    const uint8_t* bytes, size_t n, RingBuffer* rb) {
  const size_t masked_pos = rb->pos_ & rb->mask_;
  if (BROTLI_PREDICT_FALSE(masked_pos < rb->tail_size_)) {

    const size_t p = rb->size_ + masked_pos;
    memcpy(&rb->buffer_[p], bytes,
        BROTLI_MIN(size_t, n, rb->tail_size_ - masked_pos));
  }
}

static BROTLI_INLINE void RingBufferWrite(
    MemoryManager* m, const uint8_t* bytes, size_t n, RingBuffer* rb) {
  if (rb->pos_ == 0 && n < rb->tail_size_) {

    rb->pos_ = (uint32_t)n;
    RingBufferInitBuffer(m, rb->pos_, rb);
    if (BROTLI_IS_OOM(m)) return;
    memcpy(rb->buffer_, bytes, n);
    return;
  }
  if (rb->cur_size_ < rb->total_size_) {

    RingBufferInitBuffer(m, rb->total_size_, rb);
    if (BROTLI_IS_OOM(m)) return;

    rb->buffer_[rb->size_ - 2] = 0;
    rb->buffer_[rb->size_ - 1] = 0;

    memcpy(&rb->buffer_[rb->size_], rb->buffer_,
           BROTLI_MIN(size_t, rb->pos_, rb->tail_size_));
  }
  {
    const size_t masked_pos = rb->pos_ & rb->mask_;

    RingBufferWriteTail(bytes, n, rb);
    if (BROTLI_PREDICT_TRUE(masked_pos + n <= rb->size_)) {

      memcpy(&rb->buffer_[masked_pos], bytes, n);
    } else {

      memcpy(&rb->buffer_[masked_pos], bytes,
             BROTLI_MIN(size_t, n, rb->total_size_ - masked_pos));

      memcpy(&rb->buffer_[0], bytes + (rb->size_ - masked_pos),
             n - (rb->size_ - masked_pos));
    }
  }
  {
    BROTLI_BOOL not_first_lap = (rb->pos_ & (1u << 31)) != 0;
    uint32_t rb_pos_mask = (1u << 31) - 1;
    rb->buffer_[-2] = rb->buffer_[rb->size_ - 2];
    rb->buffer_[-1] = rb->buffer_[rb->size_ - 1];
    rb->pos_ = (rb->pos_ & rb_pos_mask) + (uint32_t)(n & rb_pos_mask);
    if (not_first_lap) {

      rb->pos_ |= 1u << 31;
    }
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_STATE_H_
#define BROTLI_ENC_STATE_H_

typedef enum BrotliEncoderStreamState {

  BROTLI_STREAM_PROCESSING = 0,

  BROTLI_STREAM_FLUSH_REQUESTED = 1,

  BROTLI_STREAM_FINISHED = 2,

  BROTLI_STREAM_METADATA_HEAD = 3,

  BROTLI_STREAM_METADATA_BODY = 4
} BrotliEncoderStreamState;

typedef enum BrotliEncoderFlintState {
  BROTLI_FLINT_NEEDS_2_BYTES = 2,
  BROTLI_FLINT_NEEDS_1_BYTE = 1,
  BROTLI_FLINT_WAITING_FOR_PROCESSING = 0,
  BROTLI_FLINT_WAITING_FOR_FLUSHING = -1,
  BROTLI_FLINT_DONE = -2
} BrotliEncoderFlintState;

typedef struct BrotliEncoderStateStruct {
  BrotliEncoderParams params;

  MemoryManager memory_manager_;

  uint64_t input_pos_;
  RingBuffer ringbuffer_;
  size_t cmd_alloc_size_;
  Command* commands_;
  size_t num_commands_;
  size_t num_literals_;
  size_t last_insert_len_;
  uint64_t last_flush_pos_;
  uint64_t last_processed_pos_;
  int dist_cache_[BROTLI_NUM_DISTANCE_SHORT_CODES];
  int saved_dist_cache_[4];
  uint16_t last_bytes_;
  uint8_t last_bytes_bits_;

  int8_t flint_;
  uint8_t prev_byte_;
  uint8_t prev_byte2_;
  size_t storage_size_;
  uint8_t* storage_;

  Hasher hasher_;

  int small_table_[1 << 10];
  int* large_table_;
  size_t large_table_size_;

  BrotliOnePassArena* one_pass_arena_;
  BrotliTwoPassArena* two_pass_arena_;

  uint32_t* command_buf_;
  uint8_t* literal_buf_;

  uint64_t total_in_;
  uint8_t* next_out_;
  size_t available_out_;
  uint64_t total_out_;

  union {
    uint64_t u64[2];
    uint8_t u8[16];
  } tiny_buf_;
  uint32_t remaining_metadata_bytes_;
  BrotliEncoderStreamState stream_state_;

  BROTLI_BOOL is_last_block_emitted_;
  BROTLI_BOOL is_initialized_;
} BrotliEncoderStateStruct;

typedef struct BrotliEncoderStateStruct BrotliEncoderStateInternal;
#define BrotliEncoderState BrotliEncoderStateInternal

#endif

#ifndef THIRD_PARTY_BROTLI_ENC_STATIC_INIT_H_
#define THIRD_PARTY_BROTLI_ENC_STATIC_INIT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_LAZY)
BROTLI_INTERNAL void BrotliEncoderLazyStaticInitInner(void);
BROTLI_INTERNAL void BrotliEncoderLazyStaticInit(void);
#endif

BROTLI_INTERNAL BROTLI_BOOL BrotliEncoderEnsureStaticInit(void);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

#ifndef BROTLI_ENC_UTF8_UTIL_H_
#define BROTLI_ENC_UTF8_UTIL_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static const double kMinUTF8Ratio = 0.75;

BROTLI_INTERNAL BROTLI_BOOL BrotliIsMostlyUTF8(
    const uint8_t* data, const size_t pos, const size_t mask,
    const size_t length, const double min_fraction);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
