/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/**
 * @file
 * Common types used in decoder and encoder API.
 */

#ifndef BROTLI_COMMON_TYPES_H_
#define BROTLI_COMMON_TYPES_H_

#include <stddef.h>  /* IWYU pragma: export */

#if defined(_MSC_VER) && (_MSC_VER < 1600)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;
#else
#include <stdint.h>  /* IWYU pragma: export */
#endif  /* defined(_MSC_VER) && (_MSC_VER < 1600) */

/**
 * A portable @c bool replacement.
 *
 * ::BROTLI_BOOL is a "documentation" type: actually it is @c int, but in API it
 * denotes a type, whose only values are ::BROTLI_TRUE and ::BROTLI_FALSE.
 *
 * ::BROTLI_BOOL values passed to Brotli should either be ::BROTLI_TRUE or
 * ::BROTLI_FALSE, or be a result of ::TO_BROTLI_BOOL macros.
 *
 * ::BROTLI_BOOL values returned by Brotli should not be tested for equality
 * with @c true, @c false, ::BROTLI_TRUE, ::BROTLI_FALSE, but rather should be
 * evaluated, for example: @code{.cpp}
 * if (SomeBrotliFunction(encoder, BROTLI_TRUE) &&
 *     !OtherBrotliFunction(decoder, BROTLI_FALSE)) {
 *   bool x = !!YetAnotherBrotliFunction(encoder, TO_BROTLI_BOOL(2 * 2 == 4));
 *   DoSomething(x);
 * }
 * @endcode
 */
#define BROTLI_BOOL int
/** Portable @c true replacement. */
#define BROTLI_TRUE 1
/** Portable @c false replacement. */
#define BROTLI_FALSE 0
/** @c bool to ::BROTLI_BOOL conversion macros. */
#define TO_BROTLI_BOOL(X) (!!(X) ? BROTLI_TRUE : BROTLI_FALSE)

#define BROTLI_MAKE_UINT64_T(high, low) ((((uint64_t)(high)) << 32) | low)

#define BROTLI_UINT32_MAX (~((uint32_t)0))
#define BROTLI_SIZE_MAX (~((size_t)0))

/**
 * Allocating function pointer type.
 *
 * @param opaque custom memory manager handle provided by client
 * @param size requested memory region size; can not be @c 0
 * @returns @c 0 in the case of failure
 * @returns a valid pointer to a memory region of at least @p size bytes
 *          long otherwise
 */
typedef void* (*brotli_alloc_func)(void* opaque, size_t size);

/**
 * Deallocating function pointer type.
 *
 * This function @b SHOULD do nothing if @p address is @c 0.
 *
 * @param opaque custom memory manager handle provided by client
 * @param address memory region pointer returned by ::brotli_alloc_func, or @c 0
 */
typedef void (*brotli_free_func)(void* opaque, void* address);

#endif  /* BROTLI_COMMON_TYPES_H_ */

/* Copyright 2016 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Macros for compiler / platform specific API declarations. */

#ifndef BROTLI_COMMON_PORT_H_
#define BROTLI_COMMON_PORT_H_

/* The following macros were borrowed from https://github.com/nemequ/hedley
 * with permission of original author - Evan Nemerson <evan@nemerson.com> */

/* >>> >>> >>> hedley macros */

#define BROTLI_MAKE_VERSION(major, minor, revision) \
  (((major) * 1000000) + ((minor) * 1000) + (revision))

#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#define BROTLI_GNUC_VERSION \
  BROTLI_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__)
#define BROTLI_GNUC_VERSION BROTLI_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, 0)
#endif

#if defined(BROTLI_GNUC_VERSION)
#define BROTLI_GNUC_VERSION_CHECK(major, minor, patch) \
  (BROTLI_GNUC_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_GNUC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000)
