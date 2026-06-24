/*-
 * Copyright (c) 2026 Tobias Stoeckmann
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ARCHIVE_INTEGER_H_INCLUDED
#define ARCHIVE_INTEGER_H_INCLUDED

#include "archive_platform.h"

/* Note:  This is a purely internal header! */
/* Do not use this outside of libarchive internal code! */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifdef HAVE_INTSAFE_H
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDCKDINT_H
#include <stdckdint.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifdef HAVE_STDCKDINT_H
#define USE_STDCKDINT 1
#elif (__GNUC__ >= 5 && !defined(__INTEL_COMPILER))
#define USE_BUILTIN 1
#elif __has_builtin(__builtin_add_overflow)
#define USE_BUILTIN 1
#elif defined HAVE_INTSAFE_H
#define USE_INTSAFE 1
#endif

/*
 * Disabling inline keyword for compilers known to choke on it:
 * - Watcom C++ in C code.  (For any version?)
 * - SGI MIPSpro
 * - Microsoft Visual C++ 6.0 (supposedly newer versions too)
 * - IBM VisualAge 6 (XL v6)
 * - Sun WorkShop C (SunPro) before 5.9
 */
#if defined(__WATCOMC__) || defined(__sgi) || defined(__hpux) || defined(__BORLANDC__)
#define	inline
#elif defined(__IBMC__) && __IBMC__ < 700
#define	inline
#elif defined(__SUNPRO_C) && __SUNPRO_C < 0x590
#define inline
#elif defined(_MSC_VER) || defined(__osf__)
#define inline __inline
#endif

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_add_i64(int64_t *result, int64_t a, int64_t b)
{
#if USE_STDCKDINT
	return ckd_add(result, a, b);
#elif USE_BUILTIN
	return __builtin_add_overflow(a, b, result);
#elif USE_INTSAFE
	LONGLONG res;
	int ret;

	ret = LongLongAdd(a, b, &res);
	*result = (int64_t)res;
	return ret;
#else
	if ((b > 0 && a > INT64_MAX - b) ||
	    (b < 0 && a < INT64_MIN - b))
		return 1;

	*result = a + b;
	return 0;
#endif
}

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_add_size(size_t *result, size_t a, size_t b)
{
#if USE_STDCKDINT
	return ckd_add(result, a, b);
#elif USE_BUILTIN
	return __builtin_add_overflow(a, b, result);
#elif USE_INTSAFE
	return SizeTAdd(a, b, result);
#else
	if (a > SIZE_MAX - b)
		return 1;
	*result = a + b;
	return 0;
#endif
}

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_add_u64(uint64_t *result, uint64_t a, uint64_t b)
{
#if USE_STDCKDINT
	return ckd_add(result, a, b);
#elif USE_BUILTIN
	return __builtin_add_overflow(a, b, result);
#elif USE_INTSAFE
	ULONGLONG res;
	int ret;

	ret = ULongLongAdd(a, b, &res);
	*result = (uint64_t)res;
	return ret;
#else
	if (a > UINT64_MAX - b)
		return 1;
	*result = a + b;
	return 0;
#endif
}

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_mul_i64(int64_t *result, int64_t a, int64_t b)
{
#if USE_STDCKDINT
	return ckd_mul(result, a, b);
#elif USE_BUILTIN
	return __builtin_mul_overflow(a, b, result);
#elif USE_INTSAFE
	LONGLONG res;
	int ret;

	ret = LongLongMult(a, b, &res);
	*result = (int64_t)res;
	return ret;
#else
	if ((a > 0 && b > 0 && a > INT64_MAX / b) ||
	    (a < 0 && b > 0 && a < INT64_MIN / b) ||
	    (a > 0 && b < 0 && b < INT64_MIN / a) ||
	    (a < 0 && b < 0 && a < INT64_MAX / b))
		return 1;

	*result = a * b;
	return 0;
#endif
}

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_mul_size(size_t *result, size_t a, size_t b)
{
#if USE_STDCKDINT
	return ckd_mul(result, a, b);
#elif USE_BUILTIN
	return __builtin_mul_overflow(a, b, result);
#elif USE_INTSAFE
	return SizeTMult(a, b, result);
#else
	if (b != 0 && a > SIZE_MAX / b)
		return 1;
	*result = a * b;
	return 0;
#endif
}

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_mul_u64(uint64_t *result, uint64_t a, uint64_t b)
{
#if USE_STDCKDINT
	return ckd_mul(result, a, b);
#elif USE_BUILTIN
	return __builtin_mul_overflow(a, b, result);
#elif USE_INTSAFE
	ULONGLONG res;
	int ret;

	ret = ULongLongMult(a, b, &res);
	*result = (uint64_t)res;
	return ret;
#else
	if (b != 0 && a > UINT64_MAX / b)
		return 1;
	*result = a * b;
	return 0;
#endif
}

/* Returns 0 on success, a non-zero value otherwise. */
static inline int
archive_ckd_sub_i64(int64_t *result, int64_t a, int64_t b)
{
#if USE_STDCKDINT
	return ckd_sub(result, a, b);
#elif USE_BUILTIN
	return __builtin_sub_overflow(a, b, result);
#elif USE_INTSAFE
	LONGLONG res;
	int ret;

	ret = LongLongSub(a, b, &res);
	*result = (int64_t)res;
	return ret;
#else
	if ((b > 0 && a < INT64_MIN + b) ||
	    (b < 0 && a > INT64_MAX + b))
		return 1;

	*result = a - b;
	return 0;
#endif
}

#endif
