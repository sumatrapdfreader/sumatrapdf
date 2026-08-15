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

package com.artifex.mupdf.fitz;

public final class ColorParams
{
	public enum RenderingIntent {
		PERCEPTUAL,
		RELATIVE_COLORIMETRIC,
		SATURATION,
		ABSOLUTE_COLORIMETRIC
	}
	public static final int BP = 32;
	public static final int OP = 64;
	public static final int OPM = 128;

	public static RenderingIntent RI(int flags) {
		switch (flags & 3) {
			default:
			case 0: return RenderingIntent.PERCEPTUAL;
			case 1: return RenderingIntent.RELATIVE_COLORIMETRIC;
			case 2: return RenderingIntent.SATURATION;
			case 3: return RenderingIntent.ABSOLUTE_COLORIMETRIC;
		}
	}

	public static boolean BP(int flags) {
		return (flags & BP) != 0;
	}

	public static boolean OP(int flags) {
		return (flags & OP) != 0;
	}

	public static boolean OPM(int flags) {
		return (flags & OPM) != 0;
	}

	public static int pack(RenderingIntent ri, boolean bp, boolean op, boolean opm) {
		int flags;
		switch (ri) {
		default:
		case PERCEPTUAL: flags = 0; break;
		case RELATIVE_COLORIMETRIC: flags = 1; break;
		case SATURATION: flags = 2; break;
		case ABSOLUTE_COLORIMETRIC: flags = 3; break;
		}
		if (bp) flags |= BP;
		if (op) flags |= OP;
		if (opm) flags |= OPM;
		return flags;
	}
}