#define BROTLI_MSVC_VERSION                                \
  BROTLI_MAKE_VERSION((_MSC_FULL_VER / 10000000),          \
                      (_MSC_FULL_VER % 10000000) / 100000, \
                      (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER)
#define BROTLI_MSVC_VERSION                              \
  BROTLI_MAKE_VERSION((_MSC_FULL_VER / 1000000),         \
                      (_MSC_FULL_VER % 1000000) / 10000, \
                      (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER)
#define BROTLI_MSVC_VERSION \
  BROTLI_MAKE_VERSION(_MSC_VER / 100, _MSC_VER % 100, 0)
#endif

#if !defined(_MSC_VER)
#define BROTLI_MSVC_VERSION_CHECK(major, minor, patch) (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#define BROTLI_MSVC_VERSION_CHECK(major, minor, patch) \
  (_MSC_FULL_VER >= ((major * 10000000) + (minor * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#define BROTLI_MSVC_VERSION_CHECK(major, minor, patch) \
  (_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
#define BROTLI_MSVC_VERSION_CHECK(major, minor, patch) \
  (_MSC_VER >= ((major * 100) + (minor)))
#endif

#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE)
#define BROTLI_INTEL_VERSION                   \
  BROTLI_MAKE_VERSION(__INTEL_COMPILER / 100,  \
                      __INTEL_COMPILER % 100,  \
                      __INTEL_COMPILER_UPDATE)
#elif defined(__INTEL_COMPILER)
#define BROTLI_INTEL_VERSION \
  BROTLI_MAKE_VERSION(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, 0)
#endif

#if defined(BROTLI_INTEL_VERSION)
#define BROTLI_INTEL_VERSION_CHECK(major, minor, patch) \
  (BROTLI_INTEL_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_INTEL_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__PGI) && \
    defined(__PGIC__) && defined(__PGIC_MINOR__) && defined(__PGIC_PATCHLEVEL__)
#define BROTLI_PGI_VERSION \
  BROTLI_MAKE_VERSION(__PGIC__, __PGIC_MINOR__, __PGIC_PATCHLEVEL__)
#endif

#if defined(BROTLI_PGI_VERSION)
#define BROTLI_PGI_VERSION_CHECK(major, minor, patch) \
  (BROTLI_PGI_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_PGI_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__SUNPRO_C) && (__SUNPRO_C > 0x1000)
#define BROTLI_SUNPRO_VERSION                                       \
  BROTLI_MAKE_VERSION(                                              \
    (((__SUNPRO_C >> 16) & 0xf) * 10) + ((__SUNPRO_C >> 12) & 0xf), \
    (((__SUNPRO_C >> 8) & 0xf) * 10) + ((__SUNPRO_C >> 4) & 0xf),   \
    (__SUNPRO_C & 0xf) * 10)
#elif defined(__SUNPRO_C)
#define BROTLI_SUNPRO_VERSION                  \
  BROTLI_MAKE_VERSION((__SUNPRO_C >> 8) & 0xf, \
                      (__SUNPRO_C >> 4) & 0xf, \
                      (__SUNPRO_C) & 0xf)
#elif defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x1000)
#define BROTLI_SUNPRO_VERSION                                         \
  BROTLI_MAKE_VERSION(                                                \
    (((__SUNPRO_CC >> 16) & 0xf) * 10) + ((__SUNPRO_CC >> 12) & 0xf), \
    (((__SUNPRO_CC >> 8) & 0xf) * 10) + ((__SUNPRO_CC >> 4) & 0xf),   \
    (__SUNPRO_CC & 0xf) * 10)
#elif defined(__SUNPRO_CC)
#define BROTLI_SUNPRO_VERSION                   \
  BROTLI_MAKE_VERSION((__SUNPRO_CC >> 8) & 0xf, \
                      (__SUNPRO_CC >> 4) & 0xf, \
                      (__SUNPRO_CC) & 0xf)
#endif

#if defined(BROTLI_SUNPRO_VERSION)
#define BROTLI_SUNPRO_VERSION_CHECK(major, minor, patch) \
  (BROTLI_SUNPRO_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_SUNPRO_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__CC_ARM) && defined(__ARMCOMPILER_VERSION)
#define BROTLI_ARM_VERSION                                       \
  BROTLI_MAKE_VERSION((__ARMCOMPILER_VERSION / 1000000),         \
                      (__ARMCOMPILER_VERSION % 1000000) / 10000, \
                      (__ARMCOMPILER_VERSION % 10000) / 100)
#elif defined(__CC_ARM) && defined(__ARMCC_VERSION)
#define BROTLI_ARM_VERSION                                 \
  BROTLI_MAKE_VERSION((__ARMCC_VERSION / 1000000),         \
                      (__ARMCC_VERSION % 1000000) / 10000, \
                      (__ARMCC_VERSION % 10000) / 100)
#endif

#if defined(BROTLI_ARM_VERSION)
#define BROTLI_ARM_VERSION_CHECK(major, minor, patch) \
  (BROTLI_ARM_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_ARM_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__ibmxl__)
#define BROTLI_IBM_VERSION                    \
  BROTLI_MAKE_VERSION(__ibmxl_version__,      \
                      __ibmxl_release__,      \
                      __ibmxl_modification__)
#elif defined(__xlC__) && defined(__xlC_ver__)
#define BROTLI_IBM_VERSION \
  BROTLI_MAKE_VERSION(__xlC__ >> 8, __xlC__ & 0xff, (__xlC_ver__ >> 8) & 0xff)
#elif defined(__xlC__)
#define BROTLI_IBM_VERSION BROTLI_MAKE_VERSION(__xlC__ >> 8, __xlC__ & 0xff, 0)
#endif

#if defined(BROTLI_IBM_VERSION)
#define BROTLI_IBM_VERSION_CHECK(major, minor, patch) \
  (BROTLI_IBM_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_IBM_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__TI_COMPILER_VERSION__)
#define BROTLI_TI_VERSION                                         \
  BROTLI_MAKE_VERSION((__TI_COMPILER_VERSION__ / 1000000),        \
                      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
                      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(BROTLI_TI_VERSION)
#define BROTLI_TI_VERSION_CHECK(major, minor, patch) \
  (BROTLI_TI_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_TI_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__IAR_SYSTEMS_ICC__)
#if __VER__ > 1000
#define BROTLI_IAR_VERSION                     \
  BROTLI_MAKE_VERSION((__VER__ / 1000000),     \
                      (__VER__ / 1000) % 1000, \
                      (__VER__ % 1000))
#else
#define BROTLI_IAR_VERSION BROTLI_MAKE_VERSION(VER / 100, __VER__ % 100, 0)
#endif
#endif

#if defined(BROTLI_IAR_VERSION)
#define BROTLI_IAR_VERSION_CHECK(major, minor, patch) \
  (BROTLI_IAR_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_IAR_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__TINYC__)
#define BROTLI_TINYC_VERSION \
  BROTLI_MAKE_VERSION(__TINYC__ / 1000, (__TINYC__ / 100) % 10, __TINYC__ % 100)
#endif

#if defined(BROTLI_TINYC_VERSION)
#define BROTLI_TINYC_VERSION_CHECK(major, minor, patch) \
  (BROTLI_TINYC_VERSION >= BROTLI_MAKE_VERSION(major, minor, patch))
#else
#define BROTLI_TINYC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__has_attribute)
#define BROTLI_GNUC_HAS_ATTRIBUTE(attribute, major, minor, patch) \
  __has_attribute(attribute)
#else
#define BROTLI_GNUC_HAS_ATTRIBUTE(attribute, major, minor, patch) \
  BROTLI_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(__has_builtin)
#define BROTLI_GNUC_HAS_BUILTIN(builtin, major, minor, patch) \
  __has_builtin(builtin)
#else
#define BROTLI_GNUC_HAS_BUILTIN(builtin, major, minor, patch) \
  BROTLI_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(__has_feature)
#define BROTLI_HAS_FEATURE(feature) __has_feature(feature)
#else
#define BROTLI_HAS_FEATURE(feature) (0)
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#define BROTLI_PUBLIC
#elif BROTLI_GNUC_VERSION_CHECK(3, 3, 0) ||                         \
    BROTLI_TI_VERSION_CHECK(8, 0, 0) ||                             \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0) ||                         \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) ||                            \
    BROTLI_IBM_VERSION_CHECK(13, 1, 0) ||                           \
    BROTLI_SUNPRO_VERSION_CHECK(5, 11, 0) ||                        \
    (BROTLI_TI_VERSION_CHECK(7, 3, 0) &&                            \
     defined(__TI_GNU_ATTRIBUTE_SUPPORT__) && defined(__TI_EABI__))
#define BROTLI_PUBLIC __attribute__ ((visibility ("default")))
#else
#define BROTLI_PUBLIC
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#define BROTLI_INTERNAL
#elif BROTLI_GNUC_VERSION_CHECK(3, 3, 0) ||                         \
    BROTLI_TI_VERSION_CHECK(8, 0, 0) ||                             \
    BROTLI_INTEL_VERSION_CHECK(16, 0, 0) ||                         \
    BROTLI_ARM_VERSION_CHECK(4, 1, 0) ||                            \
    BROTLI_IBM_VERSION_CHECK(13, 1, 0) ||                           \
    BROTLI_SUNPRO_VERSION_CHECK(5, 11, 0) ||                        \
    (BROTLI_TI_VERSION_CHECK(7, 3, 0) &&                            \
     defined(__TI_GNU_ATTRIBUTE_SUPPORT__) && defined(__TI_EABI__))
#define BROTLI_INTERNAL __attribute__ ((visibility ("hidden")))
#else
#define BROTLI_INTERNAL
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) &&   \
    !defined(__STDC_NO_VLA__) && !defined(__cplusplus) &&           \
    !defined(__PGI) && !defined(__PGIC__) && !defined(__TINYC__) && \
    !defined(__clang__)
#define BROTLI_ARRAY_PARAM(name) (name)
#else
#define BROTLI_ARRAY_PARAM(name)
#endif

/* <<< <<< <<< end of hedley macros. */

#if defined(BROTLI_SHARED_COMPILATION)
#if defined(_WIN32)
#if defined(BROTLICOMMON_SHARED_COMPILATION)
#define BROTLI_COMMON_API __declspec(dllexport)
#else  /* !BROTLICOMMON_SHARED_COMPILATION */
#define BROTLI_COMMON_API __declspec(dllimport)
#endif  /* BROTLICOMMON_SHARED_COMPILATION */
#if defined(BROTLIDEC_SHARED_COMPILATION)
#define BROTLI_DEC_API __declspec(dllexport)
#else   /* !BROTLIDEC_SHARED_COMPILATION */
#define BROTLI_DEC_API __declspec(dllimport)
#endif  /* BROTLIDEC_SHARED_COMPILATION */
#if defined(BROTLIENC_SHARED_COMPILATION)
#define BROTLI_ENC_API __declspec(dllexport)
#else  /* !BROTLIENC_SHARED_COMPILATION */
#define BROTLI_ENC_API __declspec(dllimport)
#endif  /* BROTLIENC_SHARED_COMPILATION */
#else  /* !_WIN32 */
#define BROTLI_COMMON_API BROTLI_PUBLIC
#define BROTLI_DEC_API BROTLI_PUBLIC
#define BROTLI_ENC_API BROTLI_PUBLIC
#endif  /* _WIN32 */
#else  /* BROTLI_SHARED_COMPILATION */
#define BROTLI_COMMON_API
#define BROTLI_DEC_API
#define BROTLI_ENC_API
#endif

#if defined(BROTLI_BUILD_ENC_EXTRA_API)
#define BROTLI_ENC_EXTRA_API BROTLI_ENC_API
#else
#define BROTLI_ENC_EXTRA_API BROTLI_INTERNAL
#endif

#endif  /* BROTLI_COMMON_PORT_H_ */

/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* (Opaque) Shared Dictionary definition and utilities. */

#ifndef BROTLI_COMMON_SHARED_DICTIONARY_H_
#define BROTLI_COMMON_SHARED_DICTIONARY_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define SHARED_BROTLI_MIN_DICTIONARY_WORD_LENGTH 4
#define SHARED_BROTLI_MAX_DICTIONARY_WORD_LENGTH 31
#define SHARED_BROTLI_NUM_DICTIONARY_CONTEXTS 64
#define SHARED_BROTLI_MAX_COMPOUND_DICTS 15

/**
 * Practical limit for RAW dictionary size.
 *
 * Maximal distance (for non large window mode) is `(2 << 26 + 11) << NPOSTFIX`.
 * Usually NPOSTFIX is 0 for regular data (i.e. not WORD/DWORD/QWORD stream).
 * In large window mode, distance could be much larger.
 *
 * Currently, there are no known use cases for larger than 64MiB dictionary in
 * regular mode and 2GiB in large window mode.
 */
#define SHARED_BROTLI_MAX_RAW_DICT_SIZE (1u << (23u + sizeof(size_t)))

/**
 * Opaque structure that holds shared dictionary data.
 *
 * Allocated and initialized with ::BrotliSharedDictionaryCreateInstance.
 * Cleaned up and deallocated with ::BrotliSharedDictionaryDestroyInstance.
 */
typedef struct BrotliSharedDictionaryStruct BrotliSharedDictionary;

/**
 * Input data type for ::BrotliSharedDictionaryAttach.
 */
typedef enum BrotliSharedDictionaryType {
  /** Raw LZ77 prefix dictionary. */
  BROTLI_SHARED_DICTIONARY_RAW = 0,
  /** Serialized shared dictionary.
   *
   * DO NOT USE: methods accepting this value will fail.
   */
  BROTLI_SHARED_DICTIONARY_SERIALIZED = 1
} BrotliSharedDictionaryType;

/**
 * Creates an instance of ::BrotliSharedDictionary.
 *
 * Fresh instance has default word dictionary and transforms
 * and no LZ77 prefix dictionary.
 *
 * @p alloc_func and @p free_func @b MUST be both zero or both non-zero. In the
 * case they are both zero, default memory allocators are used. @p opaque is
 * passed to @p alloc_func and @p free_func when they are called. @p free_func
 * has to return without doing anything when asked to free a NULL pointer.
 *
 * @param alloc_func custom memory allocation function
 * @param free_func custom memory free function
 * @param opaque custom memory manager handle
 * @returns @c 0 if instance can not be allocated or initialized
 * @returns pointer to initialized ::BrotliSharedDictionary otherwise
 */
BROTLI_COMMON_API BrotliSharedDictionary* BrotliSharedDictionaryCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

/**
 * Deinitializes and frees ::BrotliSharedDictionary instance.
 *
 * @param dict shared dictionary instance to be cleaned up and deallocated
 */
BROTLI_COMMON_API void BrotliSharedDictionaryDestroyInstance(
    BrotliSharedDictionary* dict);

/**
 * Attaches dictionary to a given instance of ::BrotliSharedDictionary.
 *
 * Dictionary to be attached is represented in a serialized format as a region
 * of memory.
 *
 * Provided data it partially referenced by a resulting (compound) dictionary,
 * and should be kept untouched, while at least one compound dictionary uses it.
 * This way memory overhead is kept minimal by the cost of additional resource
 * management.
 *
 * @param dict dictionary to extend
 * @param type type of dictionary to attach
 * @param data_size size of @p data
 * @param data serialized dictionary of type @p type, with at least @p data_size
 *        addressable bytes
 * @returns ::BROTLI_TRUE if provided dictionary is successfully attached
 * @returns ::BROTLI_FALSE otherwise
 */
BROTLI_COMMON_API BROTLI_BOOL BrotliSharedDictionaryAttach(
    BrotliSharedDictionary* dict, BrotliSharedDictionaryType type,
    size_t data_size, const uint8_t data[BROTLI_ARRAY_PARAM(data_size)]);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_COMMON_SHARED_DICTIONARY_H_ */

/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/**
 * @file
 * API for Brotli decompression.
 */

#ifndef BROTLI_DEC_DECODE_H_
#define BROTLI_DEC_DECODE_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/**
 * Opaque structure that holds decoder state.
 *
 * Allocated and initialized with ::BrotliDecoderCreateInstance.
 * Cleaned up and deallocated with ::BrotliDecoderDestroyInstance.
 */
typedef struct BrotliDecoderStateStruct BrotliDecoderState;

/**
 * Result type for ::BrotliDecoderDecompress and
 * ::BrotliDecoderDecompressStream functions.
 */
typedef enum {
  /** Decoding error, e.g. corrupted input or memory allocation problem. */
  BROTLI_DECODER_RESULT_ERROR = 0,
  /** Decoding successfully completed. */
  BROTLI_DECODER_RESULT_SUCCESS = 1,
  /** Partially done; should be called again with more input. */
  BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT = 2,
  /** Partially done; should be called again with more output. */
  BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT = 3
} BrotliDecoderResult;

/**
 * Template that evaluates items of ::BrotliDecoderErrorCode.
 *
 * Example: @code {.cpp}
 * // Log Brotli error code.
 * switch (brotliDecoderErrorCode) {
 * #define CASE_(PREFIX, NAME, CODE) \
 *   case BROTLI_DECODER ## PREFIX ## NAME: \
 *     LOG(INFO) << "error code:" << #NAME; \
 *     break;
 * #define NEWLINE_
 * BROTLI_DECODER_ERROR_CODES_LIST(CASE_, NEWLINE_)
 * #undef CASE_
 * #undef NEWLINE_
 *   default: LOG(FATAL) << "unknown brotli error code";
 * }
 * @endcode
 */
#define BROTLI_DECODER_ERROR_CODES_LIST(BROTLI_ERROR_CODE, SEPARATOR)      \
  BROTLI_ERROR_CODE(_, NO_ERROR, 0) SEPARATOR                              \
  /* Same as BrotliDecoderResult values */                                 \
  BROTLI_ERROR_CODE(_, SUCCESS, 1) SEPARATOR                               \
  BROTLI_ERROR_CODE(_, NEEDS_MORE_INPUT, 2) SEPARATOR                      \
  BROTLI_ERROR_CODE(_, NEEDS_MORE_OUTPUT, 3) SEPARATOR                     \
                                                                           \
  /* Errors caused by invalid input */                                     \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, EXUBERANT_NIBBLE, -1) SEPARATOR        \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, RESERVED, -2) SEPARATOR                \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, EXUBERANT_META_NIBBLE, -3) SEPARATOR   \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, SIMPLE_HUFFMAN_ALPHABET, -4) SEPARATOR \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, SIMPLE_HUFFMAN_SAME, -5) SEPARATOR     \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, CL_SPACE, -6) SEPARATOR                \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, HUFFMAN_SPACE, -7) SEPARATOR           \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, CONTEXT_MAP_REPEAT, -8) SEPARATOR      \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, BLOCK_LENGTH_1, -9) SEPARATOR          \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, BLOCK_LENGTH_2, -10) SEPARATOR         \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, TRANSFORM, -11) SEPARATOR              \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, DICTIONARY, -12) SEPARATOR             \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, WINDOW_BITS, -13) SEPARATOR            \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, PADDING_1, -14) SEPARATOR              \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, PADDING_2, -15) SEPARATOR              \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, DISTANCE, -16) SEPARATOR               \
  BROTLI_ERROR_CODE(_ERROR_FORMAT_, BLOCK_SWITCH, -17) SEPARATOR           \
  BROTLI_ERROR_CODE(_ERROR_, COMPOUND_DICTIONARY, -18) SEPARATOR           \
  BROTLI_ERROR_CODE(_ERROR_, DICTIONARY_NOT_SET, -19) SEPARATOR            \
  BROTLI_ERROR_CODE(_ERROR_, INVALID_ARGUMENTS, -20) SEPARATOR             \
                                                                           \
  /* Memory allocation problems */                                         \
  BROTLI_ERROR_CODE(_ERROR_ALLOC_, CONTEXT_MODES, -21) SEPARATOR           \
  /* Literal, insert and distance trees together */                        \
  BROTLI_ERROR_CODE(_ERROR_ALLOC_, TREE_GROUPS, -22) SEPARATOR             \
  /* -23..-24 codes are reserved for distinct tree groups */               \
  BROTLI_ERROR_CODE(_ERROR_ALLOC_, CONTEXT_MAP, -25) SEPARATOR             \
  BROTLI_ERROR_CODE(_ERROR_ALLOC_, RING_BUFFER_1, -26) SEPARATOR           \
  BROTLI_ERROR_CODE(_ERROR_ALLOC_, RING_BUFFER_2, -27) SEPARATOR           \
  /* -28..-29 codes are reserved for dynamic ring-buffer allocation */     \
  BROTLI_ERROR_CODE(_ERROR_ALLOC_, BLOCK_TYPE_TREES, -30) SEPARATOR        \
                                                                           \
  /* "Impossible" states */                                                \
  BROTLI_ERROR_CODE(_ERROR_, UNREACHABLE, -31)

