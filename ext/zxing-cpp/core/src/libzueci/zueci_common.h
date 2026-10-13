/*  zueci_common.h - shared defines */
/*
    libzueci - an open source UTF-8 ECI library adapted from libzint
    Copyright (C) 2022 gitlost
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef ZUECI_COMMON_H
#define ZUECI_COMMON_H

#define ZUECI_ASIZE(x) ((int) (sizeof(x) / sizeof((x)[0])))
#define ZUECI_MIN(x, y) (x < y ? x : y)

#if (defined(__GNUC__) || defined(__clang__)) && !defined(ZUECI_TEST) && !defined(__MINGW32__)
#  define ZUECI_INTERN __attribute__ ((visibility ("hidden")))
#elif defined(ZUECI_TEST)
   /* The test suite references ZUECI_INTERN functions, so they need to be exported */
#  define ZUECI_INTERN ZUECI_EXTERN
#else
#  define ZUECI_INTERN
#endif

typedef unsigned short zueci_u16; /* `unsigned short` guaranteed to be at least 16 bits */
#ifndef ZUECI_U32_TYPE /* On the off chance it needs to be `unsigned long` */
typedef unsigned int zueci_u32;
#endif

typedef char zueci_static_assert_u32_at_least_32bits[sizeof(zueci_u32) < 4 ? -1 : 1];

/* vim: set ts=4 sw=4 et : */
#endif /* ZUECI_COMMON_H */
