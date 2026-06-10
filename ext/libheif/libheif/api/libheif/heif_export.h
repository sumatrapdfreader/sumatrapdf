/*
 * HEIF codec.
 * Copyright (c) 2017-2026 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBHEIF_HEIF_EXPORT_H
#define LIBHEIF_HEIF_EXPORT_H

// LIBHEIF_API marks the symbols that are exported from the shared library.
//
// This macro lives in its own tiny, dependency-free header so that every
// public header can pull it in without creating include cycles. In particular,
// heif_library.h and heif_error.h are mutually dependent (heif_library.h needs
// the heif_error type while heif_error.h needs LIBHEIF_API), so the macro
// cannot live in either of them if both are to compile standalone.

#if (defined(_WIN32) || defined __CYGWIN__) && !defined(LIBHEIF_STATIC_BUILD)
#ifdef LIBHEIF_EXPORTS
#define LIBHEIF_API __declspec(dllexport)
#else
#define LIBHEIF_API __declspec(dllimport)
#endif
#elif defined(HAVE_VISIBILITY) && HAVE_VISIBILITY
#ifdef LIBHEIF_EXPORTS
#define LIBHEIF_API __attribute__((__visibility__("default")))
#else
#define LIBHEIF_API
#endif
#else
#define LIBHEIF_API
#endif

#endif  // LIBHEIF_HEIF_EXPORT_H