/**
 * Error code for detailed logging / production debugging.
 *
 * See ::BrotliDecoderGetErrorCode and ::BROTLI_LAST_ERROR_CODE.
 */
typedef enum {
#define BROTLI_COMMA_ ,
#define BROTLI_ERROR_CODE_ENUM_ITEM_(PREFIX, NAME, CODE) \
    BROTLI_DECODER ## PREFIX ## NAME = CODE
  BROTLI_DECODER_ERROR_CODES_LIST(BROTLI_ERROR_CODE_ENUM_ITEM_, BROTLI_COMMA_)
} BrotliDecoderErrorCode;
#undef BROTLI_ERROR_CODE_ENUM_ITEM_
#undef BROTLI_COMMA_

/**
 * The value of the last error code, negative integer.
 *
 * All other error code values are in the range from ::BROTLI_LAST_ERROR_CODE
 * to @c -1. There are also 4 other possible non-error codes @c 0 .. @c 3 in
 * ::BrotliDecoderErrorCode enumeration.
 */
#define BROTLI_LAST_ERROR_CODE BROTLI_DECODER_ERROR_UNREACHABLE

/** Options to be used with ::BrotliDecoderSetParameter. */
typedef enum BrotliDecoderParameter {
  /**
   * Disable "canny" ring buffer allocation strategy.
   *
   * Ring buffer is allocated according to window size, despite the real size of
   * the content.
   */
  BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION = 0,
  /**
   * Flag that determines if "Large Window Brotli" is used.
   */
  BROTLI_DECODER_PARAM_LARGE_WINDOW = 1
} BrotliDecoderParameter;

