/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/lib/libarchive/archive_platform.h 201090 2009-12-28 02:22:04Z kientzle $
 */

/* !!ONLY FOR USE INTERNALLY TO LIBARCHIVE!! */

/*
 * This header is the first thing included in any of the libarchive
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.  I'm
 * actively trying to minimize #if blocks within the main source,
 * since they obfuscate the code.
 */

#ifndef ARCHIVE_PLATFORM_H_INCLUDED
#define	ARCHIVE_PLATFORM_H_INCLUDED

/* archive.h and archive_entry.h require this. */
#define	__LIBARCHIVE_BUILD 1

#if defined(PLATFORM_CONFIG_H)
/* Use hand-built config.h in environments that need it. */
#include PLATFORM_CONFIG_H
#elif defined(HAVE_CONFIG_H)
/* Most POSIX platforms use the 'configure' script to build config.h */
#include "config.h"
#else
/* Warn if the library hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no pre-built configuration in archive_platform.h.
#endif

/* It should be possible to get rid of this by extending the feature-test
 * macros to cover Windows API functions, probably along with non-trivial
 * refactoring of code to find structures that sit more cleanly on top of
 * either Windows or Posix APIs. */
#if (defined(__WIN32__) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include "archive_windows.h"
#endif

/*
 * The config files define a lot of feature macros.  The following
 * uses those macros to select/define replacements and include key
 * headers as required.
 */

/* Get a real definition for __FBSDID if we can */
#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

/* If not, define it so as to avoid dangling semicolons. */
#ifndef __FBSDID
#define	__FBSDID(a)     struct _undefined_hack
#endif

/* Try to get standard C99-style integer type definitions. */
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif

/* Borland warns about its own constants!  */
#if defined(__BORLANDC__)
# if HAVE_DECL_UINT64_MAX
#  undef	UINT64_MAX
#  undef	HAVE_DECL_UINT64_MAX
# endif
# if HAVE_DECL_UINT64_MIN
#  undef	UINT64_MIN
#  undef	HAVE_DECL_UINT64_MIN
# endif
# if HAVE_DECL_INT64_MAX
#  undef	INT64_MAX
#  undef	HAVE_DECL_INT64_MAX
# endif
# if HAVE_DECL_INT64_MIN
#  undef	INT64_MIN
#  undef	HAVE_DECL_INT64_MIN
# endif
#endif

/* Some platforms lack the standard *_MAX definitions. */
#if !HAVE_DECL_SIZE_MAX
#define	SIZE_MAX (~(size_t)0)
#endif
#if !HAVE_DECL_SSIZE_MAX
#define	SSIZE_MAX ((ssize_t)(SIZE_MAX >> 1))
#endif
#if !HAVE_DECL_UINT32_MAX
#define	UINT32_MAX (~(uint32_t)0)
#endif
#if !HAVE_DECL_UINT64_MAX
#define	UINT64_MAX (~(uint64_t)0)
#endif
#if !HAVE_DECL_INT64_MAX
#define	INT64_MAX ((int64_t)(UINT64_MAX >> 1))
#endif
#if !HAVE_DECL_INT64_MIN
#define	INT64_MIN ((int64_t)(~INT64_MAX))
#endif

/*
 * If this platform has <sys/acl.h>, acl_create(), acl_init(),
 * acl_set_file(), and ACL_USER, we assume it has the rest of the
 * POSIX.1e draft functions used in archive_read_extract.c.
 */
#if HAVE_SYS_ACL_H && HAVE_ACL_CREATE_ENTRY && HAVE_ACL_INIT && HAVE_ACL_SET_FILE && HAVE_ACL_USER
#define	HAVE_POSIX_ACL	1
#endif

/*
 * If we can't restore metadata using a file descriptor, then
 * for compatibility's sake, close files before trying to restore metadata.
 */
#if defined(HAVE_FCHMOD) || defined(HAVE_FUTIMES) || defined(HAVE_ACL_SET_FD) || defined(HAVE_ACL_SET_FD_NP) || defined(HAVE_FCHOWN)
#define	CAN_RESTORE_METADATA_FD
#endif

/* Set up defaults for internal error codes. */
#ifndef ARCHIVE_ERRNO_FILE_FORMAT
#if HAVE_EFTYPE
#define	ARCHIVE_ERRNO_FILE_FORMAT EFTYPE
#else
#if HAVE_EILSEQ
#define	ARCHIVE_ERRNO_FILE_FORMAT EILSEQ
#else
#define	ARCHIVE_ERRNO_FILE_FORMAT EINVAL
#endif
#endif
#endif

#ifndef ARCHIVE_ERRNO_PROGRAMMER
#define	ARCHIVE_ERRNO_PROGRAMMER EINVAL
#endif

#ifndef ARCHIVE_ERRNO_MISC
#define	ARCHIVE_ERRNO_MISC (-1)
#endif

#endif /* !ARCHIVE_PLATFORM_H_INCLUDED */
