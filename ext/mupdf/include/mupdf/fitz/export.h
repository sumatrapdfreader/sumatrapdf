// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_EXPORT_H
#define MUPDF_FITZ_EXPORT_H

/*
 * Support for building/using MuPDF DLL on Windows.
 *
 * When compiling code that uses MuPDF DLL, FZ_DLL_CLIENT should be defined.
 *
 * When compiling MuPDF DLL itself, FZ_DLL should be defined.
 */

#if defined(_WIN32) || defined(_WIN64)
	#if defined(FZ_DLL)
		/* Building DLL. */
		#define FZ_FUNCTION __declspec(dllexport)
		#define FZ_DATA __declspec(dllexport)
	#elif defined(FZ_DLL_CLIENT)
		/* Building DLL client code. */
		#define FZ_FUNCTION __declspec(dllexport)
		#define FZ_DATA __declspec(dllimport)
	#else
		#define FZ_FUNCTION
		#define FZ_DATA
	#endif
#else
	#define FZ_FUNCTION
	#define FZ_DATA
#endif

#endif