/**
 * Sets the specified parameter to the given decoder instance.
 *
 * @param state decoder instance
 * @param param parameter to set
 * @param value new parameter value
 * @returns ::BROTLI_FALSE if parameter is unrecognized, or value is invalid
 * @returns ::BROTLI_TRUE if value is accepted
 */
BROTLI_DEC_API BROTLI_BOOL BrotliDecoderSetParameter(
    BrotliDecoderState* state, BrotliDecoderParameter param, uint32_t value);

/**
 * Adds LZ77 prefix dictionary, adds or replaces built-in static dictionary and
 * transforms.
 *
 * Attached dictionary ownership is not transferred.
 * Data provided to this method should be kept accessible until
 * decoding is finished and decoder instance is destroyed.
 *
 * @note Dictionaries can NOT be attached after actual decoding is started.
 *
 * @param state decoder instance
 * @param type dictionary data format
 * @param data_size length of memory region pointed by @p data
 * @param data dictionary data in format corresponding to @p type
 * @returns ::BROTLI_FALSE if dictionary is corrupted,
 *          or dictionary count limit is reached
 * @returns ::BROTLI_TRUE if dictionary is accepted / attached
 */
BROTLI_DEC_API BROTLI_BOOL BrotliDecoderAttachDictionary(
    BrotliDecoderState* state, BrotliSharedDictionaryType type,
    size_t data_size, const uint8_t data[BROTLI_ARRAY_PARAM(data_size)]);

/**
 * Creates an instance of ::BrotliDecoderState and initializes it.
 *
 * The instance can be used once for decoding and should then be destroyed with
 * ::BrotliDecoderDestroyInstance, it cannot be reused for a new decoding
 * session.
 *
 * @p alloc_func and @p free_func @b MUST be both zero or both non-zero. In the
 * case they are both zero, default memory allocators are used. @p opaque is
 * passed to @p alloc_func and @p free_func when they are called. @p free_func
 * has to return without doing anything when asked to free a NULL pointer.
 *
 * @param alloc_func custom memory allocation function
 * @param free_func custom memory free function
 * @param opaque custom memory manager handle
 * @returns @c 0 if instance can not be allocated or initialized
 * @returns pointer to initialized ::BrotliDecoderState otherwise
 */
BROTLI_DEC_API BrotliDecoderState* BrotliDecoderCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

/**
 * Deinitializes and frees ::BrotliDecoderState instance.
 *
 * @param state decoder instance to be cleaned up and deallocated
 */
BROTLI_DEC_API void BrotliDecoderDestroyInstance(BrotliDecoderState* state);

/**
 * Performs one-shot memory-to-memory decompression.
 *
 * Decompresses the data in @p encoded_buffer into @p decoded_buffer, and sets
 * @p *decoded_size to the decompressed length.
 *
 * @param encoded_size size of @p encoded_buffer
 * @param encoded_buffer compressed data buffer with at least @p encoded_size
 *        addressable bytes
 * @param[in, out] decoded_size @b in: size of @p decoded_buffer; \n
 *                 @b out: length of decompressed data written to
 *                 @p decoded_buffer
 * @param decoded_buffer decompressed data destination buffer
 * @returns ::BROTLI_DECODER_RESULT_ERROR if input is corrupted, memory
 *          allocation failed, or @p decoded_buffer is not large enough;
 * @returns ::BROTLI_DECODER_RESULT_SUCCESS otherwise
 */
BROTLI_DEC_API BrotliDecoderResult BrotliDecoderDecompress(
    size_t encoded_size,
    const uint8_t encoded_buffer[BROTLI_ARRAY_PARAM(encoded_size)],
    size_t* decoded_size,
    uint8_t decoded_buffer[BROTLI_ARRAY_PARAM(*decoded_size)]);

/**
 * Decompresses the input stream to the output stream.
 *
 * The values @p *available_in and @p *available_out must specify the number of
 * bytes addressable at @p *next_in and @p *next_out respectively.
 * When @p *available_out is @c 0, @p next_out is allowed to be @c NULL.
 *
 * After each call, @p *available_in will be decremented by the amount of input
 * bytes consumed, and the @p *next_in pointer will be incremented by that
 * amount. Similarly, @p *available_out will be decremented by the amount of
 * output bytes written, and the @p *next_out pointer will be incremented by
 * that amount.
 *
 * @p total_out, if it is not a null-pointer, will be set to the number
 * of bytes decompressed since the last @p state initialization.
 *
 * @note Input is never overconsumed, so @p next_in and @p available_in could be
 * passed to the next consumer after decoding is complete.
 *
 * @param state decoder instance
 * @param[in, out] available_in @b in: amount of available input; \n
 *                 @b out: amount of unused input
 * @param[in, out] next_in pointer to the next compressed byte
 * @param[in, out] available_out @b in: length of output buffer; \n
 *                 @b out: remaining size of output buffer
 * @param[in, out] next_out output buffer cursor;
 *                 can be @c NULL if @p available_out is @c 0
 * @param[out] total_out number of bytes decompressed so far; can be @c NULL
 * @returns ::BROTLI_DECODER_RESULT_ERROR if input is corrupted, memory
 *          allocation failed, arguments were invalid, etc.;
 *          use ::BrotliDecoderGetErrorCode to get detailed error code
 * @returns ::BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT decoding is blocked until
 *          more input data is provided
 * @returns ::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT decoding is blocked until
 *          more output space is provided
 * @returns ::BROTLI_DECODER_RESULT_SUCCESS decoding is finished, no more
 *          input might be consumed and no more output will be produced
 */
BROTLI_DEC_API BrotliDecoderResult BrotliDecoderDecompressStream(
  BrotliDecoderState* state, size_t* available_in, const uint8_t** next_in,
  size_t* available_out, uint8_t** next_out, size_t* total_out);

/**
 * Checks if decoder has more output.
 *
 * @param state decoder instance
 * @returns ::BROTLI_TRUE, if decoder has some unconsumed output
 * @returns ::BROTLI_FALSE otherwise
 */
BROTLI_DEC_API BROTLI_BOOL BrotliDecoderHasMoreOutput(
    const BrotliDecoderState* state);

/**
 * Acquires pointer to internal output buffer.
 *
 * This method is used to make language bindings easier and more efficient:
 *  -# push data to ::BrotliDecoderDecompressStream,
 *     until ::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT is reported
 *  -# use ::BrotliDecoderTakeOutput to peek bytes and copy to language-specific
 *     entity
 *
 * Also this could be useful if there is an output stream that is able to
 * consume all the provided data (e.g. when data is saved to file system).
 *
 * @attention After every call to ::BrotliDecoderTakeOutput @p *size bytes of
 *            output are considered consumed for all consecutive calls to the
 *            instance methods; returned pointer becomes invalidated as well.
 *
 * @note Decoder output is not guaranteed to be contiguous. This means that
 *       after the size-unrestricted call to ::BrotliDecoderTakeOutput,
 *       immediate next call to ::BrotliDecoderTakeOutput may return more data.
 *
 * @param state decoder instance
 * @param[in, out] size @b in: number of bytes caller is ready to take, @c 0 if
 *                 any amount could be handled; \n
 *                 @b out: amount of data pointed by returned pointer and
 *                 considered consumed; \n
 *                 out value is never greater than in value, unless it is @c 0
 * @returns pointer to output data
 */
BROTLI_DEC_API const uint8_t* BrotliDecoderTakeOutput(
    BrotliDecoderState* state, size_t* size);

/**
 * Checks if instance has already consumed input.
 *
 * Instance that returns ::BROTLI_FALSE is considered "fresh" and could be
 * reused.
 *
 * @param state decoder instance
 * @returns ::BROTLI_TRUE if decoder has already used some input bytes
 * @returns ::BROTLI_FALSE otherwise
 */
BROTLI_DEC_API BROTLI_BOOL BrotliDecoderIsUsed(const BrotliDecoderState* state);

/**
 * Checks if decoder instance reached the final state.
 *
 * @param state decoder instance
 * @returns ::BROTLI_TRUE if decoder is in a state where it reached the end of
 *          the input and produced all of the output
 * @returns ::BROTLI_FALSE otherwise
 */
BROTLI_DEC_API BROTLI_BOOL BrotliDecoderIsFinished(
    const BrotliDecoderState* state);

/**
 * Acquires a detailed error code.
 *
 * Should be used only after ::BrotliDecoderDecompressStream returns
 * ::BROTLI_DECODER_RESULT_ERROR.
 *
 * See also ::BrotliDecoderErrorString
 *
 * @param state decoder instance
 * @returns last saved error code
 */
BROTLI_DEC_API BrotliDecoderErrorCode BrotliDecoderGetErrorCode(
    const BrotliDecoderState* state);

/**
 * Converts error code to a c-string.
 */
BROTLI_DEC_API const char* BrotliDecoderErrorString(BrotliDecoderErrorCode c);

/**
 * Gets a decoder library version.
 *
 * Look at BROTLI_MAKE_HEX_VERSION for more information.
 */
BROTLI_DEC_API uint32_t BrotliDecoderVersion(void);

/**
 * Callback to fire on metadata block start.
 *
 * After this callback is fired, if @p size is not @c 0, it is followed by
 * ::brotli_decoder_metadata_chunk_func as more metadata block contents become
 * accessible.
 *
 * @param opaque callback handle
 * @param size size of metadata block
 */
typedef void (*brotli_decoder_metadata_start_func)(void* opaque, size_t size);

/**
 * Callback to fire on metadata block chunk becomes available.
 *
 * This function can be invoked multiple times per metadata block; block should
 * be considered finished when sum of @p size matches the announced metadata
 * block size. Chunks contents pointed by @p data are transient and shouln not
 * be accessed after leaving the callback.
 *
 * @param opaque callback handle
 * @param data pointer to metadata contents
 * @param size size of metadata block chunk, at least @c 1
 */
typedef void (*brotli_decoder_metadata_chunk_func)(void* opaque,
                                                   const uint8_t* data,
                                                   size_t size);

/**
 * Sets callback for receiving metadata blocks.
 *
 * @param state decoder instance
 * @param start_func callback on metadata block start
 * @param chunk_func callback on metadata block chunk
 * @param opaque callback handle
 */
BROTLI_DEC_API void BrotliDecoderSetMetadataCallbacks(
    BrotliDecoderState* state,
    brotli_decoder_metadata_start_func start_func,
    brotli_decoder_metadata_chunk_func chunk_func, void* opaque);

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

#endif  /* BROTLI_DEC_DECODE_H_ */

/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/**
 * @file
 * API for Brotli compression.
 */

#ifndef BROTLI_ENC_ENCODE_H_
#define BROTLI_ENC_ENCODE_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/** Minimal value for ::BROTLI_PARAM_LGWIN parameter. */
#define BROTLI_MIN_WINDOW_BITS 10
/**
 * Maximal value for ::BROTLI_PARAM_LGWIN parameter.
 *
 * @note equal to @c BROTLI_MAX_DISTANCE_BITS constant.
 */
#define BROTLI_MAX_WINDOW_BITS 24
/**
 * Maximal value for ::BROTLI_PARAM_LGWIN parameter
 * in "Large Window Brotli" (32-bit).
 */
#define BROTLI_LARGE_MAX_WINDOW_BITS 30
/** Minimal value for ::BROTLI_PARAM_LGBLOCK parameter. */
#define BROTLI_MIN_INPUT_BLOCK_BITS 16
/** Maximal value for ::BROTLI_PARAM_LGBLOCK parameter. */
#define BROTLI_MAX_INPUT_BLOCK_BITS 24
/** Minimal value for ::BROTLI_PARAM_QUALITY parameter. */
#define BROTLI_MIN_QUALITY 0
/** Maximal value for ::BROTLI_PARAM_QUALITY parameter. */
#define BROTLI_MAX_QUALITY 11

/** Options for ::BROTLI_PARAM_MODE parameter. */
typedef enum BrotliEncoderMode {
  /**
   * Default compression mode.
   *
   * In this mode compressor does not know anything in advance about the
   * properties of the input.
   */
  BROTLI_MODE_GENERIC = 0,
  /** Compression mode for UTF-8 formatted text input. */
  BROTLI_MODE_TEXT = 1,
  /** Compression mode used in WOFF 2.0. */
  BROTLI_MODE_FONT = 2
} BrotliEncoderMode;

/** Options for ::BROTLI_PARAM_BASE64_MODE parameter. */
typedef enum BrotliEncoderBase64Mode {
  /** Base64 optimization is disabled. */
  BROTLI_BASE64_MODE_DISABLED = 0,
  /** Automatic detection of Base64 zones and direct jump (skipping dictionary
      and LZ77 lookups). */
  BROTLI_BASE64_MODE_DETECTION = 1
} BrotliEncoderBase64Mode;

#define BROTLI_DEFAULT_BASE64_MODE BROTLI_BASE64_MODE_DISABLED

#define BROTLI_DEFAULT_MAX_BASE64_REGIONS 16

#define BROTLI_DEFAULT_MIN_BASE64_REGION_LEN 2048

/** Options for ::BROTLI_PARAM_SIMD_HASHER parameter. */
typedef enum BrotliEncoderSimdHasher {
  /** Use SIMD hasher when recommended for the quality level. */
  BROTLI_SIMD_HASHER_DEFAULT = 0,
  /** Use SIMD hasher when supported up to quality 7. */
  BROTLI_SIMD_HASHER_ENABLE = 1,
  /** Never use SIMD hasher. */
  BROTLI_SIMD_HASHER_DISABLE = 2
} BrotliEncoderSimdHasher;

#define BROTLI_DEFAULT_SIMD_HASHER BROTLI_SIMD_HASHER_DEFAULT

/** Default value for ::BROTLI_PARAM_QUALITY parameter. */
#define BROTLI_DEFAULT_QUALITY 11
/** Default value for ::BROTLI_PARAM_LGWIN parameter. */
#define BROTLI_DEFAULT_WINDOW 22
/** Default value for ::BROTLI_PARAM_MODE parameter. */
#define BROTLI_DEFAULT_MODE BROTLI_MODE_GENERIC

/** Operations that can be performed by streaming encoder. */
typedef enum BrotliEncoderOperation {
  /**
   * Process input.
   *
   * Encoder may postpone producing output, until it has processed enough input.
   */
  BROTLI_OPERATION_PROCESS = 0,
  /**
   * Produce output for all processed input.
   *
   * Actual flush is performed when input stream is depleted and there is enough
   * space in output stream. This means that client should repeat
   * ::BROTLI_OPERATION_FLUSH operation until @p available_in becomes @c 0, and
   * ::BrotliEncoderHasMoreOutput returns ::BROTLI_FALSE. If output is acquired
   * via ::BrotliEncoderTakeOutput, then operation should be repeated after
   * output buffer is drained.
   *
   * @warning Until flush is complete, client @b SHOULD @b NOT swap,
   *          reduce or extend input stream.
   *
   * When flush is complete, output data will be sufficient for decoder to
   * reproduce all the given input.
   */
  BROTLI_OPERATION_FLUSH = 1,
  /**
   * Finalize the stream.
   *
   * Actual finalization is performed when input stream is depleted and there is
   * enough space in output stream. This means that client should repeat
   * ::BROTLI_OPERATION_FINISH operation until @p available_in becomes @c 0, and
   * ::BrotliEncoderHasMoreOutput returns ::BROTLI_FALSE. If output is acquired
   * via ::BrotliEncoderTakeOutput, then operation should be repeated after
   * output buffer is drained.
   *
   * @warning Until finalization is complete, client @b SHOULD @b NOT swap,
   *          reduce or extend input stream.
   *
   * Helper function ::BrotliEncoderIsFinished checks if stream is finalized and
   * output fully dumped.
   *
   * Adding more input data to finalized stream is impossible.
   */
  BROTLI_OPERATION_FINISH = 2,
  /**
   * Emit metadata block to stream.
   *
   * Metadata is opaque to Brotli: neither encoder, nor decoder processes this
   * data or relies on it. It may be used to pass some extra information from
   * encoder client to decoder client without interfering with main data stream.
   *
   * @note Encoder may emit empty metadata blocks internally, to pad encoded
   *       stream to byte boundary.
   *
   * @warning Until emitting metadata is complete client @b SHOULD @b NOT swap,
   *          reduce or extend input stream.
   *
   * @warning The whole content of input buffer is considered to be the content
   *          of metadata block. Do @b NOT @e append metadata to input stream,
   *          before it is depleted with other operations.
   *
   * Stream is soft-flushed before metadata block is emitted. Metadata block
   * @b MUST be no longer than than 16MiB.
   */
  BROTLI_OPERATION_EMIT_METADATA = 3
} BrotliEncoderOperation;

/** Options to be used with ::BrotliEncoderSetParameter. */
typedef enum BrotliEncoderParameter {
  /**
   * Tune encoder for specific input.
   *
   * ::BrotliEncoderMode enumerates all available values.
   */
  BROTLI_PARAM_MODE = 0,
  /**
   * The main compression speed-density lever.
   *
   * The higher the quality, the slower the compression. Range is
   * from ::BROTLI_MIN_QUALITY to ::BROTLI_MAX_QUALITY.
   */
  BROTLI_PARAM_QUALITY = 1,
  /**
   * Recommended sliding LZ77 window size.
   *
   * Encoder may reduce this value, e.g. if input is much smaller than
   * window size.
   *
   * Window size is `(1 << value) - 16`.
   *
   * Range is from ::BROTLI_MIN_WINDOW_BITS to ::BROTLI_MAX_WINDOW_BITS.
   */
  BROTLI_PARAM_LGWIN = 2,
  /**
   * Recommended input block size.
   *
   * Encoder may reduce this value, e.g. if input is much smaller than input
   * block size.
   *
   * Range is from ::BROTLI_MIN_INPUT_BLOCK_BITS to
   * ::BROTLI_MAX_INPUT_BLOCK_BITS.
   *
   * @note Bigger input block size allows better compression, but consumes more
   *       memory. \n The rough formula of memory used for temporary input
   *       storage is `3 << lgBlock`.
   */
  BROTLI_PARAM_LGBLOCK = 3,
  /**
   * Flag that affects usage of "literal context modeling" format feature.
   *
   * This flag is a "decoding-speed vs compression ratio" trade-off.
   */
  BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING = 4,
  /**
   * Estimated total input size for all ::BrotliEncoderCompressStream calls.
   *
   * The default value is 0, which means that the total input size is unknown.
   */
  BROTLI_PARAM_SIZE_HINT = 5,
  /**
   * Flag that determines if "Large Window Brotli" is used.
   */
  BROTLI_PARAM_LARGE_WINDOW = 6,
  /**
   * Recommended number of postfix bits (NPOSTFIX).
   *
   * Encoder may change this value.
   *
   * Range is from 0 to ::BROTLI_MAX_NPOSTFIX.
   */
  BROTLI_PARAM_NPOSTFIX = 7,
  /**
   * Recommended number of direct distance codes (NDIRECT).
   *
   * Encoder may change this value.
   *
   * Range is from 0 to (15 << NPOSTFIX) in steps of (1 << NPOSTFIX).
   */
  BROTLI_PARAM_NDIRECT = 8,
  /**
   * Number of bytes of input stream already processed by a different instance.
   *
   * @note It is important to configure all the encoder instances with same
   *       parameters (except this one) in order to allow all the encoded parts
   *       obey the same restrictions implied by header.
   *
   * If offset is not 0, then stream header is omitted.
   * In any case output start is byte aligned, so for proper streams stitching
   * "predecessor" stream must be flushed.
   *
   * Range is not artificially limited, but all the values greater or equal to
   * maximal window size have the same effect. Values greater than 2**30 are not
   * allowed.
   */
  BROTLI_PARAM_STREAM_OFFSET = 9,
  /**
   * Base64 encoding mode. Controls how the encoder handles Base64 content.
   * Currently supports 0 (disabled) and 1 (automatic detection and skip
   * dictionary lookups).
   */
  BROTLI_PARAM_BASE64_MODE = 10,
  /**
   * Maximum number of Base64 regions to detect.
   * Default is 16.
   */
  BROTLI_PARAM_MAX_BASE64_REGIONS = 11,
  /**
   * SIMD hasher usage mode.
   *
   * Controls whether the encoder uses SIMD hashers.
   * See ::BrotliEncoderSimdHasher for options.
   */
  BROTLI_PARAM_SIMD_HASHER = 12,
  /**
   * Engage optimized hasher.
   *
   * When enabled (1), engages H59 instead of H58.
   */
  BROTLI_PARAM_HASHER_OPT = 13,
  /**
   * Minimum length of a Base64 region to trigger detection and literal block
   * splitting. Below this threshold, Base64 regions are encoded using standard
   * LZ77 and Huffman coding.
   * Default is 2048.
   */
  BROTLI_PARAM_MIN_BASE64_REGION_LEN = 14
} BrotliEncoderParameter;

/**
 * Opaque structure that holds encoder state.
 *
 * Allocated and initialized with ::BrotliEncoderCreateInstance.
 * Cleaned up and deallocated with ::BrotliEncoderDestroyInstance.
 */
typedef struct BrotliEncoderStateStruct BrotliEncoderState;

/**
 * Sets the specified parameter to the given encoder instance.
 *
 * @param state encoder instance
 * @param param parameter to set
 * @param value new parameter value
 * @returns ::BROTLI_FALSE if parameter is unrecognized, or value is invalid
 * @returns ::BROTLI_FALSE if value of parameter can not be changed at current
 *          encoder state (e.g. when encoding is started, window size might be
 *          already encoded and therefore it is impossible to change it)
 * @returns ::BROTLI_TRUE if value is accepted
 * @warning invalid values might be accepted in case they would not break
 *          encoding process.
 */
BROTLI_ENC_API BROTLI_BOOL BrotliEncoderSetParameter(
    BrotliEncoderState* state, BrotliEncoderParameter param, uint32_t value);

/**
 * Creates an instance of ::BrotliEncoderState and initializes it.
 *
 * @p alloc_func and @p free_func @b MUST be both zero or both non-zero. In the
 * case they are both zero, default memory allocators are used. @p opaque is
 * passed to @p alloc_func and @p free_func when they are called. @p free_func
 * has to return without doing anything when asked to free a NULL pointer.
 *
 * @param alloc_func custom memory allocation function
 * @param free_func custom memory free function
 * @param opaque custom memory manager handle
 * @returns @c 0 if instance can not be allocated or initialized
 * @returns pointer to initialized ::BrotliEncoderState otherwise
 */
BROTLI_ENC_API BrotliEncoderState* BrotliEncoderCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

/**
 * Deinitializes and frees ::BrotliEncoderState instance.
 *
 * @param state decoder instance to be cleaned up and deallocated
 */
BROTLI_ENC_API void BrotliEncoderDestroyInstance(BrotliEncoderState* state);

/* Opaque type for pointer to different possible internal structures containing
   dictionary prepared for the encoder */
typedef struct BrotliEncoderPreparedDictionaryStruct
    BrotliEncoderPreparedDictionary;

/**
 * Prepares a shared dictionary from the given file format for the encoder.
 *
 * @p alloc_func and @p free_func @b MUST be both zero or both non-zero. In the
 * case they are both zero, default memory allocators are used. @p opaque is
 * passed to @p alloc_func and @p free_func when they are called. @p free_func
 * has to return without doing anything when asked to free a NULL pointer.
 *
 * @warning Created instance is "lean"; it does not contain copy of @p data,
 *          rather it contains only pointer to it; therefore,
 *          @p data @b MUST outlive the created instance.
 *
 * @param type type of dictionary stored in data
 * @param data_size size of @p data buffer
 * @param data pointer to the dictionary data
 * @param quality the maximum Brotli quality to prepare the dictionary for,
 *        use BROTLI_MAX_QUALITY by default
 * @param alloc_func custom memory allocation function
 * @param free_func custom memory free function
 * @param opaque custom memory manager handle
 */
BROTLI_ENC_API BrotliEncoderPreparedDictionary*
BrotliEncoderPrepareDictionary(BrotliSharedDictionaryType type,
    size_t data_size, const uint8_t data[BROTLI_ARRAY_PARAM(data_size)],
    int quality,
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

BROTLI_ENC_API void BrotliEncoderDestroyPreparedDictionary(
    BrotliEncoderPreparedDictionary* dictionary);

/**
 * Attaches a prepared dictionary of any type to the encoder. Can be used
 * multiple times to attach multiple dictionaries. The dictionary type was
 * determined by BrotliEncoderPrepareDictionary. Multiple raw prefix
 * dictionaries and/or max 1 serialized dictionary with custom words can be
 * attached.
 *
 * @returns ::BROTLI_FALSE in case of error
 * @returns ::BROTLI_TRUE otherwise
 */
BROTLI_ENC_API BROTLI_BOOL BrotliEncoderAttachPreparedDictionary(
    BrotliEncoderState* state,
    const BrotliEncoderPreparedDictionary* dictionary);

/**
 * Calculates the output size bound for the given @p input_size.
 *
 * @warning Result is only valid if quality is at least @c 2 and, in
 *          case ::BrotliEncoderCompressStream was used, no flushes
 *          (::BROTLI_OPERATION_FLUSH) were performed.
 *
 * @param input_size size of projected input
 * @returns @c 0 if result does not fit @c size_t
 */
BROTLI_ENC_API size_t BrotliEncoderMaxCompressedSize(size_t input_size);

/**
 * Performs one-shot memory-to-memory compression.
 *
 * Compresses the data in @p input_buffer into @p encoded_buffer, and sets
 * @p *encoded_size to the compressed length.
 *
 * @note If ::BrotliEncoderMaxCompressedSize(@p input_size) returns non-zero
 *       value, then output is guaranteed to be no longer than that.
 *
 * @note If @p lgwin is greater than ::BROTLI_MAX_WINDOW_BITS then resulting
 *       stream might be incompatible with RFC 7932; to decode such streams,
 *       decoder should be configured with
 *       ::BROTLI_DECODER_PARAM_LARGE_WINDOW = @c 1
 *
 * @param quality quality parameter value, e.g. ::BROTLI_DEFAULT_QUALITY
 * @param lgwin lgwin parameter value, e.g. ::BROTLI_DEFAULT_WINDOW
 * @param mode mode parameter value, e.g. ::BROTLI_DEFAULT_MODE
 * @param input_size size of @p input_buffer
 * @param input_buffer input data buffer with at least @p input_size
 *        addressable bytes
 * @param[in, out] encoded_size @b in: size of @p encoded_buffer; \n
 *                 @b out: length of compressed data written to
 *                 @p encoded_buffer, or @c 0 if compression fails
 * @param encoded_buffer compressed data destination buffer
 * @returns ::BROTLI_FALSE in case of compression error
 * @returns ::BROTLI_FALSE if output buffer is too small
 * @returns ::BROTLI_TRUE otherwise
 */
BROTLI_ENC_API BROTLI_BOOL BrotliEncoderCompress(
    int quality, int lgwin, BrotliEncoderMode mode, size_t input_size,
    const uint8_t input_buffer[BROTLI_ARRAY_PARAM(input_size)],
    size_t* encoded_size,
    uint8_t encoded_buffer[BROTLI_ARRAY_PARAM(*encoded_size)]);

/**
 * Compresses input stream to output stream.
 *
 * The values @p *available_in and @p *available_out must specify the number of
 * bytes addressable at @p *next_in and @p *next_out respectively.
 * When @p *available_out is @c 0, @p next_out is allowed to be @c NULL.
 *
 * After each call, @p *available_in will be decremented by the amount of input
 * bytes consumed, and the @p *next_in pointer will be incremented by that
 * amount. Similarly, @p *available_out will be decremented by the amount of
 * output bytes written, and the @p *next_out pointer will be incremented by
 * that amount.
 *
 * @p total_out, if it is not a null-pointer, will be set to the number
 * of bytes compressed since the last @p state initialization.
 *
 *
 *
 * Internally workflow consists of 3 tasks:
 *  -# (optionally) copy input data to internal buffer
 *  -# actually compress data and (optionally) store it to internal buffer
 *  -# (optionally) copy compressed bytes from internal buffer to output stream
 *
 * Whenever all 3 tasks can't move forward anymore, or error occurs, this
 * method returns the control flow to caller.
 *
 * @p op is used to perform flush, finish the stream, or inject metadata block.
 * See ::BrotliEncoderOperation for more information.
 *
 * Flushing the stream means forcing encoding of all input passed to encoder and
 * completing the current output block, so it could be fully decoded by stream
 * decoder. To perform flush set @p op to ::BROTLI_OPERATION_FLUSH.
 * Under some circumstances (e.g. lack of output stream capacity) this operation
 * would require several calls to ::BrotliEncoderCompressStream. The method must
 * be called again until both input stream is depleted and encoder has no more
 * output (see ::BrotliEncoderHasMoreOutput) after the method is called.
 *
 * Finishing the stream means encoding of all input passed to encoder and
 * adding specific "final" marks, so stream decoder could determine that stream
 * is complete. To perform finish set @p op to ::BROTLI_OPERATION_FINISH.
 * Under some circumstances (e.g. lack of output stream capacity) this operation
 * would require several calls to ::BrotliEncoderCompressStream. The method must
 * be called again until both input stream is depleted and encoder has no more
 * output (see ::BrotliEncoderHasMoreOutput) after the method is called.
 *
 * @warning When flushing and finishing, @p op should not change until operation
 *          is complete; input stream should not be swapped, reduced or
 *          extended as well.
 *
 * @param state encoder instance
 * @param op requested operation
 * @param[in, out] available_in @b in: amount of available input; \n
 *                 @b out: amount of unused input
 * @param[in, out] next_in pointer to the next input byte
 * @param[in, out] available_out @b in: length of output buffer; \n
 *                 @b out: remaining size of output buffer
 * @param[in, out] next_out compressed output buffer cursor;
 *                 can be @c NULL if @p available_out is @c 0
 * @param[out] total_out number of bytes produced so far; can be @c NULL
 * @returns ::BROTLI_FALSE if there was an error
 * @returns ::BROTLI_TRUE otherwise
 */
BROTLI_ENC_API BROTLI_BOOL BrotliEncoderCompressStream(
    BrotliEncoderState* state, BrotliEncoderOperation op, size_t* available_in,
    const uint8_t** next_in, size_t* available_out, uint8_t** next_out,
    size_t* total_out);

/**
 * Checks if encoder instance reached the final state.
 *
 * @param state encoder instance
 * @returns ::BROTLI_TRUE if encoder is in a state where it reached the end of
 *          the input and produced all of the output
 * @returns ::BROTLI_FALSE otherwise
 */
BROTLI_ENC_API BROTLI_BOOL BrotliEncoderIsFinished(BrotliEncoderState* state);

/**
 * Checks if encoder has more output.
 *
 * @param state encoder instance
 * @returns ::BROTLI_TRUE, if encoder has some unconsumed output
 * @returns ::BROTLI_FALSE otherwise
 */
BROTLI_ENC_API BROTLI_BOOL BrotliEncoderHasMoreOutput(
    BrotliEncoderState* state);

/**
 * Acquires pointer to internal output buffer.
 *
 * This method is used to make language bindings easier and more efficient:
 *  -# push data to ::BrotliEncoderCompressStream,
 *     until ::BrotliEncoderHasMoreOutput returns BROTLI_TRUE
 *  -# use ::BrotliEncoderTakeOutput to peek bytes and copy to language-specific
 *     entity
 *
 * Also this could be useful if there is an output stream that is able to
 * consume all the provided data (e.g. when data is saved to file system).
 *
 * @attention After every call to ::BrotliEncoderTakeOutput @p *size bytes of
 *            output are considered consumed for all consecutive calls to the
 *            instance methods; returned pointer becomes invalidated as well.
 *
 * @note Encoder output is not guaranteed to be contiguous. This means that
 *       after the size-unrestricted call to ::BrotliEncoderTakeOutput,
 *       immediate next call to ::BrotliEncoderTakeOutput may return more data.
 *
 * @param state encoder instance
 * @param[in, out] size @b in: number of bytes caller is ready to take, @c 0 if
 *                 any amount could be handled; \n
 *                 @b out: amount of data pointed by returned pointer and
 *                 considered consumed; \n
 *                 out value is never greater than in value, unless it is @c 0
 * @returns pointer to output data
 */
BROTLI_ENC_API const uint8_t* BrotliEncoderTakeOutput(
    BrotliEncoderState* state, size_t* size);

/* Returns the estimated peak memory usage (in bytes) of the BrotliCompress()
   function, not counting the memory needed for the input and output. */
BROTLI_ENC_EXTRA_API size_t BrotliEncoderEstimatePeakMemoryUsage(
    int quality, int lgwin, size_t input_size);
/* Returns 0 if dictionary is not valid; otherwise returns allocation size. */
BROTLI_ENC_EXTRA_API size_t BrotliEncoderGetPreparedDictionarySize(
    const BrotliEncoderPreparedDictionary* dictionary);

/**
 * Gets an encoder library version.
 *
 * Look at BROTLI_MAKE_HEX_VERSION for more information.
 */
BROTLI_ENC_API uint32_t BrotliEncoderVersion(void);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_ENCODE_H_ */
